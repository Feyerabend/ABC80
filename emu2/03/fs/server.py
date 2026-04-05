# server.py - HTTP file server for PicoFS
#
# Routes
#   GET  /ping                         --> "PicoFS OK  ram=N B"
#   GET  /ls?path=/                    --> JSON [{n,t,s}]  (n=name, t=f|d, s=size)
#   GET  /disk                         --> JSON {t,f}      (total/free bytes)
#   GET  /read?path=f&off=0&n=2048     --> binary chunk (≤ CHUNK bytes)
#   POST /write?path=f&off=0           --> body ≤ CHUNK --> "OK"
#   POST /mkdir?path=d                 --> "OK"
#   POST /rename?src=old&dst=new       --> "OK"
#   DELETE /rm?path=f                  --> "OK"
#   OPTIONS *                          --> CORS preflight
#
# Every request and response body is less or equal to CHUNK bytes.
# Connection: close on every response - simple and reliable on lwIP.
# Server socket timeout = 1 s so the watchdog is fed on every loop tick.

import socket, gc, ujson

CHUNK    = 2048   # max bytes per read / write body
HDRMAX   = 2048   # max request header size accepted
TOUT     = 8      # normal socket timeout (s)
WTOUT    = 6      # per-recv() timeout while reading a write body (s)
RAM_WARN = 20000  # log warning when free heap falls below this (bytes)

CORS = (
    b'Access-Control-Allow-Origin: *\r\n'
    b'Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n'
    b'Access-Control-Allow-Headers: Content-Type\r\n'
)

#
# Network helpers

def _send(conn, data):
    if isinstance(data, str):
        data = data.encode()
    conn.write(data)   # write() == sendall on MicroPython blocking sockets

def _resp(conn, status, ctype, body=b''):
    if isinstance(body, str):
        body = body.encode()
    _send(conn,
          'HTTP/1.1 {}\r\nContent-Type: {}\r\nContent-Length: {}\r\n'
          .format(status, ctype, len(body)).encode()
          + CORS + b'Connection: close\r\n\r\n')
    if body:
        _send(conn, body)

def _ok(conn, msg='OK'):
    _resp(conn, '200 OK', 'text/plain', msg)

def _err(conn, code, msg):
    _resp(conn, code, 'text/plain', str(msg)[:120])

def _json(conn, obj):
    _resp(conn, '200 OK', 'application/json', ujson.dumps(obj))

def _drain(conn, n):
    """Discard n unread body bytes so the socket stays clean for the next request."""
    while n > 0:
        try:
            got = conn.recv(min(256, n))
            if not got:
                break
            n -= len(got)
        except OSError:
            break

#
# Request parsing helpers

def _url_dec(s):
    if not isinstance(s, str):
        return ''
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c == '%' and i + 2 < len(s):
            try:
                out.append(chr(int(s[i+1:i+3], 16)))
                i += 3
                continue
            except (ValueError, TypeError):
                pass
        elif c == '+':
            out.append(' ')
            i += 1
            continue
        out.append(c)
        i += 1
    return ''.join(out)

def _parse_qs(qs):
    p = {}
    for kv in (qs or '').split('&'):
        if '=' in kv:
            k, v = kv.split('=', 1)
            p[_url_dec(k)] = _url_dec(v)
    return p

def _safe_path(raw):
    """
    Sanitise a path parameter: decode, strip slashes, reject '..' traversal.
    Returns clean relative path string, or None if invalid.
    """
    path = _url_dec(raw).strip().strip('/')
    if not path:
        return None
    for part in path.split('/'):
        if part in ('..', '.', ''):
            return None
    return path

def _int_param(params, key, default=0, lo=0, hi=None):
    """Parse and range-check an integer query param. Returns (value, err_str)."""
    raw = params.get(key)
    if raw is None:
        return default, None
    try:
        v = int(raw)
    except (ValueError, TypeError):
        return None, 'bad param: ' + key
    if v < lo:
        return None, '{} must be >= {}'.format(key, lo)
    if hi is not None and v > hi:
        return None, '{} must be <= {}'.format(key, hi)
    return v, None

def _read_header(conn):
    """
    Read HTTP request header (up to HDRMAX bytes).
    Returns (header_bytes, body_prefix_bytes), or (None, None) on failure.
    """
    buf = b''
    while len(buf) <= HDRMAX:
        try:
            chunk = conn.recv(256)
        except OSError:
            return None, None
        if not chunk:
            return None, None
        buf += chunk
        if b'\r\n\r\n' in buf:
            break
    sep = buf.find(b'\r\n\r\n')
    if sep < 0:
        return None, None
    return buf[:sep], buf[sep+4:]

#

class HTTPServer:

    def __init__(self, fs, wdt=None, led=None):
        self.fs  = fs    # FileServer instance, or None (no SD card)
        self.wdt = wdt   # machine.WDT, or None
        self.led = led   # machine.Pin LED, or None

    # Accept loop

    def serve(self, port=80):
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        except Exception:
            pass
        srv.bind(socket.getaddrinfo('0.0.0.0', port, 0, socket.SOCK_STREAM)[0][-1])
        srv.listen(5)
        srv.settimeout(1)   # 1 s timeout --> WDT fed every tick, Ctrl-C works
        print('listening on port', port)

        while True:
            if self.wdt:
                self.wdt.feed()   # fed here on every 1-second tick

            conn = None
            try:
                conn, addr = srv.accept()
            except OSError:
                continue   # normal 1-second timeout
            except Exception as e:
                print('accept err:', e)
                continue

            if self.led:
                self.led.on()
            try:
                conn.settimeout(TOUT)
                self._handle(conn, addr[0])
            except Exception as e:
                print('handler err:', e)
            finally:
                if self.led:
                    self.led.off()
                try:
                    conn.close()
                except Exception:
                    pass
                gc.collect()
                free = gc.mem_free()
                if free < RAM_WARN:
                    print('low RAM: {} B'.format(free))

    # Request handler

    def _handle(self, conn, ip):
        if self.wdt:
            self.wdt.feed()

        hdr, body_head = _read_header(conn)
        if hdr is None:
            return

        # Parse request line
        try:
            first = hdr.decode('utf-8', 'replace').split('\r\n')[0]
            parts = first.split(' ', 2)
            if len(parts) < 2:
                _err(conn, '400 Bad Request', 'bad request line')
                return
            method   = parts[0].upper()
            raw_path = parts[1]
        except Exception:
            _err(conn, '400 Bad Request', 'unreadable header')
            return

        # Parse Content-Length
        clen = 0
        try:
            for line in hdr.decode('utf-8', 'replace').split('\r\n')[1:]:
                if line.lower().startswith('content-length:'):
                    clen = max(0, int(line.split(':', 1)[1].strip()))
                    break
        except Exception:
            pass

        # Split path and query string, compute remaining body bytes
        qs = ''
        if '?' in raw_path:
            raw_path, qs = raw_path.split('?', 1)
        path   = _url_dec(raw_path)
        params = _parse_qs(qs)
        rem    = max(0, clen - len(body_head))   # unread body bytes in socket

        print('{} {} {}'.format(method, path, ip))

        if method == 'OPTIONS':
            _resp(conn, '204 No Content', 'text/plain')
            return

        if method == 'GET':
            if path in ('/', '/ping'):
                _ok(conn, 'PicoFS OK  ram={} B\n'.format(gc.mem_free()))
            elif path == '/ls':
                self._ls(conn, params)
            elif path == '/disk':
                self._disk(conn)
            elif path == '/read':
                self._read(conn, params)
            else:
                _drain(conn, rem)
                _err(conn, '404 Not Found', path)

        elif method == 'POST':
            if path == '/write':
                self._write(conn, params, clen, body_head, rem)
            elif path == '/mkdir':
                _drain(conn, rem)
                self._mkdir(conn, params)
            elif path == '/rename':
                _drain(conn, rem)
                self._rename(conn, params)
            else:
                _drain(conn, rem)
                _err(conn, '404 Not Found', path)

        elif method == 'DELETE':
            if path == '/rm':
                self._rm(conn, params)
            else:
                _err(conn, '404 Not Found', path)

        else:
            _drain(conn, rem)
            _err(conn, '405 Method Not Allowed', method)

    # Helpers

    def _need_sd(self, conn, rem=0):
        """Send 503 and drain body if no SD card. Returns True if SD missing."""
        if self.fs:
            return False
        _drain(conn, rem)
        _err(conn, '503 Service Unavailable', 'no SD card')
        return True

    # Route handlers

    def _ls(self, conn, params):
        if self._need_sd(conn): return
        raw   = params.get('path', '/')
        lpath = '/' if raw in ('', '/') else raw
        try:
            entries = self.fs.list(lpath)
            _json(conn, [{'n': e['name'].split('/')[-1],
                          't': 'd' if e['type'] == 'dir' else 'f',
                          's': e.get('size', 0)}
                         for e in entries])
        except Exception as e:
            _err(conn, '500 Internal Server Error', e)

    def _disk(self, conn):
        if self._need_sd(conn): return
        try:
            d = self.fs.disk_info()
            _json(conn, {'t': d['total'], 'f': d['free']})
        except Exception as e:
            _err(conn, '500 Internal Server Error', e)

    def _read(self, conn, params):
        if self._need_sd(conn): return

        path = _safe_path(params.get('path', ''))
        if not path:
            _err(conn, '400 Bad Request', 'missing/invalid path'); return

        off, err = _int_param(params, 'off', 0, lo=0)
        if err: _err(conn, '400 Bad Request', err); return

        n, err = _int_param(params, 'n', CHUNK, lo=1, hi=CHUNK)
        if err: _err(conn, '400 Bad Request', err); return

        try:
            if self.wdt:
                self.wdt.feed()
            with open(self.fs._full(path), 'rb') as f:
                f.seek(off)
                data = f.read(n)
            if self.wdt:
                self.wdt.feed()
            _resp(conn, '200 OK', 'application/octet-stream', data)
        except OSError as e:
            _err(conn, '404 Not Found', '{}: {}'.format(path, e))
        except Exception as e:
            _err(conn, '500 Internal Server Error', e)

    def _write(self, conn, params, clen, body_head, rem):
        if self._need_sd(conn, rem): return

        path = _safe_path(params.get('path', ''))
        if not path:
            _drain(conn, rem); _err(conn, '400 Bad Request', 'missing/invalid path'); return

        if clen > CHUNK:
            _drain(conn, rem); _err(conn, '400 Bad Request', 'body exceeds CHUNK'); return

        off, err = _int_param(params, 'off', 0, lo=0)
        if err:
            _drain(conn, rem); _err(conn, '400 Bad Request', err); return

        # Read the complete body - each recv() has its own timeout so the
        # watchdog stays fed between calls.
        body = body_head
        while len(body) < clen:
            if self.wdt:
                self.wdt.feed()
            conn.settimeout(WTOUT)
            try:
                got = conn.recv(min(512, clen - len(body)))
            except OSError as e:
                print('write recv err:', e)
                _err(conn, '500 Internal Server Error', 'body recv failed')
                return
            if not got:
                print('write: connection closed early')
                _err(conn, '500 Internal Server Error', 'body truncated')
                return
            body += got
        conn.settimeout(TOUT)

        if len(body) != clen:
            _err(conn, '400 Bad Request', 'body length mismatch'); return

        try:
            if self.wdt:
                self.wdt.feed()
            mode = 'wb' if off == 0 else 'ab'
            with open(self.fs._full(path), mode) as f:
                f.write(body)
            if self.wdt:
                self.wdt.feed()
            _ok(conn)
        except Exception as e:
            _err(conn, '500 Internal Server Error', e)

    def _mkdir(self, conn, params):
        if self._need_sd(conn): return
        path = _safe_path(params.get('path', ''))
        if not path:
            _err(conn, '400 Bad Request', 'missing/invalid path'); return
        try:
            self.fs.mkdir(path)
            _ok(conn)
        except Exception as e:
            _err(conn, '500 Internal Server Error', e)

    def _rename(self, conn, params):
        if self._need_sd(conn): return
        src = _safe_path(params.get('src', ''))
        dst = _safe_path(params.get('dst', ''))
        if not src or not dst:
            _err(conn, '400 Bad Request', 'missing/invalid src or dst'); return
        try:
            import uos
            uos.rename(self.fs._full(src), self.fs._full(dst))
            _ok(conn)
        except Exception as e:
            _err(conn, '500 Internal Server Error', e)

    def _rm(self, conn, params):
        if self._need_sd(conn): return
        path = _safe_path(params.get('path', ''))
        if not path:
            _err(conn, '400 Bad Request', 'missing/invalid path'); return
        try:
            if self.fs.is_dir(path):
                self.fs.rmdir(path, recursive=True)
            else:
                self.fs.delete(path)
            _ok(conn)
        except Exception as e:
            _err(conn, '500 Internal Server Error', e)
