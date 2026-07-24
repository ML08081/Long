#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_cjk_font.py —— 生成龙芯 SPI 小屏用的 16x16 中文点阵字体头文件。
仅收录 PatrolSystem 显示界面实际用到的汉字，产物 include/modules/display/CjkFont16.h。
用法(Windows): F:\\ProjectOPaYO\\my_envs\\python.exe scripts\\gen_cjk_font.py
"""
import os
from PIL import Image, ImageFont, ImageDraw

FONT_PATH = r"C:\Windows\Fonts\simhei.ttf"
PX = 16                       # 字模尺寸 16x16
OUT = os.path.join(os.path.dirname(__file__), "..", "include", "modules", "display", "CjkFont16.h")

# 界面用到的全部汉字（去重后生成；新增文案请在此补字并重跑）
CHARS = (
    "巡检状态传感数据"
    "模式手动自避障航"
    "距离激光气体温度湿"
    "安全注意警告危险火"
    "中转站上位机在线网络断开已"
    "地址时长下正常未接"
    "遥测环境热成像"
    "视觉关闭"
    "版本运行帧秒无有"
    # 2026-07-23 追加：传感页全中文化 + 模式行所需
    "循迹控巡离连厘米度百分号超标等待"
    # 2026-07-23 追加：遥控避障锁
    "锁停"
)

def render(ch, font):
    img = Image.new("L", (PX, PX), 0)
    d = ImageDraw.Draw(img)
    # 居中排布：用 bbox 计算偏移，尽量把字放进 16x16
    try:
        bbox = d.textbbox((0, 0), ch, font=font)
        w = bbox[2] - bbox[0]; h = bbox[3] - bbox[1]
        ox = (PX - w) // 2 - bbox[0]
        oy = (PX - h) // 2 - bbox[1]
    except Exception:
        ox, oy = 0, 0
    d.text((ox, oy), ch, fill=255, font=font)
    px = img.load()
    rows = []
    for y in range(PX):
        b0 = b1 = 0
        for x in range(PX):
            on = 1 if px[x, y] >= 128 else 0
            if on:
                if x < 8:
                    b0 |= (1 << (7 - x))
                else:
                    b1 |= (1 << (7 - (x - 8)))
        rows.append((b0, b1))
    return rows

def main():
    font = ImageFont.truetype(FONT_PATH, PX)
    seen = []
    for ch in CHARS:
        if ch not in seen and not ch.isspace():
            seen.append(ch)
    seen.sort(key=lambda c: ord(c))   # 按码点排序，C 端二分查找

    lines = []
    lines.append("// CjkFont16.h —— 自动生成，请勿手改。由 scripts/gen_cjk_font.py 产出。")
    lines.append("// 16x16 中文点阵：每字 32 字节 = 16 行 x 2 字节(MSB=最左像素)。按码点升序，C 端二分。")
    lines.append("#ifndef PATROL_MODULES_DISPLAY_CJKFONT16_H")
    lines.append("#define PATROL_MODULES_DISPLAY_CJKFONT16_H")
    lines.append("#include <cstdint>")
    lines.append("namespace patrol {")
    lines.append("struct CjkGlyph16 { uint32_t cp; uint8_t rows[32]; };")
    lines.append("static const CjkGlyph16 kCjk16[] = {")
    for ch in seen:
        rows = render(ch, font)
        flat = []
        for (b0, b1) in rows:
            flat.append(b0); flat.append(b1)
        hexs = ",".join("0x%02X" % b for b in flat)
        lines.append("  {0x%04X,{%s}}, // %s" % (ord(ch), hexs, ch))
    lines.append("};")
    lines.append("static const int kCjk16Count = %d;" % len(seen))
    lines.append("} // namespace patrol")
    lines.append("#endif")

    outp = os.path.abspath(OUT)
    with open(outp, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("生成 %d 个汉字 -> %s" % (len(seen), outp))

if __name__ == "__main__":
    main()
