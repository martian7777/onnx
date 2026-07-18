// Headless verification harness for the shared inference core (plan Milestone 3).
// Loads yolov8n.onnx via ORT + DirectML, runs it on a JPEG, and prints the
// detected classes / boxes / latency. No WinRT, no XAML.

#include "YoloDetector.h"
#include "stb_image.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <clocale>

static std::vector<std::string> LoadLabels(const std::string& path) {
    std::vector<std::string> names;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) names.push_back(line);
    }
    return names;
}

static std::wstring ToW(const std::string& s) { return std::wstring(s.begin(), s.end()); }

int main(int argc, char** argv) {
    std::string modelPath  = argc > 1 ? argv[1] : "VisionAI/Assets/yolov8n.onnx";
    std::string imagePath  = argc > 2 ? argv[2] : "test-assets/bus.jpg";
    std::string labelsPath = argc > 3 ? argv[3] : "VisionAI/Assets/coco.names";

    auto labels = LoadLabels(labelsPath);
    printf("Model : %s\n", modelPath.c_str());
    printf("Image : %s\n", imagePath.c_str());
    printf("Labels: %zu classes\n", labels.size());

    int w = 0, h = 0, ch = 0;
    unsigned char* rgb = stbi_load(imagePath.c_str(), &w, &h, &ch, 3); // force RGB
    if (!rgb) { printf("ERROR: could not load image (%s)\n", stbi_failure_reason()); return 2; }
    printf("Decoded image: %dx%d (%d ch)\n", w, h, ch);

    vai::YoloDetector det;
    vai::YoloDetector::Options opts;
    opts.modelPath = ToW(modelPath);
    opts.useDirectML = true;
    if (!det.Load(opts)) {
        printf("ERROR: detector load failed: %s\n", det.LastError().c_str());
        stbi_image_free(rgb);
        return 3;
    }
    wprintf(L"Hardware target: %s\n", det.HardwareTarget().c_str());

    // Warm-up + timed runs.
    double ms = 0;
    std::vector<vai::Detection> dets;
    for (int i = 0; i < 5; ++i) {
        dets = det.Run(rgb, w, h, w * 3, vai::PixelFormat::Rgb8, &ms);
        printf("  run %d: %.2f ms, %zu detections\n", i, ms, dets.size());
    }

    std::sort(dets.begin(), dets.end(),
              [](const vai::Detection& a, const vai::Detection& b) { return a.score > b.score; });
    printf("\nDetections (conf >= %.2f):\n", opts.confThreshold);
    for (const auto& d : dets) {
        const char* name = (d.classId >= 0 && d.classId < (int)labels.size())
                               ? labels[d.classId].c_str() : "?";
        printf("  %-14s %.2f  [x=%.0f y=%.0f w=%.0f h=%.0f]\n",
               name, d.score, d.x, d.y, d.w, d.h);
    }

    stbi_image_free(rgb);
    return dets.empty() ? 1 : 0;
}
