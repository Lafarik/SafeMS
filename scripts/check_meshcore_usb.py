"""Read MeshCore USB status without requesting radio transmissions or keys."""

import argparse
import json
import struct
import time

import serial


def query(port, payload, prefix, timeout=8):
    port.write(b"<" + struct.pack("<H", len(payload)) + payload)
    port.flush()
    data = bytearray()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        data.extend(port.read(port.in_waiting or 1))
        while data:
            start = data.find(b">")
            if start < 0:
                data.clear()
                break
            del data[:start]
            if len(data) < 3:
                break
            size = struct.unpack_from("<H", data, 1)[0]
            if not 1 <= size <= 256:
                del data[0]
                continue
            if len(data) < size + 3:
                break
            frame = bytes(data[3:3 + size])
            del data[:3 + size]
            if frame.startswith(prefix):
                return frame
    raise TimeoutError("No matching MeshCore response on " + port.port)


def text_field(value):
    return value.split(b"\0", 1)[0].decode("utf-8", errors="replace")


def core_status(port):
    frame = query(port, bytes([56, 0]), bytes([24, 0]))
    _, uptime, errors, queued = struct.unpack_from("<HIHB", frame, 2)
    return {"uptime_seconds": uptime, "error_flags": errors, "queued_packets": queued}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM7")
    args = parser.parse_args()
    port = serial.Serial(port=None, baudrate=115200, timeout=0.2, write_timeout=2)
    port.dtr = False
    port.rts = False
    port.port = args.port
    port.open()
    try:
        info = query(port, bytes([22, 13]), bytes([13]))
        own = query(port, bytes([1]) + bytes(7) + b"SafeMS USB check", bytes([5]))
        first = core_status(port)
        time.sleep(3)
        second = core_status(port)
        packets = query(port, bytes([56, 2]), bytes([24, 2]))
        counts = struct.unpack_from("<7I", packets, 2)
        result = {
            "port": args.port,
            "manufacturer": text_field(info[20:60]),
            "firmware": text_field(info[60:80]),
            "protocol_version": info[1],
            "client_repeat_enabled": bool(info[80]),
            "frequency_mhz": struct.unpack_from("<I", own, 48)[0] / 1000,
            "bandwidth_khz": struct.unpack_from("<I", own, 52)[0] / 1000,
            "spreading_factor": own[56],
            "coding_rate_denominator": own[57],
            "tx_power_dbm": struct.unpack_from("b", own, 2)[0],
            "first_status": first,
            "second_status": second,
            "packets_received": counts[0],
            "packets_sent": counts[1],
            "receive_errors": counts[6],
        }
        print(json.dumps(result, indent=2))
        if second["uptime_seconds"] <= first["uptime_seconds"]:
            raise RuntimeError("Uptime did not increase; check for restarts or a stalled clock")
        if second["error_flags"]:
            raise RuntimeError("MeshCore reports nonzero error flags")
    finally:
        port.close()


if __name__ == "__main__":
    main()
