#!/usr/bin/env python3
"""Generate the ready-only QUIC status badge, without modifying brand assets."""
from pathlib import Path
from PIL import Image, ImageDraw

image = Image.new("RGBA", (64, 64))
draw = ImageDraw.Draw(image)
draw.ellipse((5, 5, 57, 57), outline=(72, 224, 148, 255), width=7)
draw.line((36, 37, 57, 58), fill=(72, 224, 148, 255), width=7)
root = Path(__file__).resolve().parents[1]
image.resize((24, 24), Image.Resampling.LANCZOS).save(
    root / "indra/newview/skins/default/textures/icons/quic_on.png")
