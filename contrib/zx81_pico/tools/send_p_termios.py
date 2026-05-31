#!/usr/bin/env python3
"""
send_p_termios.py  —  Send a ZX81 .p file to the Pico emulator over USB-CDC serial.
No external dependencies (uses built-in termios/os modules).

Usage
-----
    python3 tools/send_p_termios.py <port> <file.p>

    macOS:  python3 tools/send_p_termios.py /dev/tty.usbmodem* game.p
    Linux:  python3 tools/send_p_termios.py /dev/ttyACM0         game.p

Protocol
--------
    1. Script sends Ctrl+L  (0x0C) to trigger load mode on the Pico.
    2. Pico responds with "LOAD: waiting..." line.
    3. Script sends 2-byte little-endian file length, then raw .p bytes.
    4. Pico responds with "OK  (...)" on success.
"""

import sys
import os
import glob
import struct
import termios
import time


def open_serial(port, baud=115200):
    # O_NONBLOCK required on macOS to open tty without carrier-detect wait
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    import fcntl
    flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, flags & ~os.O_NONBLOCK)

    attrs = termios.tcgetattr(fd)
    # iflag, oflag, cflag, lflag
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0
    speed = getattr(termios, f"B{baud}")
    attrs[4] = speed   # ispeed
    attrs[5] = speed   # ospeed
    attrs[6][termios.VMIN]  = 0
    attrs[6][termios.VTIME] = 10   # 1 s read timeout (units: 0.1 s)
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIOFLUSH)
    return fd


def readline(fd, timeout=6.0):
    """Read bytes until '\n' or timeout; return decoded string."""
    buf = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        chunk = os.read(fd, 1)
        if chunk:
            buf += chunk
            if chunk == b"\n":
                break
    return buf.decode(errors="replace").strip()


def main():
    if len(sys.argv) != 3:
        sys.exit(f"Usage: {sys.argv[0]} <port> <file.p>")

    port_glob, path = sys.argv[1], sys.argv[2]

    matches = glob.glob(port_glob)
    port = matches[0] if matches else port_glob

    data = open(path, "rb").read()
    size = len(data)
    if size == 0 or size > 32759:
        sys.exit(f"File size {size} out of range (1–32759 bytes)")

    print(f"Port : {port}")
    print(f"File : {path}  ({size} bytes)")

    fd = open_serial(port)
    try:
        # Flush any pending input
        termios.tcflush(fd, termios.TCIFLUSH)

        # Send Ctrl+L to trigger load mode
        os.write(fd, b"\x0c")
        print("Sent Ctrl+L, waiting for Pico...")

        # Wait for "LOAD: waiting..." prompt
        deadline = time.time() + 5
        got_prompt = False
        while time.time() < deadline:
            line = readline(fd, timeout=1.0)
            if line:
                print(f"  Pico: {line}")
            if "waiting" in line.lower():
                got_prompt = True
                break
        if not got_prompt:
            sys.exit("Timed out waiting for load prompt")

        # Send 2-byte length header + raw data
        os.write(fd, struct.pack("<H", size))
        os.write(fd, data)
        print(f"Sent {size + 2} bytes, waiting for OK...")

        # Wait for "OK" response
        deadline = time.time() + 10
        while time.time() < deadline:
            line = readline(fd, timeout=1.0)
            if line:
                print(f"  Pico: {line}")
            if line.startswith("OK"):
                print("Done.")
                return
        sys.exit("Timed out waiting for OK")

    finally:
        os.close(fd)


if __name__ == "__main__":
    main()
