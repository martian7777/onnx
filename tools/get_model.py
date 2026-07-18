import sys, subprocess
def sh(*a):
    print("+", *a, flush=True); subprocess.check_call(list(a))
try:
    import ultralytics  # noqa
except ImportError:
    sh(sys.executable, "-m", "pip", "install", "--quiet", "ultralytics")
from ultralytics import YOLO
m = YOLO("yolov8n.pt")  # downloads weights if missing
path = m.export(format="onnx", opset=13, imgsz=640, simplify=True, dynamic=False)
print("EXPORTED:", path, flush=True)
