#include "pch.h"
#include "FrameSource.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Capture;
using namespace winrt::Windows::Media::Capture::Frames;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Media::MediaProperties;

namespace vai {

FrameSource::~FrameSource() {
    Stop();
}

IAsyncAction FrameSource::StartCameraAsync() {
    // Pick a color frame source group.
    auto groups = co_await MediaFrameSourceGroup::FindAllAsync();
    MediaFrameSourceGroup selectedGroup{ nullptr };
    MediaFrameSourceInfo  selectedInfo{ nullptr };

    for (auto const& group : groups) {
        for (auto const& info : group.SourceInfos()) {
            if (info.SourceKind() == MediaFrameSourceKind::Color) {
                selectedGroup = group;
                selectedInfo  = info;
                break;
            }
        }
        if (selectedGroup) break;
    }
    if (!selectedGroup) co_return;

    capture_ = MediaCapture();
    MediaCaptureInitializationSettings settings;
    settings.SourceGroup(selectedGroup);
    settings.SharingMode(MediaCaptureSharingMode::ExclusiveControl);
    settings.MemoryPreference(MediaCaptureMemoryPreference::Cpu);   // CPU-readable frames
    settings.StreamingCaptureMode(StreamingCaptureMode::Video);
    co_await capture_.InitializeAsync(settings);

    auto source = capture_.FrameSources().Lookup(selectedInfo.Id());

    // Prefer a moderate resolution to keep preprocessing cheap.
    reader_ = co_await capture_.CreateFrameReaderAsync(source, MediaEncodingSubtypes::Bgra8());
    frameToken_ = reader_.FrameArrived({ this, &FrameSource::OnFrameArrived });

    running_ = true;
    co_await reader_.StartAsync();
}

void FrameSource::OnFrameArrived(MediaFrameReader const& sender,
                                 MediaFrameArrivedEventArgs const&) {
    auto frame = sender.TryAcquireLatestFrame();
    if (!frame) return;
    auto vmf = frame.VideoMediaFrame();
    if (!vmf) return;

    SoftwareBitmap bmp = vmf.SoftwareBitmap();
    if (!bmp) return;

    // Normalize to Bgra8 / premultiplied for consistent downstream handling.
    if (bmp.BitmapPixelFormat() != BitmapPixelFormat::Bgra8) {
        bmp = SoftwareBitmap::Convert(bmp, BitmapPixelFormat::Bgra8, BitmapAlphaMode::Premultiplied);
    }

    {
        std::lock_guard<std::mutex> lk(mtx_);
        latest_ = bmp;   // replace; previous frame dropped
    }
    if (OnFrame) OnFrame();
}

SoftwareBitmap FrameSource::LatestFrame() {
    std::lock_guard<std::mutex> lk(mtx_);
    return latest_;
}

void FrameSource::Stop() {
    running_ = false;
    if (reader_) {
        if (frameToken_) { reader_.FrameArrived(frameToken_); frameToken_ = {}; }
        try { reader_.StopAsync(); } catch (...) {}
        reader_ = nullptr;
    }
    if (capture_) {
        try { capture_.Close(); } catch (...) {}
        capture_ = nullptr;
    }
    std::lock_guard<std::mutex> lk(mtx_);
    latest_ = nullptr;
}

} // namespace vai
