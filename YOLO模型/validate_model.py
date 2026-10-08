#!/usr/bin/env python3
"""Validate the bundled fire/smoke model against known positive and negative images."""

import json
import time
from pathlib import Path

import cv2
import numpy as np
from ultralytics import YOLO


ROOT = Path(__file__).resolve().parent
MODEL = ROOT / "fire_smoke_yolov8n.pt"
IMAGE_DIR = ROOT / "测试图片"
OUTPUT_DIR = ROOT / "测试结果"
FIRE_CONF = 0.25
SMOKE_CONF = 0.50


def read_image(path: Path):
    data = np.fromfile(str(path), dtype=np.uint8)
    return cv2.imdecode(data, cv2.IMREAD_COLOR)


def write_image(path: Path, image):
    ok, encoded = cv2.imencode(".jpg", image)
    if not ok:
        raise RuntimeError(f"cannot encode {path.name}")
    encoded.tofile(str(path))


def main():
    if not MODEL.is_file():
        raise FileNotFoundError(MODEL)

    images = sorted(IMAGE_DIR.glob("*_test_*.jpg"))
    if not images:
        raise RuntimeError(f"no test images in {IMAGE_DIR}")

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    model = YOLO(str(MODEL))
    frames = [read_image(path) for path in images]
    if any(frame is None for frame in frames):
        raise RuntimeError("one or more test images cannot be decoded")

    started = time.perf_counter()
    results = model.predict(frames, imgsz=416, conf=0.20, verbose=False)
    elapsed_ms = (time.perf_counter() - started) * 1000.0

    records = []
    all_passed = True
    for path, result in zip(images, results):
        detections = []
        for box in result.boxes:
            class_id = int(box.cls[0].item())
            class_name = str(model.names[class_id]).lower()
            confidence = float(box.conf[0].item())
            threshold = SMOKE_CONF if "smoke" in class_name else FIRE_CONF
            if confidence >= threshold:
                detections.append({"class": class_name, "confidence": round(confidence, 3)})

        expected = path.name.split("_test_", 1)[0]
        if expected == "normal":
            passed = not detections
        else:
            passed = any(item["class"] == expected for item in detections)
        all_passed = all_passed and passed

        write_image(OUTPUT_DIR / path.name, result.plot())
        records.append({
            "image": path.name,
            "expected": expected,
            "passed": passed,
            "detections": detections,
            "inference_ms": round(float(result.speed.get("inference", 0.0)), 1),
        })

    report = {
        "model": MODEL.name,
        "classes": model.names,
        "imgsz": 416,
        "fire_threshold": FIRE_CONF,
        "smoke_threshold": SMOKE_CONF,
        "total_ms": round(elapsed_ms, 1),
        "all_passed": all_passed,
        "images": records,
    }
    (OUTPUT_DIR / "validation_report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(json.dumps(report, ensure_ascii=False, indent=2))
    raise SystemExit(0 if all_passed else 1)


if __name__ == "__main__":
    main()
