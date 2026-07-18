#pragma once
#include <deque>
#include <chrono>
#include <mutex>
#include <string>

namespace vai {

// Thread-safe rolling telemetry: end-to-end FPS and inference latency.
// The worker thread reports inference latency; the UI thread ticks frames.
class Stats {
public:
    // Called by the render/UI loop once per displayed frame.
    void TickFrame();

    // Called by the inference worker with the most recent Session::Run time.
    void ReportInference(double ms);

    void SetHardwareTarget(std::wstring target);

    struct Snapshot {
        double fps            = 0.0;
        double inferenceMs    = 0.0;  // rolling average
        double inferenceMsMin = 0.0;
        double inferenceMsMax = 0.0;
        std::wstring hardwareTarget;
    };
    Snapshot Get() const;

private:
    using clock = std::chrono::steady_clock;

    mutable std::mutex     mtx_;
    std::deque<clock::time_point> frameTimes_;   // sliding 1s window
    std::deque<double>     inferMs_;             // last N inference times
    std::wstring           hardwareTarget_ = L"(initializing)";

    static constexpr size_t kInferWindow = 30;
};

} // namespace vai
