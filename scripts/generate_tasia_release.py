#!/usr/bin/env python3
"""Generate the release artifacts the launcher consumes.

build_data.json  - release metadata (version, channel, file base URL)
mainifest.list   - one line per published file: <sha256> <size> <relative path>
news.txt         - plain UTF-8 announcement, never HTML

The manifest only lists files that are actually published, so the launcher can
verify what it downloads. Nothing here is a placeholder.
"""
import hashlib
import json
from pathlib import Path

PACKAGE = Path('/tmp/opencode/release-package')
OUT = Path('/tmp/opencode/release-artifacts')
VERSION = '26.9.4015'
CHANNEL = 'Tasia'
BASE_URL = 'https://api.tasiaviewer.work/viewer/updates/files'
BUILD_DATA_URL = 'https://api.tasiaviewer.work/viewer/updates/build_data.json'
MANIFEST_URL = 'https://api.tasiaviewer.work/viewer/updates/files/mainifest.list'
NEWS_URL = 'https://api.tasiaviewer.work/viewer/updates/news.txt'

# Files that make up this release, relative to the package root.
RELEASE_FILES = [
    'TasiaViewer.exe',
    'app_settings/settings.xml',
    'app_settings/tasia_default_grids.xml',
    'skins/default/xui/en/floater_preferences.xml',
    'skins/default/xui/pl/floater_preferences.xml',
    'skins/default/xui/en/panel_preferences_tasia.xml',
    'skins/default/xui/pl/panel_preferences_tasia.xml',
    'skins/default/xui/en/panel_status_bar.xml',
    'skins/default/xui/pl/panel_status_bar.xml',
]


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b''):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    missing = [rel for rel in RELEASE_FILES if not (PACKAGE / rel).is_file()]
    if missing:
        raise SystemExit('missing release files: ' + ', '.join(missing))

    lines = []
    files = []
    for rel in RELEASE_FILES:
        path = PACKAGE / rel
        digest = sha256(path)
        size = path.stat().st_size
        lines.append(f'{digest} {size} {rel}')
        files.append({'path': rel, 'sha256': digest, 'size': size,
                      'url': f'{BASE_URL}/{rel}'})
    (OUT / 'mainifest.list').write_text(
        '# Tasia Viewer release manifest\n# <sha256> <size> <path>\n'
        + '\n'.join(lines) + '\n', encoding='utf-8')

    build_data = {
        'version': VERSION,
        'Version': VERSION,
        'channel': CHANNEL,
        'Channel': CHANNEL,
        'platform': 'win',
        'Type': 'viewer',
        'AppName': 'TasiaViewer',
        'ProductName': 'Tasia Viewer DX',
        'files_base_url': BASE_URL,
        'manifest_url': MANIFEST_URL,
        'news_url': NEWS_URL,
        'files': files,
    }
    (OUT / 'build_data.json').write_text(json.dumps(build_data, indent=2) + '\n', encoding='utf-8')

    news = (
        'Tasia Viewer DX — release notes\n'
        '\n'
        'This release adds the native 3D globe map, the Tasia - Network preferences tab\n'
        'with QUIC and TLS options, the full OpenSim grid list, and the grid plus UUID\n'
        'cosmetic identity catalog.\n'
        '\n'
        'The 3D globe: open World Map and switch on the globe. Drag to rotate, use the\n'
        'mouse wheel to zoom, and select a region to fill the destination. Teleport stays\n'
        'an explicit action.\n'
        '\n'
        'Network settings live in Preferences under "Tasia - Network". QUIC is used when a\n'
        'simulator advertises it, with an automatic fallback to the standard UDP transport.\n'
        '\n'
        'Media: GIPHY and KLIPY are supplied by the launcher at start-up, so the pickers\n'
        'work without any manual key entry.\n'
    )
    (OUT / 'news.txt').write_text(news, encoding='utf-8')

    receipt = {
        'version': VERSION,
        'files': len(RELEASE_FILES),
        'total_bytes': sum((PACKAGE / rel).stat().st_size for rel in RELEASE_FILES),
        'build_data_sha256': sha256(OUT / 'build_data.json'),
        'manifest_sha256': sha256(OUT / 'mainifest.list'),
        'news_sha256': sha256(OUT / 'news.txt'),
    }
    (OUT / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
