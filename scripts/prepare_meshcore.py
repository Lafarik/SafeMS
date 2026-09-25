"""Fetch pinned upstream source and install the SafeMS board overlay."""
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "MeshCore"
REV = "e94125987ed87497e706a0b54d1e80c709343980"
git = shutil.which("git")
if not git:
    raise SystemExit("Git must be installed and available on PATH.")

def run(*args):
    return subprocess.check_output([git, *args], text=True).strip()

if not DEST.exists():
    subprocess.run([git, "clone", "https://github.com/meshcore-dev/MeshCore.git", str(DEST)], check=True)
    subprocess.run([git, "-C", str(DEST), "checkout", REV], check=True)
elif run("-C", str(DEST), "rev-parse", "HEAD") != REV:
    raise SystemExit("Existing MeshCore checkout uses another commit; refusing to change it.")

source = ROOT / "meshcore-overlay" / "variants" / "sensebox_eye"
target = DEST / "variants" / "sensebox_eye"
target.mkdir(parents=True, exist_ok=True)
for item in source.iterdir():
    if item.is_file():
        existing = target / item.name
        if existing.exists() and existing.read_bytes() != item.read_bytes():
            raise SystemExit(f"Existing local file differs: {existing}. Review it before replacing.")
        shutil.copy2(item, existing)
print(f"Prepared {DEST} at {REV}")
