#pragma once
#include "Detection.h"
#include <vector>

namespace vai {

// Intersection-over-Union of two axis-aligned boxes (x,y,w,h).
float IoU(const Detection& a, const Detection& b);

// Class-aware greedy non-maximum suppression.
// Input boxes may be in any order; returns kept boxes sorted by descending score.
std::vector<Detection> NonMaxSuppression(std::vector<Detection> boxes, float iouThreshold);

} // namespace vai
