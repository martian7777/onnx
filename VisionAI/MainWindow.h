#pragma once

#include "Inference/YoloDetector.h"
#include "Inference/Detection.h"
#include "Capture/FrameSource.h"
#include "Telemetry/Stats.h"

#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <memory>
#include <string>

namespace vai {

// Pure-code WinUI 3 main window (no XAML markup, no Win2D). Renders the video
// via an Image + SoftwareBitmapSource and draws detection boxes as Canvas shapes.
class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    void Activate();

private:
    void BuildUi();
    void OnStartStop();
    void Start();
    void Stop();
    void WorkerLoop();
    void RenderFrame();      // UI thread: push frame + reposition overlays + telemetry
    void UpdateTelemetry();

    // UI
    winrt::Microsoft::UI::Xaml::Window                              window_{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::Image                    image_{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::Canvas                   overlay_{ nullptr };
    winrt::Microsoft::UI::Xaml::Media::Imaging::SoftwareBitmapSource imageSource_{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::Primitives::ToggleButton  startBtn_{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::Slider                    confSlider_{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::TextBlock                 statusText_{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::TextBlock                 fpsText_{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::TextBlock                 latencyText_{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::TextBlock                 latencyRange_{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::TextBlock                 hwText_{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::TextBlock                 detText_{ nullptr };
    winrt::Microsoft::UI::Dispatching::DispatcherQueue             dispatcher_{ nullptr };

    std::atomic<bool>              renderQueued_{ false };
    std::atomic<bool>              imageBusy_{ false };

    // Pipeline
    std::unique_ptr<YoloDetector>  detector_;
    FrameSource                    source_;
    Stats                          stats_;

    std::thread                    worker_;
    std::atomic<bool>              running_{ false };
    std::atomic<float>             confThreshold_{ 0.25f };

    std::mutex                     detMutex_;
    std::vector<Detection>         detections_;
    int                            frameW_ = 0;
    int                            frameH_ = 0;

    std::vector<std::string>       labels_;
};

} // namespace vai
