"""Read the live kiosk feed/self-test via MeshCore USB; optionally synchronize UTC."""
import argparse
import json
import struct
import time
import serial
from check_meshcore_usb import query


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument('--port', default='COM7')
    args.add_argument('--sync-time', action='store_true')
    options = args.parse_args()
    port = serial.Serial(port=None, baudrate=115200, timeout=.2, write_timeout=2)
    port.dtr = False
    port.rts = False
    port.port = options.port
    port.open()
    try:
        if options.sync_time:
            query(port, b'\x70' + struct.pack('<I', int(time.time())), b'\x00')
            print('Kiosk UTC synchronized from host; MeshCore RTC/settings unchanged.')
        failures = struct.unpack_from('<I', query(port, b'\x72', b'\x72'), 1)[0]
        if failures:
            raise RuntimeError('Feed self-test failed, bitmap: ' + hex(failures))
        print('19 on-device parser/JSON/history checks passed.')
        # A live arrival can change the snapshot between chunks; retry a torn read.
        for attempt in range(3):
            content = bytearray()
            while True:
                offset = len(content)
                frame = query(port, b'\x71' + struct.pack('<H', offset), b'\x71')
                received_offset, total = struct.unpack_from('<HH', frame, 1)
                assert received_offset == offset and total <= 16384
                content.extend(frame[5:])
                if len(content) >= total:
                    break
                if len(frame) == 5:
                    break
            try:
                feed = json.loads(content)
                print(json.dumps(feed, ensure_ascii=True, indent=2))
                break
            except (ValueError, UnicodeError):
                if attempt == 2:
                    raise
    finally:
        port.close()


if __name__ == '__main__':
    main()
