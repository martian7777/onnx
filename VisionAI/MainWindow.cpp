#include "pch.h"
#include "MainWindow.h"

#include <fstream>
#include <array>
#include <algorithm>
#include <chrono>
#include <cstring>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
using namespace winrt::Microsoft::UI::Xaml::Shapes;

struct __declspec(uuid("5B0D3235-4DBA-4D44-865E-8F1D0E4FD04D")) __declspec(novtable)
IMemoryBufferByteAccess : ::IUnknown {
    virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
};

namespace {

using winrt::Windows::UI::Color;
Color Rgb(uint8_t r, uint8_t g, uint8_t b) { return Color{ 255, r, g, b }; }

const std::array<Color, 10> kPalette{ {
    {255,  46, 204, 113}, {255, 231,  76,  60}, {255,  52, 152, 219},
    {255, 241, 196,  15}, {255, 155,  89, 182}, {255,  26, 188, 156},
    {255, 230, 126,  34}, {255,  86, 156, 255}, {255, 236, 240, 241},
    {255, 149, 165, 166},
} };

std::wstring AssetPath(std::wstring_view name) {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring dir(buf);
    dir = dir.substr(0, dir.find_last_of(L"\\/"));
    return dir + L"\\Assets\\" + std::wstring(name);
}

std::vector<std::string> LoadLabels(const std::wstring& path) {
    std::vector<std::string> names;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) names.push_back(line);
    }
    return names;
}

TextBlock Label(std::wstring text, double size, bool bold, Color color) {
    TextBlock t;
    t.Text(text);
    t.FontSize(size);
    if (bold) t.FontWeight(winrt::Microsoft::UI::Text::FontWeights::Bold());
    t.Foreground(SolidColorBrush(color));
    return t;
}

std::wstring Widen(const char* s) { return std::wstring(s, s + strlen(s)); }

} // namespace

namespace vai {

MainWindow::MainWindow() {
    dispatcher_ = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
    labels_ = LoadLabels(AssetPath(L"coco.names"));
    BuildUi();
}

MainWindow::~MainWindow() { Stop(); }

void MainWindow::Activate() {
    window_.Activate();
    if (auto aw = window_.AppWindow()) aw.Resize({ 1280, 780 });
}

void MainWindow::BuildUi() {
    window_ = Window();
    window_.Title(L"Local Vision AI Dashboard");

    const Color kBg{ 255, 18, 20, 26 };
    const Color kBar{ 255, 28, 31, 40 };
    const Color kCard{ 255, 30, 33, 43 };
    const Color kText{ 255, 240, 242, 245 };
    const Color kSub{ 255, 150, 158, 170 };

    Grid root;
    root.Background(SolidColorBrush(kBg));
    { RowDefinition r; r.Height(GridLengthHelper::Auto()); root.RowDefinitions().Append(r); }
    { RowDefinition r; r.Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star)); root.RowDefinitions().Append(r); }

    // ---- Command bar ----
    StackPanel bar;
    bar.Orientation(Orientation::Horizontal);
    bar.Spacing(16);
    bar.Padding(ThicknessHelper::FromLengths(16, 10, 16, 10));
    bar.VerticalAlignment(VerticalAlignment::Center);
    bar.Background(SolidColorBrush(kBar));
    bar.Children().Append(Label(L"Local Vision AI", 18, true, kText));

    startBtn_ = Controls::Primitives::ToggleButton();
    startBtn_.Content(box_value(L"Start"));
    startBtn_.MinWidth(90);
    startBtn_.Click([this](auto&&, auto&&) { OnStartStop(); });
    bar.Children().Append(startBtn_);

    {
        StackPanel sp; sp.Width(200);
        sp.Children().Append(Label(L"Confidence", 12, false, kSub));
        confSlider_ = Slider();
        confSlider_.Minimum(5); confSlider_.Maximum(90); confSlider_.Value(25);
        confSlider_.StepFrequency(5);
        confSlider_.ValueChanged([this](auto&&, Controls::Primitives::RangeBaseValueChangedEventArgs const& e) {
            confThreshold_.store(static_cast<float>(e.NewValue() / 100.0));
        });
        sp.Children().Append(confSlider_);
        bar.Children().Append(sp);
    }

    statusText_ = Label(L"Idle", 13, false, kSub);
    statusText_.VerticalAlignment(VerticalAlignment::Center);
    bar.Children().Append(statusText_);

    Grid::SetRow(bar, 0);
    root.Children().Append(bar);

    // ---- Content ----
    Grid content;
    { ColumnDefinition c; c.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star)); content.ColumnDefinitions().Append(c); }
    { ColumnDefinition c; c.Width(GridLengthHelper::FromValueAndType(320, GridUnitType::Pixel)); content.ColumnDefinitions().Append(c); }

    // Video (Image) + overlay (Canvas) stacked in one cell.
    Border videoBorder;
    videoBorder.Margin(ThicknessHelper::FromLengths(12, 12, 6, 12));
    videoBorder.CornerRadius(CornerRadiusHelper::FromUniformRadius(8));
    videoBorder.Background(SolidColorBrush(Rgb(0, 0, 0)));

    Grid videoCell;
    image_ = Image();
    image_.Stretch(Stretch::Uniform);
    imageSource_ = SoftwareBitmapSource();
    image_.Source(imageSource_);
    videoCell.Children().Append(image_);

    overlay_ = Canvas();
    videoCell.Children().Append(overlay_);

    videoBorder.Child(videoCell);
    Grid::SetColumn(videoBorder, 0);
    content.Children().Append(videoBorder);

    // Telemetry panel
    Border panel;
    panel.Margin(ThicknessHelper::FromLengths(6, 12, 12, 12));
    panel.Padding(ThicknessHelper::FromUniformLength(18));
    panel.CornerRadius(CornerRadiusHelper::FromUniformRadius(8));
    panel.Background(SolidColorBrush(kCard));

    StackPanel col; col.Spacing(16);
    col.Children().Append(Label(L"PERFORMANCE", 12, true, kSub));
    fpsText_ = Label(L"0.0", 46, true, kText);
    col.Children().Append(fpsText_);
    col.Children().Append(Label(L"Frames / second", 12, false, kSub));
    latencyText_ = Label(L"0.0 ms", 30, true, kText);
    col.Children().Append(latencyText_);
    col.Children().Append(Label(L"Inference latency (avg)", 12, false, kSub));
    latencyRange_ = Label(L"min - / max -", 11, false, kSub);
    col.Children().Append(latencyRange_);
    col.Children().Append(Label(L"HARDWARE TARGET", 12, true, kSub));
    hwText_ = Label(L"(initializing)", 14, false, kText);
    hwText_.TextWrapping(TextWrapping::Wrap);
    col.Children().Append(hwText_);
    col.Children().Append(Label(L"OBJECTS", 12, true, kSub));
    detText_ = Label(L"0 detected", 14, false, kText);
    col.Children().Append(detText_);

    panel.Child(col);
    Grid::SetColumn(panel, 1);
    content.Children().Append(panel);

    Grid::SetRow(content, 1);
    root.Children().Append(content);

    window_.Content(root);
}

void MainWindow::OnStartStop() {
    auto checked = startBtn_.IsChecked();
    if (checked && checked.Value()) {
        startBtn_.Content(box_value(L"Stop"));
        Start();
    } else {
        startBtn_.Content(box_value(L"Start"));
        Stop();
    }
}

void MainWindow::Start() {
    if (running_.exchange(true)) return;
    statusText_.Text(L"Starting camera...");

    source_.OnFrame = [this]() {
        if (dispatcher_ && !renderQueued_.exchange(true)) {
            dispatcher_.TryEnqueue([this]() { renderQueued_.store(false); RenderFrame(); });
        }
    };

    worker_ = std::thread([this]() { WorkerLoop(); });

    [](MainWindow* self) -> fire_and_forget {
        try {
            co_await self->source_.StartCameraAsync();
        } catch (hresult_error const& ex) {
            auto msg = ex.message();
            self->dispatcher_.TryEnqueue([self, msg]() { self->statusText_.Text(L"Camera error: " + msg); });
        }
    }(this);
}

void MainWindow::Stop() {
    if (!running_.exchange(false)) return;
    source_.Stop();
    if (worker_.joinable()) worker_.join();
    { std::lock_guard<std::mutex> lk(detMutex_); detections_.clear(); }
    if (overlay_) overlay_.Children().Clear();
    if (statusText_) statusText_.Text(L"Idle");
}

void MainWindow::WorkerLoop() {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);   // COM for D3D12/DXGI on this thread
    if (!detector_) {
        detector_ = std::make_unique<YoloDetector>();
        YoloDetector::Options opts;
        opts.modelPath     = AssetPath(L"yolov8n.onnx");
        opts.confThreshold = confThreshold_.load();
        opts.useDirectML   = true;
        bool ok = detector_->Load(opts);

        std::wstring hw = detector_->HardwareTarget();
        stats_.SetHardwareTarget(hw);
        dispatcher_.TryEnqueue([this, hw, ok]() {
            hwText_.Text(hw);
            statusText_.Text(ok ? L"Running" : L"Model load failed");
        });
        if (!ok) { running_ = false; return; }
    }

    uint64_t lastId = 0;
    while (running_.load()) {
        const uint64_t id = source_.FrameId();
        if (id == lastId) { std::this_thread::sleep_for(std::chrono::milliseconds(2)); continue; }
        SoftwareBitmap bmp = source_.LatestFrame();
        if (!bmp) { std::this_thread::sleep_for(std::chrono::milliseconds(2)); continue; }
        lastId = id;

        detector_->SetConfidence(confThreshold_.load());

        BitmapBuffer buffer = bmp.LockBuffer(BitmapBufferAccessMode::Read);
        auto reference = buffer.CreateReference();
        uint8_t* data = nullptr; uint32_t capacity = 0;
        auto access = reference.as<IMemoryBufferByteAccess>();
        if (FAILED(access->GetBuffer(&data, &capacity)) || !data) { reference.Close(); buffer.Close(); continue; }

        auto desc = buffer.GetPlaneDescription(0);
        double ms = 0;
        auto dets = detector_->Run(data, desc.Width, desc.Height, desc.Stride, PixelFormat::Bgra8, &ms);

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

void MainWindow::RenderFrame() try {
    stats_.TickFrame();

    SoftwareBitmap bmp = source_.LatestFrame();
    if (bmp && !imageBusy_.exchange(true)) {
        // SoftwareBitmapSource requires Bgra8 + premultiplied alpha; convert
        // (also decouples from the worker's read lock on the shared frame).
        SoftwareBitmap disp = SoftwareBitmap::Convert(
            bmp, BitmapPixelFormat::Bgra8, BitmapAlphaMode::Premultiplied);
        imageSource_.SetBitmapAsync(disp).Completed([this](auto&&, auto&&) { imageBusy_.store(false); });
    } else if (!bmp) {
        imageBusy_.store(false);
    }

    // Reposition overlays to match the letterboxed Image.
    std::vector<Detection> dets;
    int fw = 0, fh = 0;
    { std::lock_guard<std::mutex> lk(detMutex_); dets = detections_; fw = frameW_; fh = frameH_; }

    overlay_.Children().Clear();
    const double cw = overlay_.ActualWidth();
    const double ch = overlay_.ActualHeight();
    if (fw > 0 && fh > 0 && cw > 1 && ch > 1) {
        const double scale = std::min(cw / fw, ch / fh);
        const double dw = fw * scale, dh = fh * scale;
        const double ox = (cw - dw) * 0.5, oy = (ch - dh) * 0.5;

        for (auto const& d : dets) {
            auto color = kPalette[d.classId % kPalette.size()];
            const double rx = ox + d.x * scale;
            const double ry = oy + d.y * scale;

            Shapes::Rectangle box;
            box.Width(d.w * scale);
            box.Height(d.h * scale);
            box.Stroke(SolidColorBrush(color));
            box.StrokeThickness(2.5);
            Canvas::SetLeft(box, rx);
            Canvas::SetTop(box, ry);
            overlay_.Children().Append(box);

            const char* nm = (d.classId >= 0 && d.classId < (int)labels_.size())
                                 ? labels_[d.classId].c_str() : "?";
            wchar_t buf[64];
            swprintf_s(buf, L"%s %d%%", Widen(nm).c_str(), (int)(d.score * 100));
            TextBlock t;
            t.Text(buf);
            t.FontSize(12);
            t.Foreground(SolidColorBrush(Rgb(0, 0, 0)));
            Border lb;
            lb.Background(SolidColorBrush(color));
            lb.Padding(ThicknessHelper::FromLengths(4, 1, 4, 1));
            lb.Child(t);
            Canvas::SetLeft(lb, rx);
            Canvas::SetTop(lb, std::max(0.0, ry - 20));
            overlay_.Children().Append(lb);
        }
    }

    UpdateTelemetry();
} catch (winrt::hresult_error const&) {
    // A transient per-frame WinRT failure must not tear down the render loop.
}

void MainWindow::UpdateTelemetry() {
    auto s = stats_.Get();
    wchar_t buf[128];
    swprintf_s(buf, L"%.1f", s.fps);                 fpsText_.Text(buf);
    swprintf_s(buf, L"%.1f ms", s.inferenceMs);      latencyText_.Text(buf);
    swprintf_s(buf, L"min %.1f / max %.1f ms", s.inferenceMsMin, s.inferenceMsMax);
    latencyRange_.Text(buf);
    size_t n = 0;
    { std::lock_guard<std::mutex> lk(detMutex_); n = detections_.size(); }
    swprintf_s(buf, L"%zu detected", n);             detText_.Text(buf);
}

} // namespace vai
