#pragma once
#include "Detection.h"
#include <onnxruntime_cxx_api.h>
#include <string>
#include <vector>
#include <memory>

namespace vai {

// YOLOv8/YOLOv11 detector running on ONNX Runtime with the DirectML EP.
// The class is deliberately free of any WinRT / XAML dependency so it can be
// exercised from a plain console test and reused inside the WinUI 3 app.
class YoloDetector {
public:
    struct Options {
        std::wstring modelPath;          // path to yolov8n.onnx
        float        confThreshold = 0.25f;
        float        iouThreshold  = 0.45f;
        int          inputSize     = 640; // square letterbox target
        bool         useDirectML   = true;
        int          deviceId      = 0;   // DML/DXGI adapter index
    };

    YoloDetector();
    ~YoloDetector();

    // Creates the ORT session. Returns false on failure (message in LastError()).
    bool Load(const Options& opts);

    // Runs detection on one frame. Coordinates in the returned boxes are in the
    // ORIGINAL frame's pixel space. `inferMs` (optional) receives the time spent
    // inside Session::Run in milliseconds.
    std::vector<Detection> Run(const uint8_t* pixels, int width, int height,
                               int strideBytes, PixelFormat fmt,
                               double* inferMs = nullptr);

    // Human-readable execution target, e.g. "DirectML – NVIDIA GeForce RTX 4060"
    // or "CPU". Valid after Load().
    const std::wstring& HardwareTarget() const { return hardwareTarget_; }
    bool  UsingDirectML() const { return usingDirectML_; }
    const std::string& LastError() const { return lastError_; }

private:
    // Letterbox parameters produced by preprocessing, needed to un-map boxes.
    struct Letterbox { float scale; float padX; float padY; };

    Letterbox Preprocess(const uint8_t* pixels, int width, int height,
                         int strideBytes, PixelFormat fmt, std::vector<float>& outChw) const;

    std::vector<Detection> Postprocess(const float* out, size_t attrs, size_t anchors,
                                       const Letterbox& lb, int srcW, int srcH,
                                       float confThreshold) const;

    static std::wstring QueryAdapterName(int deviceId);

    Options                      opts_{};
    Ort::Env                     env_;
    Ort::SessionOptions          sessionOptions_;
    std::unique_ptr<Ort::Session> session_;
    Ort::MemoryInfo              memInfo_{nullptr};

    std::string                  inputName_;
    std::string                  outputName_;
    std::wstring                 hardwareTarget_ = L"CPU";
    bool                         usingDirectML_  = false;
    std::string                  lastError_;
};

} // namespace vai
