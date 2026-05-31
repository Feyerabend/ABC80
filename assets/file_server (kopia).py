import machine
import sdcard
import uos
import ujson
import utime
import network
import socket
import gc


WIFI_SSID     = "PicoFiles"
WIFI_PASSWORD = "picofiles"
SERVER_PORT   = 8080


class SimpleVFS:
    def __init__(self, mount_point = "/sd"):
        self.mount_point   = mount_point
        self.metadata_file = f"{mount_point}/.vfs_metadata.json"
        self.metadata      = self._load_metadata()

    def _load_metadata(self):
        try:
            with open(self.metadata_file, "r") as f:
                return ujson.load(f)
        except:
            return {}

    def _save_metadata(self):
        with open(self.metadata_file, "w") as f:
            ujson.dump(self.metadata, f)

    def _detect_file_type(self, filename):
        ext = filename.lower().split('.')[-1] if '.' in filename else ''
        if ext in ['txt', 'log', 'cfg', 'ini', 'json', 'py', 'md']:
            return 'text'
        elif ext in ['img', 'raw', 'rgb', 'rgb565', 'jpg', 'jpeg', 'png', 'bmp', 'gif']:
            return 'image'
        elif ext in ['bin', 'dat']:
            return 'binary'
        return 'unknown'

    def create_file(self, filename, data, permissions = "rw", file_type = None):
        full_path = f"{self.mount_point}/{filename}"
        with open(full_path, "wb") as f:
            f.write(data.encode() if isinstance(data, str) else data)

        if file_type is None:
            file_type = self._detect_file_type(filename)

        self.metadata[filename] = {
            "permissions": permissions,
            "created":     utime.time(),
            "size":        len(data),
            "type":        file_type,
        }
        self._save_metadata()
        print(f"Created: {filename} ({len(data)} bytes, type: {file_type})")

    def read_file(self, filename):
        self._check_readable(filename)
        with open(f"{self.mount_point}/{filename}", "rb") as f:
            return f.read()

    def read_file_chunked(self, filename, chunk_size = 1024):
        self._check_readable(filename)
        with open(f"{self.mount_point}/{filename}", "rb") as f:
            while True:
                chunk = f.read(chunk_size)
                if not chunk:
                    break
                yield chunk

    def _check_readable(self, filename):
        if filename not in self.metadata:
            raise FileNotFoundError(f"File not found: {filename}")
        if 'r' not in self.metadata[filename]["permissions"]:
            raise OSError(f"No read permission: {filename}")

    def list_files(self):
        return [
            {"name": f, "size": m["size"], "type": m.get("type", "unknown")}
            for f, m in self.metadata.items()
        ]

    def file_exists(self, filename):
        return filename in self.metadata

    def get_file_info(self, filename):
        return self.metadata.get(filename)


class RLECompressor:

    @staticmethod
    def compress(data, max_chunk = 4096):
        if not data:
            return bytes()

        compressed = bytearray()
        i = 0
        while i < len(data):
            current = data[i]
            count   = 1
            while (i + count < len(data)
                   and data[i + count] == current
                   and count < 255):
                count += 1
            compressed.append(count)
            compressed.append(current)
            i += count
            if len(compressed) % max_chunk == 0:
                gc.collect()

        return bytes(compressed)

    @staticmethod
    def decompress(data):
        decompressed = bytearray()
        i = 0
        while i + 1 < len(data):
            count = data[i]
            value = data[i + 1]
            decompressed.extend([value] * count)
            i += 2
        return bytes(decompressed)


class FileServer:
    RECV_BUFFER = 256
    SEND_BUFFER = 2048
    MAX_CHUNK   = 4096

    def __init__(self, vfs, ssid = WIFI_SSID, password = WIFI_PASSWORD, port = SERVER_PORT):
        self.vfs      = vfs
        self.ssid     = ssid
        self.password = password
        self.port     = port
        self.ap       = None
        self.sock     = None
        self.led      = None

        try:
            self.led = machine.Pin(25, machine.Pin.OUT)
        except Exception as e:
            print(f"LED not available: {e}")

    def setup_access_point(self):
        print("Setting up access point..")

        try:
            sta = network.WLAN(network.STA_IF)
            if sta.active():
                sta.active(False)
                utime.sleep(1)
        except Exception as e:
            print(f"STA disable warning: {e}")

        self.ap = network.WLAN(network.AP_IF)

        if self.ap.active():
            self.ap.active(False)
            utime.sleep(1)

        # Activate first, then configure — more compatible across builds
        self.ap.active(True)
        utime.sleep(2)

        # Try each param individually so an unsupported one doesn't abort the rest
        for key, val in [("essid", self.ssid), ("password", self.password), ("authmode", 3), ("channel", 6)]:
            try:
                self.ap.config(**{key: val})
            except Exception as e:
                print(f"Config param '{key}' not supported, skipping: {e}")

        deadline = utime.time() + 15
        while not self.ap.active() and utime.time() < deadline:
            utime.sleep(0.5)

        if not self.ap.active():
            raise RuntimeError("Access point failed to start")

        utime.sleep(2)

        cfg = self.ap.ifconfig()
        print(f"AP up  |  SSID: {self.ssid}  |  IP: {cfg[0]}  |  password: {self.password}")

        if self.led:
            self.led.value(1)

    def setup_tcp_server(self):
        addr = socket.getaddrinfo('0.0.0.0', self.port)[0][-1]

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(addr)
        self.sock.listen(3)

        # Non-infinite timeout so KeyboardInterrupt can escape accept()
        self.sock.settimeout(5.0)

        print(f"TCP server listening on port {self.port}")

    def _recv_line(self, conn):
        """Read bytes until newline — macOS may split the request across packets."""
        buf = b""
        while True:
            ch = conn.recv(1)
            if not ch or ch == b'\n':
                break
            buf += ch
        return buf.decode().strip()

    def _send_all(self, conn, data):
        total = 0
        while total < len(data):
            end  = min(total + self.SEND_BUFFER, len(data))
            sent = conn.send(data[total:end])
            if sent == 0:
                raise RuntimeError("Socket closed mid-send")
            total += sent
        return total

    def _send_error(self, conn, msg):
        try:
            conn.send(f"ERROR|{msg}\n".encode())
        except:
            pass

    def handle_list(self, conn):
        files   = self.vfs.list_files()
        payload = ujson.dumps(files).encode()
        conn.send(f"OK|{len(payload)}|\n".encode())
        self._send_all(conn, payload)
        print(f"LIST sent ({len(files)} files)")

    def handle_info(self, conn, filename):
        info = self.vfs.get_file_info(filename)
        if not info:
            self._send_error(conn, "File not found")
            return
        payload = ujson.dumps(info).encode()
        conn.send(f"OK|{len(payload)}|\n".encode())
        self._send_all(conn, payload)
        print(f"INFO sent for {filename}")

    def handle_get(self, conn, filename, compress = True):
        if not self.vfs.file_exists(filename):
            self._send_error(conn, "File not found")
            return

        info      = self.vfs.get_file_info(filename)
        file_type = info.get('type', 'unknown')

        gc.collect()
        raw = self.vfs.read_file(filename)

        if compress and len(raw) > 512:
            gc.collect()
            payload = RLECompressor.compress(raw, self.MAX_CHUNK)
            print(f"Compressed {len(raw)} -> {len(payload)} bytes")
        else:
            payload = raw

        header = f"OK|{len(raw)}|{len(payload)}|{file_type}|\n"
        conn.send(header.encode())
        self._send_all(conn, payload)
        gc.collect()
        print(f"GET done: {filename}  free mem: {gc.mem_free()}")

    def handle_client(self, conn, addr):
        print(f"Client: {addr}")
        conn.settimeout(30.0)

        try:
            request = self._recv_line(conn)
            print(f"Request: {request!r}")

            parts = request.split("|")
            cmd   = parts[0].upper()

            if cmd == "LIST":
                self.handle_list(conn)

            elif cmd == "INFO" and len(parts) >= 2:
                self.handle_info(conn, parts[1])

            elif cmd == "GET" and len(parts) >= 2:
                compress = (len(parts) < 3 or parts[2].lower() != 'raw')
                self.handle_get(conn, parts[1], compress)

            else:
                self._send_error(conn, "Unknown command. Use LIST / INFO|file / GET|file")

        except OSError as e:
            print(f"Client OS error: {e}")
        except Exception as e:
            print(f"Client error: {e}")
            import sys
            sys.print_exception(e)
        finally:
            try:
                conn.close()
            except:
                pass

    def run(self):
        self.setup_access_point()
        self.setup_tcp_server()

        print(f"\nReady.  Connect Mac to WiFi '{self.ssid}' (password: {self.password})")
        print(f"Then:   nc 192.168.4.1 {self.port}")

        try:
            while True:
                try:
                    conn, addr = self.sock.accept()
                    self.handle_client(conn, addr)
                    gc.collect()
                except OSError:
                    # accept() timed out — loop so KeyboardInterrupt stays responsive
                    pass
        except KeyboardInterrupt:
            print("Shutdown requested")
        finally:
            self.cleanup()

    def cleanup(self):
        if self.sock:
            try:
                self.sock.close()
            except:
                pass
        if self.ap:
            self.ap.active(False)
        if self.led:
            self.led.value(0)
        print("Cleanup done")


def main():
    import sys
    print(f"Platform: {sys.platform}  |  Free mem: {gc.mem_free()} bytes")

    try:
        cs  = machine.Pin(1, machine.Pin.OUT)
        spi = machine.SPI(
            0,
            baudrate  = 1000000,
            polarity  = 0,
            phase     = 0,
            bits      = 8,
            firstbit  = machine.SPI.MSB,
            sck       = machine.Pin(2),
            mosi      = machine.Pin(3),
            miso      = machine.Pin(4),
        )
        sd      = sdcard.SDCard(spi, cs)
        vfs_fat = uos.VfsFat(sd)
        uos.mount(vfs_fat, "/sd")
        print("SD card mounted at /sd")
    except Exception as e:
        import sys as _sys
        print(f"SD mount failed: {e}")
        _sys.print_exception(e)
        raise

    vfs   = SimpleVFS("/sd")
    files = vfs.list_files()

    if not files:
        print("No files found, creating test files..")
        vfs.create_file(
            "readme.txt",
            "Hello from Pico File Server!\nTest line 2\nTest line 3",
            file_type = 'text',
        )
        img = bytearray(320 * 240 * 2)
        for i in range(len(img)):
            img[i] = i % 256
        vfs.create_file("test.img", bytes(img), file_type = 'image')
    else:
        print(f"Existing files ({len(files)}):")
        for f in files:
            print(f"  {f['name']}  {f['size']} bytes  [{f['type']}]")

    FileServer(vfs).run()


if __name__ == "__main__":
    main()