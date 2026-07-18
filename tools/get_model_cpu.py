"""Robust YOLOv8n -> ONNX export.

Installs the CPU-only torch wheel (far smaller than the default CUDA build)
with generous timeouts/retries, then ultralytics, then exports the model.
Run from e:\\onnx\\VisionAI\\Assets so the .onnx lands next to coco.names.
"""
import subprocess, sys

PIP = [sys.executable, "-m", "pip", "install", "--default-timeout=100", "--retries", "30"]

def sh(*a):
    print("+", " ".join(a), flush=True)
    subprocess.check_call(list(a))

# CPU torch first (small, reliable). ultralytics then reuses it.
sh(*PIP, "--index-url", "https://download.pytorch.org/whl/cpu", "torch")
sh(*PIP, "ultralytics", "onnx", "onnxslim")

from ultralytics import YOLO
model = YOLO("yolov8n.pt")                      # downloads ~6 MB weights
path = model.export(format="onnx", opset=13, imgsz=640, simplify=True, dynamic=False)
print("EXPORTED:", path, flush=True)
