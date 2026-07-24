#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
yolo_sidecar.py — LongLook 上位机视觉识别边车进程
---------------------------------------------------------------------------
由 LongLook(Qt) 通过 QProcess 启动。与 LongLook 的约定协议：

  stdin (二进制)  : 每帧一包  [len u32 小端][JPEG 字节]
  stdout(文本行)  : 每帧一行 JSON:
        {"dets":[{"cls":"person","conf":0.87,"x":10,"y":20,"w":50,"h":80}, ...]}
  stderr          : 诊断/日志（LongLook 会转到界面日志）

坐标为送入图像的像素坐标，直接用于 LongLook 在同分辨率画面上叠加。

后期换模型：改 --model 指向新的 .pt（或改本脚本内的推理部分）即可，LongLook 无需重编。
用法：  python yolo_sidecar.py --model best.pt --conf 0.35
依赖：  pip install ultralytics opencv-python
"""
import sys
import io
import json
import struct
import argparse

# Windows 中文控制台默认 GBK，直接 print 会让上位机按 UTF-8 解码成乱码。
# 统一把 stdout/stderr 重配为 UTF-8。
try:
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
except Exception:
    pass


def log(msg):
    print(msg, file=sys.stderr, flush=True)


def read_exact(stream, n):
    """从二进制流精确读取 n 字节；EOF 返回 None。"""
    buf = bytearray()
    while len(buf) < n:
        chunk = stream.read(n - len(buf))
        if not chunk:
            return None
        buf.extend(chunk)
    return bytes(buf)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="best.pt", help="YOLO 权重路径")
    ap.add_argument("--conf", type=float, default=0.35, help="置信度阈值")
    ap.add_argument("--imgsz", type=int, default=640, help="推理输入尺寸")
    args = ap.parse_args()

    try:
        import numpy as np
        import cv2
        from ultralytics import YOLO
    except Exception as e:
        log("依赖缺失: %s" % e)
        log("当前解释器: %s" % sys.executable)
        log("请安装依赖后再开视觉: \"%s\" -m pip install ultralytics opencv-python numpy"
            % sys.executable)
        sys.exit(2)

    try:
        model = YOLO(args.model)
    except Exception as e:
        log("模型加载失败 %s: %s" % (args.model, e))
        sys.exit(3)

    names = getattr(model, "names", {}) or {}
    log("YOLO 就绪: model=%s conf=%.2f 类别数=%d" % (args.model, args.conf, len(names)))

    stdin = sys.stdin.buffer
    out = sys.stdout

    while True:
        hdr = read_exact(stdin, 4)
        if hdr is None:
            break
        (length,) = struct.unpack("<I", hdr)
        if length == 0 or length > 64 * 1024 * 1024:
            break
        data = read_exact(stdin, length)
        if data is None:
            break

        dets = []
        try:
            arr = np.frombuffer(data, dtype=np.uint8)
            img = cv2.imdecode(arr, cv2.IMREAD_COLOR)
            if img is not None:
                res = model.predict(img, conf=args.conf, imgsz=args.imgsz, verbose=False)
                if res:
                    r = res[0]
                    boxes = getattr(r, "boxes", None)
                    if boxes is not None:
                        for b in boxes:
                            xyxy = b.xyxy[0].tolist()
                            x1, y1, x2, y2 = xyxy
                            cls_id = int(b.cls[0].item())
                            conf = float(b.conf[0].item())
                            dets.append({
                                "cls": str(names.get(cls_id, cls_id)),
                                "conf": round(conf, 3),
                                "x": int(x1), "y": int(y1),
                                "w": int(x2 - x1), "h": int(y2 - y1),
                            })
        except Exception as e:
            log("推理异常: %s" % e)

        out.write(json.dumps({"dets": dets}) + "\n")
        out.flush()


if __name__ == "__main__":
    main()
