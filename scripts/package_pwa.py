"""Bundle a built notfall-ms/pwa site for the firmware without touching device storage."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', required=True, type=Path)
    parser.add_argument('--git', default='git')
    parser.add_argument('--revision', help='Pinned revision when building from the included source archive')
    args = parser.parse_args()
    repo = args.repo.resolve()
    site = repo / '_site'
    root = Path(__file__).resolve().parents[1]
    dest = root / 'firmware/variants/sensebox_eye/web'
    dest.mkdir(parents=True, exist_ok=True)
    revision = args.revision or subprocess.check_output([args.git, 'rev-parse', 'HEAD'], cwd=repo).decode().strip()
    metadata = json.loads((site / 'sw.js').read_text(encoding='utf-8').split('\n', 1)[0].removeprefix('const PWA = ').removesuffix(';'))
    info = {k: metadata[k] for k in ('version', 'build', 'cacheName', 'documents')}
    info.update({'source': 'https://github.com/notfall-ms/pwa', 'revision': revision, 'ssid': 'notfall.ms INFO'})
    runtime = {}
    for path in site.rglob('*'):
        if path.is_file() and path.suffix != '.map' and path.name != 'manifest.json':
            runtime['/' + path.relative_to(site).as_posix()] = path.read_bytes()
    # Windows cannot hold the upstream LOGO.svg and logo.svg side by side.
    # Keep both URL spellings using content-addressed on-disk filenames.
    for name in ('LOGO.svg', 'logo.svg'):
        if args.revision:
            local_name = 'logo-favicon.svg' if name == 'logo.svg' else name
            runtime['/assets/' + name] = (repo / 'src/frontend/assets' / local_name).read_bytes()
        else:
            runtime['/assets/' + name] = subprocess.check_output([args.git, 'show', revision + ':src/frontend/assets/' + name], cwd=repo)
    runtime['/assets/logo-favicon.svg'] = runtime['/assets/logo.svg']
    runtime['/kiosk-info.json'] = (json.dumps(info, ensure_ascii=False) + '\n').encode('utf-8')
    runtime['/LICENSE-pwa.txt'] = (repo / 'LICENSE').read_bytes()
    assert '/index.html' in runtime
    assert all(url in runtime for url in metadata['documents'])
    assert all(url == '/' or url in runtime for url in metadata['precache']), 'Missing precache resource'
    types = {'.html': 'text/html; charset=utf-8', '.js': 'application/javascript', '.css': 'text/css', '.svg': 'image/svg+xml', '.png': 'image/png', '.json': 'application/json', '.webmanifest': 'application/manifest+json', '.pdf': 'application/pdf', '.txt': 'text/plain; charset=utf-8', '.md': 'text/plain; charset=utf-8', '.ico': 'image/x-icon'}
    entries = []
    for url, data in sorted(runtime.items()):
        sha = hashlib.sha256(data).hexdigest()
        filename = sha + '.dat'
        (dest / filename).write_bytes(data)
        entries.append({'url': url, 'file': filename, 'size': len(data), 'sha256': sha, 'mime': types.get(Path(url).suffix, 'application/octet-stream')})
    manifest = {'source': info['source'], 'revision': revision, 'version': metadata['version'], 'build': metadata['build'], 'files': entries}
    (dest / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'revision': revision, 'routes': len(entries), 'uncompressed_bytes': sum(e['size'] for e in entries)}, indent=2))

if __name__ == '__main__':
    main()
