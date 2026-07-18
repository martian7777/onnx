# Contributing to VisionAI

Thank you for your interest in contributing to VisionAI! This document provides guidelines and instructions for contributing to this project.

## 📋 Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Project Architecture](#project-architecture)
- [Making Changes](#making-changes)
- [Coding Standards](#coding-standards)
- [Submitting a Pull Request](#submitting-a-pull-request)
- [Reporting Bugs](#reporting-bugs)
- [Requesting Features](#requesting-features)

---

## Code of Conduct

By participating in this project, you agree to maintain a respectful and inclusive environment. Please:

- Be kind and courteous to other contributors
- Respect differing viewpoints and experiences
- Accept constructive criticism gracefully
- Focus on what is best for the community and the project

---

## Getting Started

1. **Fork** the repository on GitHub
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/<your-username>/onnx.git
   ```
3. **Add the upstream remote:**
   ```bash
   git remote add upstream https://github.com/martian7777/onnx.git
   ```
4. **Create a branch** for your changes:
   ```bash
   git checkout -b feature/your-feature-name
   ```

---

## Development Setup

### Prerequisites

| Tool | Version | Required For |
|---|---|---|
| Visual Studio 2022 | 17.x | Building the solution |
| C++ Desktop Development workload | — | C++17 compiler + Windows SDK |
| Python 3.8+ | Optional | Model export scripts only |

### Build Steps

1. Open `VisionAI.sln` in Visual Studio 2022
2. Right-click the solution → **Restore NuGet Packages**
3. Set configuration to **Debug | x64**
4. Build the solution (**Ctrl+Shift+B**)

### Running Tests

Set **InferenceTest** as the startup project and run with **Ctrl+F5**. This validates the core inference engine without needing a webcam.

---

## Project Architecture

Understanding the component boundaries will help you make targeted changes:

```
VisionAI/
├── main.cpp + App.{h,cpp}        → Application entry point & shell
├── MainWindow.{h,cpp}            → UI construction & pipeline orchestration
├── Capture/FrameSource.{h,cpp}   → Webcam acquisition (WinRT-dependent)
├── Inference/                    → Core detection engine (WinRT-FREE)
│   ├── YoloDetector.{h,cpp}     → ONNX Runtime session + pre/postprocessing
│   ├── Detection.h              → Detection result struct
│   └── Nms.{h,cpp}             → Non-Maximum Suppression
└── Telemetry/Stats.{h,cpp}     → FPS & latency tracking
```

**Key design principle:** The `Inference/` module has **zero WinRT dependencies**. If your change is in the inference pipeline, it must compile without any WinRT headers. The `InferenceTest` project enforces this constraint.

---

## Making Changes

### Branch Naming Convention

| Type | Format | Example |
|---|---|---|
| Feature | `feature/description` | `feature/yolov11-support` |
| Bug fix | `fix/description` | `fix/nms-iou-calculation` |
| Documentation | `docs/description` | `docs/api-reference` |
| Performance | `perf/description` | `perf/preprocess-simd` |

### Commit Messages

Follow the [Conventional Commits](https://www.conventionalcommits.org/) specification:

```
<type>(<scope>): <short description>

[optional body]
```

**Types:** `feat`, `fix`, `docs`, `perf`, `refactor`, `test`, `build`, `ci`

**Examples:**
```
feat(inference): add YOLOv11 segmentation output parsing
fix(capture): handle camera disconnect during inference
docs(readme): add ARM64 build instructions
perf(preprocess): use SIMD intrinsics for letterbox resize
```

---

## Coding Standards

### C++ Style

- **Standard:** C++17
- **Naming:**
  - Classes and structs: `PascalCase` (e.g., `YoloDetector`, `FrameSource`)
  - Functions and methods: `PascalCase` (e.g., `LoadLabels`, `Run`)
  - Local variables: `camelCase` (e.g., `bestScore`, `srcRow`)
  - Member variables: `camelCase_` with trailing underscore (e.g., `session_`, `running_`)
  - Constants: `kPascalCase` (e.g., `kPalette`, `kInferWindow`)
  - Enums: `PascalCase` for both the type and values (e.g., `PixelFormat::Bgra8`)
- **Namespaces:** All project code lives in `namespace vai`
- **Headers:** Use `#pragma once` for include guards
- **Includes:** Group as: project headers → third-party → system/STL, separated by blank lines

### Thread Safety

- Use `std::atomic` for simple scalars shared across threads
- Use `std::mutex` with `std::lock_guard` for compound shared state
- Never hold a lock while calling into ONNX Runtime or WinRT async APIs
- The `Inference/` module must remain free of WinRT dependencies

### Error Handling

- Use exceptions for initialization failures (caught at appropriate boundaries)
- Use return values (`bool`, empty vectors) for per-frame failures
- The UI render loop catches `winrt::hresult_error` to prevent single-frame failures from crashing the app

---

## Submitting a Pull Request

1. **Ensure your branch is up to date:**
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

2. **Test your changes:**
   - Build in both Debug and Release configurations
   - Run `InferenceTest` to verify the inference pipeline
   - If you modified the UI, test with a live webcam

3. **Push your branch:**
   ```bash
   git push origin feature/your-feature-name
   ```

4. **Open a Pull Request** on GitHub with:
   - A clear title following commit message conventions
   - A description of *what* changed and *why*
   - Screenshots or console output if applicable
   - Reference to any related issues (e.g., `Fixes #42`)

5. **Respond to review feedback** — maintainers may request changes before merging.

---

## Reporting Bugs

Open an [Issue](https://github.com/martian7777/onnx/issues/new) with:

- **Title:** A concise description of the bug
- **Environment:** OS version, GPU model, Visual Studio version
- **Steps to reproduce:** Numbered steps to trigger the bug
- **Expected behavior:** What should happen
- **Actual behavior:** What actually happens
- **Logs/screenshots:** Any relevant error messages, console output, or screenshots

---

## Requesting Features

Open an [Issue](https://github.com/martian7777/onnx/issues/new) with:

- **Title:** A concise description of the feature
- **Problem:** What problem does this feature solve?
- **Proposed solution:** How would you like it to work?
- **Alternatives considered:** Other approaches you've thought of
- **Additional context:** Mockups, links, or references

---

## 💬 Questions?

If you have questions about contributing, feel free to open a [Discussion](https://github.com/martian7777/onnx/discussions) on GitHub.

---

Thank you for helping make VisionAI better! 🚀
