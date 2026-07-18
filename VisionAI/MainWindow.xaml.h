#pragma once
#include "MainWindow.xaml.g.h"

#include "Inference/YoloDetector.h"
#include "Inference/Detection.h"
#include "Capture/FrameSource.h"
#include "Telemetry/Stats.h"

#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <memory>

namespace winrt::VisionAI::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        ~MainWindow();

        void OnStartStopClick(Windows::Foundation::IInspectable const& sender,
                              Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnConfidenceChanged(Windows::Foundation::IInspectable const& sender,
                                 Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& e);
        void OnCanvasDraw(Microsoft::Graphics::Canvas::UI::Xaml::CanvasControl const& sender,
                          Microsoft::Graphics::Canvas::UI::Xaml::CanvasDrawEventArgs const& args);

    private:
        void Start();
        void Stop();
        void WorkerLoop();       // background inference thread
        void UpdateTelemetry();  // UI thread only

        std::unique_ptr<vai::YoloDetector> detector_;
        vai::FrameSource                   source_;
        vai::Stats                         stats_;

        Microsoft::UI::Dispatching::DispatcherQueue dispatcher_{ nullptr };

        std::thread                        worker_;
        std::atomic<bool>                  running_{ false };
        std::atomic<float>                 confThreshold_{ 0.25f };

        std::mutex                         detMutex_;
        std::vector<vai::Detection>        detections_;
        int                                frameW_ = 0;
        int                                frameH_ = 0;

        std::vector<std::string>           labels_;
    };
}

namespace winrt::VisionAI::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {};
}
