#pragma once
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Capture.h>
#include <winrt/Windows.Media.Capture.Frames.h>
#include <winrt/Windows.Graphics.Imaging.h>

#include <functional>
#include <mutex>

namespace vai {

// Live frame producer. Wraps MediaCapture + MediaFrameReader for the webcam.
// Only the most recent frame is retained (stale frames are dropped) so a slow
// consumer never builds a backlog. All public methods are thread-safe.
class FrameSource {
public:
    // Fired on a background thread whenever a fresh frame is available.
    std::function<void()> OnFrame;

    FrameSource() = default;
    ~FrameSource();

    // Starts the default color camera at (or near) the requested resolution.
    winrt::Windows::Foundation::IAsyncAction StartCameraAsync();
    void Stop();

    // Returns a copy handle to the latest Bgra8 SoftwareBitmap, or nullptr.
    winrt::Windows::Graphics::Imaging::SoftwareBitmap LatestFrame();

private:
    void OnFrameArrived(
        winrt::Windows::Media::Capture::Frames::MediaFrameReader const& sender,
        winrt::Windows::Media::Capture::Frames::MediaFrameArrivedEventArgs const& args);

    winrt::Windows::Media::Capture::MediaCapture              capture_{ nullptr };
    winrt::Windows::Media::Capture::Frames::MediaFrameReader  reader_{ nullptr };
    winrt::event_token                                        frameToken_{};

    std::mutex                                                mtx_;
    winrt::Windows::Graphics::Imaging::SoftwareBitmap         latest_{ nullptr };
    bool                                                      running_ = false;
};

} // namespace vai
