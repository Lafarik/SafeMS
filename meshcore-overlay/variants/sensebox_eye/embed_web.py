"""PlatformIO pre-build: embed the bundled site in read-only program flash."""
from pathlib import Path
import gzip
import hashlib
import json

Import('env')
root = Path(env.subst('$PROJECT_DIR')) / 'variants/sensebox_eye/web'
manifest = json.loads((root / 'manifest.json').read_text(encoding='utf-8'))
generated = Path(env.subst('$BUILD_DIR')) / 'generated'
generated.mkdir(parents=True, exist_ok=True)
target = generated / 'SafeMSWebAssets.h'
chunks = ['#pragma once\n#include <Arduino.h>\nstruct SafeMSWebAsset { const char* url; const char* mime; const char* etag; const uint8_t* data; size_t size; const uint8_t* gzip; size_t gzipSize; };\n']
entries = []

def emit(name, data):
    chunks.append('static const uint8_t ' + name + '[] PROGMEM = {\n')
    for offset in range(0, len(data), 32):
        chunks.append(','.join(str(b) for b in data[offset:offset+32]) + ',\n')
    chunks.append('};\n')

for i, item in enumerate(manifest['files']):
    data = (root / item['file']).read_bytes()
    assert len(data) == item['size'] and hashlib.sha256(data).hexdigest() == item['sha256'], item['url']
    name = f'safeMSAsset{i}'
    emit(name, data)
    compressed = gzip.compress(data, compresslevel=9, mtime=0)
    use_gzip = not item['mime'].startswith(('application/pdf', 'image/png')) and len(compressed) + 64 < len(data)
    if use_gzip:
        emit(name + 'Gzip', compressed)
    entries.append('{' + ','.join([json.dumps(item['url']), json.dumps(item['mime']), json.dumps('"' + item['sha256'][:20] + '"'), name, str(len(data)), name + 'Gzip' if use_gzip else 'nullptr', str(len(compressed)) if use_gzip else '0']) + '}')
chunks.append('static const SafeMSWebAsset safeMSWebAssets[] = {\n' + ',\n'.join(entries) + '\n};\n')
content = ''.join(chunks)
if not target.exists() or target.read_text(encoding='ascii') != content:
    target.write_text(content, encoding='ascii')
env.Append(CPPPATH=[str(generated)])
print(f"Bundled notfall.ms PWA: {len(entries)} routes, source {manifest['revision'][:12]}")
