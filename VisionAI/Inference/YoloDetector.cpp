#include "YoloDetector.h"
#include "Nms.h"

#include <dml_provider_factory.h>   // OrtSessionOptionsAppendExecutionProvider_DML

#include <dxgi1_4.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <chrono>

#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

namespace vai {

YoloDetector::YoloDetector()
    : env_(ORT_LOGGING_LEVEL_WARNING, "VisionAI") {}

YoloDetector::~YoloDetector() = default;

std::wstring YoloDetector::QueryAdapterName(int deviceId) {
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return L"Unknown GPU";
    ComPtr<IDXGIAdapter1> adapter;
    if (factory->EnumAdapters1(static_cast<UINT>(deviceId), &adapter) == DXGI_ERROR_NOT_FOUND)
        return L"Unknown GPU";
    DXGI_ADAPTER_DESC1 desc{};
    if (FAILED(adapter->GetDesc1(&desc))) return L"Unknown GPU";
    return std::wstring(desc.Description);
}

bool YoloDetector::Load(const Options& opts) {
    opts_ = opts;
    try {
        sessionOptions_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        if (opts_.useDirectML) {
            // DirectML EP requirements: sequential execution + no mem pattern.
            sessionOptions_.DisableMemPattern();
            sessionOptions_.SetExecutionMode(ORT_SEQUENTIAL);
            OrtStatus* st = OrtSessionOptionsAppendExecutionProvider_DML(
                sessionOptions_, opts_.deviceId);
            if (st != nullptr) {
                std::string msg = Ort::GetApi().GetErrorMessage(st);
                Ort::GetApi().ReleaseStatus(st);
                throw std::runtime_error("DML EP unavailable: " + msg);
            }
            usingDirectML_  = true;
            hardwareTarget_ = L"DirectML – " + QueryAdapterName(opts_.deviceId);
        } else {
            usingDirectML_  = false;
            hardwareTarget_ = L"CPU";
        }

        session_ = std::make_unique<Ort::Session>(env_, opts_.modelPath.c_str(), sessionOptions_);

        Ort::AllocatorWithDefaultOptions alloc;
        inputName_  = session_->GetInputNameAllocated(0, alloc).get();
        outputName_ = session_->GetOutputNameAllocated(0, alloc).get();

        memInfo_ = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        return true;
    } catch (const std::exception& e) {
        lastError_ = e.what();
        // Fall back to CPU once if DML init failed.
        if (opts_.useDirectML) {
            Options cpuOpts = opts_;
            cpuOpts.useDirectML = false;
            sessionOptions_ = Ort::SessionOptions{};
            return Load(cpuOpts);
        }
        return false;
    }
}

YoloDetector::Letterbox YoloDetector::Preprocess(const uint8_t* pixels, int width, int height,
                                                 int strideBytes, PixelFormat fmt,
                                                 std::vector<float>& outChw) const {
    const int   S     = opts_.inputSize;
    const float scale = std::min(static_cast<float>(S) / width,
                                 static_cast<float>(S) / height);
    const int   newW  = static_cast<int>(std::round(width  * scale));
    const int   newH  = static_cast<int>(std::round(height * scale));
    const float padX  = (S - newW) / 2.0f;
    const float padY  = (S - newH) / 2.0f;

    outChw.assign(static_cast<size_t>(3) * S * S, 114.0f / 255.0f); // gray letterbox pad
    const size_t plane = static_cast<size_t>(S) * S;
    const int    bpp   = (fmt == PixelFormat::Bgra8) ? 4 : 3;

    // Nearest-neighbour resize directly into the normalized planar RGB buffer.
    for (int dy = 0; dy < newH; ++dy) {
        const int   sy = std::min(height - 1, static_cast<int>(dy / scale));
        const int   ry = static_cast<int>(padY) + dy;
        if (ry < 0 || ry >= S) continue;
        const uint8_t* srcRow = pixels + static_cast<size_t>(sy) * strideBytes;
        for (int dx = 0; dx < newW; ++dx) {
            const int sx = std::min(width - 1, static_cast<int>(dx / scale));
            const int rx = static_cast<int>(padX) + dx;
            if (rx < 0 || rx >= S) continue;
            const uint8_t* p = srcRow + static_cast<size_t>(sx) * bpp;
            float r, g, b;
            if (fmt == PixelFormat::Bgra8) { b = p[0]; g = p[1]; r = p[2]; }
            else                           { r = p[0]; g = p[1]; b = p[2]; }
            const size_t idx = static_cast<size_t>(ry) * S + rx;
            outChw[0 * plane + idx] = r / 255.0f;
            outChw[1 * plane + idx] = g / 255.0f;
            outChw[2 * plane + idx] = b / 255.0f;
        }
    }
    return { scale, padX, padY };
}

std::vector<Detection> YoloDetector::Postprocess(const float* out, size_t attrs, size_t anchors,
                                                 const Letterbox& lb, int srcW, int srcH,
                                                 float confThreshold) const {
    // YOLOv8/v11 output is [1, attrs, anchors] = [1, 4 + numClasses, anchors].
    // Channel-major: value(c, a) = out[c * anchors + a].
    const size_t numClasses = attrs - 4;
    std::vector<Detection> raw;
    raw.reserve(256);

    for (size_t a = 0; a < anchors; ++a) {
        // Best class for this anchor.
        int   bestCls = -1;
        float bestScore = 0.0f;
        for (size_t c = 0; c < numClasses; ++c) {
            const float s = out[(4 + c) * anchors + a];
            if (s > bestScore) { bestScore = s; bestCls = static_cast<int>(c); }
        }
        if (bestScore < confThreshold || bestCls < 0) continue;

        const float cx = out[0 * anchors + a];
        const float cy = out[1 * anchors + a];
        const float w  = out[2 * anchors + a];
        const float h  = out[3 * anchors + a];

        // Un-letterbox back to original frame pixels.
        float x0 = (cx - w * 0.5f - lb.padX) / lb.scale;
        float y0 = (cy - h * 0.5f - lb.padY) / lb.scale;
        float bw = w / lb.scale;
        float bh = h / lb.scale;

        // Clamp to frame.
        x0 = std::clamp(x0, 0.0f, static_cast<float>(srcW));
        y0 = std::clamp(y0, 0.0f, static_cast<float>(srcH));
        bw = std::min(bw, srcW - x0);
        bh = std::min(bh, srcH - y0);
        if (bw <= 1.0f || bh <= 1.0f) continue;

        raw.push_back(Detection{ x0, y0, bw, bh, bestCls, bestScore });
    }
    return NonMaxSuppression(std::move(raw), opts_.iouThreshold);
}

std::vector<Detection> YoloDetector::Run(const uint8_t* pixels, int width, int height,
                                         int strideBytes, PixelFormat fmt, double* inferMs) {
    if (!session_) return {};

    const int S = opts_.inputSize;
    std::vector<float> input;
    const Letterbox lb = Preprocess(pixels, width, height, strideBytes, fmt, input);

    const std::array<int64_t, 4> inShape{ 1, 3, S, S };
    Ort::Value inTensor = Ort::Value::CreateTensor<float>(
        memInfo_, input.data(), input.size(), inShape.data(), inShape.size());

    const char* inNames[]  = { inputName_.c_str() };
    const char* outNames[] = { outputName_.c_str() };

    const auto t0 = std::chrono::high_resolution_clock::now();
    auto outputs = session_->Run(Ort::RunOptions{ nullptr }, inNames, &inTensor, 1, outNames, 1);
    const auto t1 = std::chrono::high_resolution_clock::now();
    if (inferMs) *inferMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    const auto info    = outputs[0].GetTensorTypeAndShapeInfo();
    const auto shape   = info.GetShape();           // expect {1, attrs, anchors}
    const size_t attrs   = shape.size() >= 2 ? static_cast<size_t>(shape[shape.size() - 2]) : 0;
    const size_t anchors = shape.size() >= 1 ? static_cast<size_t>(shape[shape.size() - 1]) : 0;
    const float* out = outputs[0].GetTensorData<float>();

    return Postprocess(out, attrs, anchors, lb, width, height, opts_.confThreshold);
}

} // namespace vai
