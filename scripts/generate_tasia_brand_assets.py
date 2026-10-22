#!/usr/bin/env python3
"""Generate viewer branding from the approved PNG (requires Pillow).

Only transparent outer margins are removed. The complete artwork and aspect
ratio are retained, including in square application icons. The supplied input
is never modified. Legacy viewer-logo outputs are regenerated too, so alternate
skins cannot silently reintroduce upstream branding. Copyright notices remain.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path

from PIL import Image, ImageOps, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
VIEWER = ROOT / 'indra/newview'
ICO_SIZES = [(s, s) for s in (16, 20, 24, 32, 40, 48, 64, 128, 256)]
PNG_OUTPUTS = {
    'skins/default/textures/windows/login_tasia_logo.png': (512, 288),
    'skins/default/textures/windows/login_tasia_logo_horizontal.png': (512, 288),
    'skins/default/textures/windows/login_tasia_logo_small.png': (256, 144),
    'skins/default/textures/windows/startup_logo.png': (1024, 1024),
    'skins/default/textures/icons/tasia_app.png': (256, 256),
    'skins/default/textures/toolbar_icons/tasia_chat.png': (48, 48),
    'skins/default/textures/icons/tasiafeed.png': (48, 48),
}
for _skin in ('default', 'Flat', 'Lysi', 'linden', 'midnight'):
    for _name, _size in {
        'windows/login_sl_logo.png': (512, 288),
        'windows/login_sl_logo_horizontal.png': (512, 288),
        'windows/login_sl_logo_small.png': (256, 144),
        'windows/startup_logo.png': (1024, 1024),
        'icons/SL_Logo.png': (256, 256),
    }.items():
        PNG_OUTPUTS[f'skins/{_skin}/textures/{_name}'] = _size


def fit_artwork(artwork, size):
    canvas = Image.new('RGBA', size, (0, 0, 0, 0))
    # Padding in square icons prevents antialiased artwork touching the edges.
    padding = max(1, size[0] // 32) if size[0] == size[1] else 0
    bounds = (size[0] - 2 * padding, size[1] - 2 * padding)
    fitted = ImageOps.contain(artwork, bounds, Image.Resampling.LANCZOS)
    canvas.alpha_composite(fitted, ((size[0] - fitted.width) // 2, (size[1] - fitted.height) // 2))
    return canvas


def generate(source, viewer):
    original = source.read_bytes()
    image = Image.open(io.BytesIO(original)).convert('RGBA')
    box = image.getchannel('A').getbbox()
    if box is None:
        raise ValueError('The approved artwork is fully transparent')
    artwork = image.crop(box)
    outputs = {}
    for name, size in PNG_OUTPUTS.items():
        path = viewer / name
        path.parent.mkdir(parents=True, exist_ok=True)
        fit_artwork(artwork, size).save(path, format='PNG', optimize=True)
        outputs[name] = {'size': list(size), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    ico = viewer / 'res/tasia_icon.ico'
    ico.parent.mkdir(parents=True, exist_ok=True)
    fit_artwork(artwork, (256, 256)).save(ico, format='ICO', sizes=ICO_SIZES)
    outputs['res/tasia_icon.ico'] = {
        'sizes': [list(size) for size in ICO_SIZES],
        'sha256': hashlib.sha256(ico.read_bytes()).hexdigest(),
    }
    # Transport status is deliberately separate from decorative brand artwork.
    # There is no green/active icon until a real region transport supplies state.
    for name, color in [('quic_off', (170, 176, 186, 255)), ('quic_unavailable', (245, 186, 65, 255))]:
        status = Image.new('RGBA', (96, 96), (0, 0, 0, 0))
        draw = ImageDraw.Draw(status)
        draw.ellipse((14, 10, 76, 72), outline=color, width=8)
        draw.line((58, 58, 83, 83), fill=color, width=8)
        draw.line((10, 86, 85, 11), fill=(32, 35, 42, 255), width=15)
        draw.line((10, 86, 85, 11), fill=color, width=7)
        relative = f'skins/default/textures/icons/{name}.png'
        path = viewer / relative
        status.resize((24, 24), Image.Resampling.LANCZOS).save(path, format='PNG')
        outputs[relative] = {'size': [24, 24], 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    manifest = {
        'source': 'res/tasia_brand_source.png',
        'source_sha256': hashlib.sha256(original).hexdigest(),
        'source_size': list(image.size),
        'alpha_crop': list(box),
        'preserves_complete_artwork': True,
        'outputs': outputs,
    }
    (viewer / 'res/tasia_brand_assets.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=VIEWER / 'res/tasia_brand_source.png')
    parser.add_argument('--viewer', type=Path, default=VIEWER)
    args = parser.parse_args()
    result = generate(args.source, args.viewer)
    print(f"Generated {len(result['outputs'])} branding assets; complete artwork preserved.")
