#!/usr/bin/env python3
"""
send_p.py  —  Send a ZX81 .p file to the Pico emulator over USB-CDC serial.

Usage
-----
    python3 tools/send_p.py <port> <file.p>

    macOS:  python3 tools/send_p.py /dev/tty.usbmodem*  game.p
    Linux:  python3 tools/send_p.py /dev/ttyACM0         game.p

Requirements
------------
    pip install pyserial

Protocol
--------
    1. Script sends Ctrl+L  (0x0C) to trigger load mode on the Pico.
    2. Pico responds with "LOAD: waiting..." line.
    3. Script sends 2-byte little-endian file length, then raw .p bytes.
    4. Pico responds with "OK  (...)" on success.
"""

import sys
import time
import struct

try:
    import serial
except ImportError:
    sys.exit("pyserial not found — run: pip install pyserial")


def main():
    if len(sys.argv) != 3:
        sys.exit(f"Usage: {sys.argv[0]} <port> <file.p>")

    port_glob, path = sys.argv[1], sys.argv[2]

    # Expand shell globs (e.g. /dev/tty.usbmodem*)
    import glob
    matches = glob.glob(port_glob)
    port = matches[0] if matches else port_glob

    data = open(path, "rb").read()
    size = len(data)
    if size == 0 or size > 32759:
        sys.exit(f"File size {size} out of range (1–32759 bytes)")

    print(f"Port : {port}")
    print(f"File : {path}  ({size} bytes)")

    with serial.Serial(port, 115200, timeout=6) as s:
        # Flush any pending input
        s.reset_input_buffer()

        # Send Ctrl+L to trigger load mode
        s.write(b"\x0c")
        print("Sent Ctrl+L, waiting for Pico...")

        # Wait for the "LOAD: waiting..." prompt
        deadline = time.time() + 5
        while time.time() < deadline:
            line = s.readline().decode(errors="replace").strip()
            if line:
                print(f"  Pico: {line}")
            if "waiting" in line.lower():
                break
        else:
            sys.exit("Timed out waiting for load prompt")

        # Send 2-byte length header + raw data
        s.write(struct.pack("<H", size))
        s.write(data)
        print(f"Sent {size + 2} bytes, waiting for OK...")

        # Wait for "OK" response
        deadline = time.time() + 10
        while time.time() < deadline:
            line = s.readline().decode(errors="replace").strip()
            if line:
                print(f"  Pico: {line}")
            if line.startswith("OK"):
                print("Done.")
                return
        sys.exit("Timed out waiting for OK")


if __name__ == "__main__":
    main()
