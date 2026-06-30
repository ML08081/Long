#!/usr/bin/env python3
# =============================================================================
#  frame_parser_check.py — 收端校验工具（复刻 LongLook 的帧解析逻辑）
#
#  作为 TCP 客户端连接龙芯端（或 link_test），按协议 v2 解析流式帧，
#  统计/校验收到的 文本/传感器/视频 帧，验证“链路 + 帧字节”是否正确。
#
#  用法：
#     python3 test/frame_parser_check.py <ip> <port> [超时秒]
#  例：
#     python3 test/frame_parser_check.py 127.0.0.1 18080 5
# =============================================================================
import socket
import struct
import sys
import time

SOF = 0xA5
FRAME_SENSOR, FRAME_VIDEO, FRAME_THERMAL, FRAME_TEXT, FRAME_COMMAND = (
    0x01, 0x10, 0x20, 0x30, 0x40)


def parse_stream(buf, stats):
    """复刻 LongLook netclient.cpp::parseBuffer 的解帧逻辑，返回剩余未消费字节。"""
    off = 0
    n = len(buf)
    while True:
        # 1) 同步到 SOF
        sof = buf.find(bytes([SOF]), off)
        if sof < 0:
            return b""  # 无 SOF，全部丢弃
        off = sof
        # 2) 头部是否完整
        if n - off < 6:
            return buf[off:]
        ftype = buf[off + 1]
        length = struct.unpack_from("<I", buf, off + 2)[0]
        # 3) 长度异常 -> 失步，丢一字节
        if length > 16 * 1024 * 1024:
            off += 1
            continue
        # 4) 负载是否完整
        if n - off < 6 + length:
            return buf[off:]
        payload = buf[off + 6: off + 6 + length]
        off += 6 + length
        dispatch(ftype, payload, stats)


def dispatch(ftype, payload, stats):
    if ftype == FRAME_TEXT:
        stats["text"] += 1
        print(f"  [TEXT]   {payload.decode('utf-8', 'replace')}")
    elif ftype == FRAME_SENSOR:
        stats["sensor"] += 1
        ok = len(payload) >= 40
        temp = struct.unpack_from("<h", payload, 4)[0] / 10.0 if ok else None
        volt = struct.unpack_from("<H", payload, 30)[0] / 1000.0 if ok else None
        print(f"  [SENSOR] {len(payload)}B 温度={temp}°C 电压={volt}V "
              f"{'OK' if ok else '长度异常'}")
        if not ok:
            stats["bad"] += 1
    elif ftype == FRAME_VIDEO:
        stats["video"] += 1
        soi = payload[:2] == b"\xFF\xD8"
        eoi = payload[-2:] == b"\xFF\xD9"
        tag = "JPEG" if soi else "非JPEG!"
        if not soi:
            stats["bad"] += 1
        print(f"  [VIDEO]  {len(payload)}B {tag} SOI={soi} EOI={eoi}")
    elif ftype == FRAME_THERMAL:
        stats["thermal"] += 1
        print(f"  [THERMAL]{len(payload)}B")
    else:
        print(f"  [0x{ftype:02X}] {len(payload)}B (未知类型)")


def main():
    if len(sys.argv) < 3:
        print("用法: frame_parser_check.py <ip> <port> [超时秒]")
        return 2
    ip, port = sys.argv[1], int(sys.argv[2])
    timeout = float(sys.argv[3]) if len(sys.argv) > 3 else 5.0

    stats = {"text": 0, "sensor": 0, "video": 0, "thermal": 0, "bad": 0}
    print(f">>> 连接 {ip}:{port} (超时 {timeout}s) ...")
    s = socket.create_connection((ip, port), timeout=timeout)
    s.settimeout(timeout)
    buf = b""
    t0 = time.time()
    try:
        while time.time() - t0 < timeout:
            try:
                chunk = s.recv(65536)
            except socket.timeout:
                break
            if not chunk:
                break
            buf += chunk
            buf = parse_stream(buf, stats)
    finally:
        s.close()

    print(">>> 统计:", stats)
    ok = stats["bad"] == 0 and (stats["video"] > 0 or stats["text"] > 0)
    print(">>> 结果:", "通过 ✅" if ok else "失败 ❌")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
