"""Render actual C++ framebuffer test output into a contact sheet for visual review."""
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
frames = root / "build-display-test" / "previews"
names = ["sensor0", "sensor3", "offline", "relay0", "relay1", "relay2"]
canvas = Image.new("RGB", (3 * 272, 2 * 350), "#1d2430")
draw = ImageDraw.Draw(canvas)
for i, name in enumerate(names):
    img = Image.open(frames / (name + ".ppm")).convert("RGB")
    assert img.size == (128, 160)
    x, y = (i % 3) * 272 + 8, (i // 3) * 350 + 24
    draw.text((x, y - 18), name, fill="white")
    canvas.paste(img.resize((256, 320), Image.Resampling.NEAREST), (x, y))
canvas.save(frames / "pages.png")
print(frames / "pages.png")
