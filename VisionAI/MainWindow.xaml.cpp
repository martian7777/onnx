#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <fstream>
#include <array>
#include <chrono>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::Graphics::Canvas;
using namespace winrt::Microsoft::Graphics::Canvas::UI::Xaml;

// Raw COM interface to read a SoftwareBitmap's CPU buffer.
struct __declspec(uuid("5B0D3235-4DBA-4D44-865E-8F1D0E4FD04D")) __declspec(novtable)
IMemoryBufferByteAccess : ::IUnknown
{
    virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
};

namespace {

// Distinct overlay colors cycled by class id.
const std::array<winrt::Windows::UI::Color, 10> kPalette{ {
    {255,  46, 204, 113}, {255, 231,  76,  60}, {255,  52, 152, 219},
    {255, 241, 196,  15}, {255, 155,  89, 182}, {255,  26, 188, 156},
    {255, 230, 126,  34}, {255,  52,  73,  94}, {255, 236, 240, 241},
    {255, 149, 165, 166},
} };

std::wstring AssetPath(std::wstring_view name)
{
    try {
        auto p = winrt::Windows::ApplicationModel::Package::Current().InstalledLocation().Path();
        return std::wstring(p) + L"\\Assets\\" + std::wstring(name);
    } catch (...) {
        wchar_t buf[MAX_PATH]{};
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        std::wstring dir(buf);
        dir = dir.substr(0, dir.find_last_of(L"\\/"));
        return dir + L"\\Assets\\" + std::wstring(name);
    }
}

std::vector<std::string> LoadLabels(const std::wstring& path)
{
    std::vector<std::string> names;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) names.push_back(line);
    }
    return names;
}

} // namespace

namespace winrt::VisionAI::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        Title(L"Local Vision AI Dashboard");
        dispatcher_ = Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
        labels_ = LoadLabels(AssetPath(L"coco.names"));
    }

    MainWindow::~MainWindow()
    {
        Stop();
    }

    void MainWindow::OnStartStopClick(IInspectable const&, RoutedEventArgs const&)
    {
        if (StartStopButton().IsChecked().GetBoolean()) {
            StartStopButton().Content(box_value(L"Stop"));
            Start();
        } else {
            StartStopButton().Content(box_value(L"Start"));
            Stop();
        }
    }

    void MainWindow::OnConfidenceChanged(
        IInspectable const&,
        Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
    {
        confThreshold_.store(static_cast<float>(e.NewValue() / 100.0));
    }

    void MainWindow::Start()
    {
        if (running_.exchange(true)) return;
        StatusText().Text(L"Starting camera…");

        source_.OnFrame = [this]() {
            if (dispatcher_) {
                dispatcher_.TryEnqueue([this]() {
                    if (VideoCanvas()) VideoCanvas().Invalidate();
                });
            }
        };

        worker_ = std::thread([this]() { WorkerLoop(); });

        // Fire-and-forget camera start with error surfacing.
        [](MainWindow* self) -> fire_and_forget {
            auto lifetime = self->get_strong();
            try {
                co_await self->source_.StartCameraAsync();
            } catch (hresult_error const& ex) {
                auto msg = ex.message();
                self->dispatcher_.TryEnqueue([self, msg]() {
                    self->StatusText().Text(L"Camera error: " + msg);
                });
            }
        }(this);
    }

    void MainWindow::Stop()
    {
        if (!running_.exchange(false)) return;
        StatusText().Text(L"Stopping…");
        source_.Stop();
        if (worker_.joinable()) worker_.join();
        {
            std::lock_guard<std::mutex> lk(detMutex_);
            detections_.clear();
        }
        if (VideoCanvas()) VideoCanvas().Invalidate();
        StatusText().Text(L"Idle");
    }

    void MainWindow::WorkerLoop()
    {
        // Lazily create + load the detector on this background thread so the
        // (potentially slow) DirectML init never blocks the UI thread.
        if (!detector_) {
            detector_ = std::make_unique<vai::YoloDetector>();
            vai::YoloDetector::Options opts;
            opts.modelPath    = AssetPath(L"yolov8n.onnx");
            opts.confThreshold = confThreshold_.load();
            opts.useDirectML  = true;
            bool ok = detector_->Load(opts);

            std::wstring hw = detector_->HardwareTarget();
            std::string  err = detector_->LastError();
            stats_.SetHardwareTarget(hw);
            dispatcher_.TryEnqueue([this, hw, ok, err]() {
                HardwareValue().Text(hw);
                StatusText().Text(ok ? L"Running" : L"Model load failed");
            });
            if (!ok) { running_ = false; return; }
        }

        uint64_t lastId = 0;
        while (running_.load()) {
            const uint64_t id = source_.FrameId();
            if (id == lastId) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            SoftwareBitmap bmp = source_.LatestFrame();
            if (!bmp) { std::this_thread::sleep_for(std::chrono::milliseconds(2)); continue; }
            lastId = id;

            detector_->SetConfidence(confThreshold_.load());

            BitmapBuffer buffer = bmp.LockBuffer(BitmapBufferAccessMode::Read);
            auto reference = buffer.CreateReference();
            uint8_t* data = nullptr; uint32_t capacity = 0;
            auto access = reference.as<IMemoryBufferByteAccess>();
            if (FAILED(access->GetBuffer(&data, &capacity)) || !data) continue;

            auto desc = buffer.GetPlaneDescription(0);
            double ms = 0;
            auto dets = detector_->Run(data, desc.Width, desc.Height, desc.Stride,
                                       vai::PixelFormat::Bgra8, &ms);

            reference.Close();
            buffer.Close();

            {
                std::lock_guard<std::mutex> lk(detMutex_);
                detections_ = std::move(dets);
                frameW_ = desc.Width;
                frameH_ = desc.Height;
            }
            stats_.ReportInference(ms);
        }
    }

    void MainWindow::OnCanvasDraw(CanvasControl const& sender, CanvasDrawEventArgs const& args)
    {
        stats_.TickFrame();
        auto ds = args.DrawingSession();

        SoftwareBitmap bmp = source_.LatestFrame();
        const float cw = sender.Size().Width;
        const float ch = sender.Size().Height;

        std::vector<vai::Detection> dets;
        int fw = 0, fh = 0;
        {
            std::lock_guard<std::mutex> lk(detMutex_);
            dets = detections_;
            fw = frameW_; fh = frameH_;
        }

        if (bmp) {
            auto cbmp = CanvasBitmap::CreateFromSoftwareBitmap(sender, bmp);
            const float iw = static_cast<float>(bmp.PixelWidth());
            const float ih = static_cast<float>(bmp.PixelHeight());
            const float scale = std::min(cw / iw, ch / ih);
            const float dw = iw * scale, dh = ih * scale;
            const float ox = (cw - dw) * 0.5f, oy = (ch - dh) * 0.5f;

            ds.DrawImage(cbmp, Rect{ ox, oy, dw, dh });

            // Overlay uses the frame size the detections were computed against.
            const float sx = (fw > 0) ? dw / fw : scale;
            const float sy = (fh > 0) ? dh / fh : scale;

            auto fmt = Microsoft::Graphics::Canvas::Text::CanvasTextFormat();
            fmt.FontSize(14.0f);

            for (auto const& d : dets) {
                auto color = kPalette[d.classId % kPalette.size()];
                const float rx = ox + d.x * sx;
                const float ry = oy + d.y * sy;
                const float rw = d.w * sx;
                const float rh = d.h * sy;
                ds.DrawRectangle(Rect{ rx, ry, rw, rh }, color, 2.5f);

                const char* nm = (d.classId >= 0 && d.classId < (int)labels_.size())
                                     ? labels_[d.classId].c_str() : "?";
                std::wstring label(nm, nm + strlen(nm));
                wchar_t buf[64];
                swprintf_s(buf, L"%s %d%%", label.c_str(), (int)(d.score * 100));

                ds.FillRectangle(Rect{ rx, ry - 20, 20 + label.size() * 9.0f, 20 }, color);
                ds.DrawText(buf, float2{ rx + 4, ry - 19 }, winrt::Windows::UI::Color{255,0,0,0}, fmt);
            }
        } else {
            auto fmt = Microsoft::Graphics::Canvas::Text::CanvasTextFormat();
            fmt.FontSize(18.0f);
            ds.DrawText(L"Waiting for frames…", float2{ 24, 24 },
                        winrt::Windows::UI::Color{ 200, 200, 200, 200 }, fmt);
        }

        UpdateTelemetry();
    }

    void MainWindow::UpdateTelemetry()
    {
        auto s = stats_.Get();
        wchar_t buf[128];

        swprintf_s(buf, L"%.1f", s.fps);
        FpsValue().Text(buf);

        swprintf_s(buf, L"%.1f ms", s.inferenceMs);
        LatencyValue().Text(buf);

        swprintf_s(buf, L"min %.1f / max %.1f ms", s.inferenceMsMin, s.inferenceMsMax);
        LatencyRange().Text(buf);

        size_t n = 0;
        { std::lock_guard<std::mutex> lk(detMutex_); n = detections_.size(); }
        swprintf_s(buf, L"%zu detected", n);
        DetectionCount().Text(buf);
    }
}
