#include "Nms.h"
#include <algorithm>

namespace vai {

float IoU(const Detection& a, const Detection& b) {
    const float ax2 = a.x + a.w, ay2 = a.y + a.h;
    const float bx2 = b.x + b.w, by2 = b.y + b.h;

    const float ix1 = std::max(a.x, b.x);
    const float iy1 = std::max(a.y, b.y);
    const float ix2 = std::min(ax2, bx2);
    const float iy2 = std::min(ay2, by2);

    const float iw = std::max(0.0f, ix2 - ix1);
    const float ih = std::max(0.0f, iy2 - iy1);
    const float inter = iw * ih;
    if (inter <= 0.0f) return 0.0f;

    const float uni = a.w * a.h + b.w * b.h - inter;
    return uni > 0.0f ? inter / uni : 0.0f;
}

std::vector<Detection> NonMaxSuppression(std::vector<Detection> boxes, float iouThreshold) {
    std::sort(boxes.begin(), boxes.end(),
              [](const Detection& l, const Detection& r) { return l.score > r.score; });

    std::vector<Detection> kept;
    std::vector<char> removed(boxes.size(), 0);

    for (size_t i = 0; i < boxes.size(); ++i) {
        if (removed[i]) continue;
        kept.push_back(boxes[i]);
        for (size_t j = i + 1; j < boxes.size(); ++j) {
            if (removed[j]) continue;
            if (boxes[j].classId != boxes[i].classId) continue;   // class-aware
            if (IoU(boxes[i], boxes[j]) > iouThreshold) removed[j] = 1;
        }
    }
    return kept;
}

} // namespace vai
