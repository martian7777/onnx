#include "Stats.h"
#include <algorithm>
#include <numeric>

namespace vai {

void Stats::TickFrame() {
    const auto now = clock::now();
    std::lock_guard<std::mutex> lk(mtx_);
    frameTimes_.push_back(now);
    // Keep only the last second of frame timestamps.
    const auto cutoff = now - std::chrono::seconds(1);
    while (!frameTimes_.empty() && frameTimes_.front() < cutoff)
        frameTimes_.pop_front();
}

void Stats::ReportInference(double ms) {
    std::lock_guard<std::mutex> lk(mtx_);
    inferMs_.push_back(ms);
    while (inferMs_.size() > kInferWindow) inferMs_.pop_front();
}

void Stats::SetHardwareTarget(std::wstring target) {
    std::lock_guard<std::mutex> lk(mtx_);
    hardwareTarget_ = std::move(target);
}

Stats::Snapshot Stats::Get() const {
    std::lock_guard<std::mutex> lk(mtx_);
    Snapshot s;
    s.hardwareTarget = hardwareTarget_;

    // FPS = frames observed in the trailing 1-second window.
    s.fps = static_cast<double>(frameTimes_.size());

    if (!inferMs_.empty()) {
        s.inferenceMs    = std::accumulate(inferMs_.begin(), inferMs_.end(), 0.0) / inferMs_.size();
        s.inferenceMsMin = *std::min_element(inferMs_.begin(), inferMs_.end());
        s.inferenceMsMax = *std::max_element(inferMs_.begin(), inferMs_.end());
    }
    return s;
}

} // namespace vai
