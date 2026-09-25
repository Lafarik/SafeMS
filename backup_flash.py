"""Read-only, resumable 16 MiB backup. esptool verifies each chunk's MD5."""
from pathlib import Path
import hashlib
import time
from esptool.cmds import detect_chip

ROOT = Path(__file__).resolve().parent / "backups"
OUT = ROOT / "sensebox-eye-original-20260925.bin"
PART = OUT.with_suffix(".partial")
TOTAL = 0x1000000
CHUNK = 0x10000

def connect():
    esp = detect_chip("COM7", 115200, "default_reset")
    if esp.CHIP_NAME != "ESP32-S3":
        raise RuntimeError("Unexpected chip")
    esp = esp.run_stub()
    esp.change_baud(115200)
    esp.flash_set_parameters(TOTAL)
    return esp

esp = None
offset = PART.stat().st_size if PART.exists() else 0
assert offset % CHUNK == 0 and offset <= TOTAL
with PART.open("ab") as stream:
    while offset < TOTAL:
        for attempt in range(3):
            try:
                if esp is None:
                    esp = connect()
                data = esp.read_flash(offset, CHUNK)
                assert len(data) == CHUNK
                break
            except Exception as exc:
                print(f"Retry at 0x{offset:x}: {type(exc).__name__}: {exc}", flush=True)
                if esp:
                    esp._port.close()
                esp = None
                if attempt == 2:
                    raise
                time.sleep(1)
        stream.write(data)
        stream.flush()
        offset += CHUNK
        print(f"Verified {offset}/{TOTAL} bytes ({100*offset//TOTAL}%)", flush=True)

if esp is None:
    esp = connect()
local_md5 = hashlib.md5(PART.read_bytes()).hexdigest()
device_md5 = esp.flash_md5sum(0, TOTAL).lower()
esp._port.close()
if local_md5 != device_md5:
    raise RuntimeError(f"Full backup MD5 mismatch: {local_md5} != {device_md5}")
PART.replace(OUT)
sha = hashlib.sha256(OUT.read_bytes()).hexdigest()
OUT.with_suffix(".sha256").write_text(f"{sha}  {OUT.name}\n", encoding="ascii")
print(f"COMPLETE: 16 MiB verified against device MD5. SHA256 {sha}", flush=True)
