#pragma once
#include <cstdint>

namespace vai {

// Pixel layout of a caller-supplied frame buffer.
enum class PixelFormat {
    Rgb8,   // 3 bytes/pixel, R,G,B     (e.g. stb_image decode)
    Bgra8,  // 4 bytes/pixel, B,G,R,A   (e.g. SoftwareBitmap Bgra8)
};

// A single detection in ORIGINAL-frame pixel coordinates (not letterboxed).
struct Detection {
    float x = 0;      // left
    float y = 0;      // top
    float w = 0;      // width
    float h = 0;      // height
    int   classId = 0;
    float score = 0;  // confidence [0,1]
};

} // namespace vai
