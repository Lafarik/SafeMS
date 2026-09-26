"""Verify the bundled MeshCore source. No clone or download is performed."""
from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "firmware"
manifest_path = DEST / "SAFEMS_SOURCE_MANIFEST.json"
if not manifest_path.is_file():
    raise SystemExit("Missing firmware/SAFEMS_SOURCE_MANIFEST.json. Download the complete repository.")

manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
errors = []
for entry in manifest["files"]:
    path = DEST / entry["path"]
    if not path.is_file():
        errors.append("Missing: " + entry["path"])
    elif hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
        errors.append("Changed: " + entry["path"])
if errors:
    raise SystemExit("Firmware source verification failed:\n" + "\n".join(errors))

print(f"Verified {len(manifest['files'])} bundled source files in {DEST}")
print(f"Upstream revision: {manifest['upstream_revision']}")
print("Bluetooth LE + USB + WLAN: " + manifest["default_environment"])
print("No MeshCore source download required. PlatformIO may download build dependencies.")
