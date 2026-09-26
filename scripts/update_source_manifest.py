"""Refresh the bundled firmware manifest after intentional source changes."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
firmware = root / 'firmware'
manifest_file = firmware / 'SAFEMS_SOURCE_MANIFEST.json'
manifest = json.loads(manifest_file.read_text(encoding='utf-8'))
previous_modes = {e['path']: e['mode'] for e in manifest['files']}
paths = {p for p in previous_modes if not p.startswith('variants/sensebox_eye/')}
paths.update(p.relative_to(firmware).as_posix() for p in (firmware / 'variants/sensebox_eye').rglob('*') if p.is_file() and '__pycache__' not in p.parts)
files = []
for relative in sorted(paths):
    path = firmware / relative
    data = path.read_bytes()
    if path.suffix not in ('.dat', '.gz'):
        try:
            normalized = data.decode('utf-8').replace('\r\n', '\n').encode('utf-8')
            if normalized != data: path.write_bytes(normalized)
            data = normalized
        except UnicodeDecodeError:
            pass
    files.append({'path': relative, 'mode': previous_modes.get(relative, '100644'), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
manifest['files'] = files
manifest['modified_upstream_paths'] = ['examples/companion_radio/main.cpp', 'examples/companion_radio/MyMesh.cpp', 'examples/companion_radio/ui-new/UITask.cpp', 'platformio.ini', 'src/helpers/ui/SSD1306Display.cpp']
manifest['integrated_display_branch'] = 'https://github.com/Lafarik/SafeMS/commit/802d6008aefe919588147c4ee70aecda692df3fe'
manifest_file.write_bytes((json.dumps(manifest, indent=2) + '\n').encode('utf-8'))
print(f'Updated {len(files)} firmware file checksums')
