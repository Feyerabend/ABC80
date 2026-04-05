# file_server.py — SD card access layer for PicoFS
#
# Thin wrapper over uos / FAT filesystem on an SD card.
# Provides only what the HTTP server actually needs:
#   list, stat, exists, is_file, is_dir,
#   mkdir, rmdir, delete, disk_info
#
# File reads and writes are done directly by the HTTP server
# using open() on the path returned by _full(), keeping this
# layer simple and avoiding double-buffering large data.
#
# SPI wiring:
#   GP1 CS  GP2 SCK  GP3 MOSI  GP4 MISO

import machine, sdcard, uos, gc

SD_CS   = 1
SD_SCK  = 2
SD_MOSI = 3
SD_MISO = 4
SD_BAUD = 4_000_000
MOUNT   = '/sd'

class FSError(Exception):
    pass

# ---------------------------------------------------------------------------

class FileServer:

    def __init__(self, mount_point=MOUNT):
        self.mp = mount_point

    # Path helpers

    def _full(self, path):
        """User-relative path → absolute path on SD."""
        path = str(path).strip('/')
        return self.mp if not path else self.mp + '/' + path

    def _norm(self, path):
        """Normalise to a clean relative path string."""
        return str(path).strip('/')

    def _name(self, path):
        """Last component of a path."""
        return self._norm(path).split('/')[-1]

    # Type checks

    def exists(self, path):
        try:
            uos.stat(self._full(path))
            return True
        except OSError:
            return False

    def is_file(self, path):
        try:
            return bool(uos.stat(self._full(path))[0] & 0x8000)
        except OSError:
            return False

    def is_dir(self, path):
        try:
            return bool(uos.stat(self._full(path))[0] & 0x4000)
        except OSError:
            return False

    # Stat / list

    def stat(self, path):
        """Return {name, type, size} for a file or directory."""
        fp = self._full(path)
        try:
            s = uos.stat(fp)
        except OSError:
            raise FSError('{}: not found'.format(path))
        is_d = bool(s[0] & 0x4000)
        return {
            'name': self._norm(path),
            'type': 'dir' if is_d else 'file',
            'size': s[6],
        }

    def list(self, path='/'):
        """
        List a directory.
        Returns a list of stat dicts, directories first, both alphabetical.
        """
        fp = self._full(path)
        if not self.exists(path):
            raise FSError('{}: not found'.format(path))
        if not self.is_dir(path):
            raise FSError('{}: not a directory'.format(path))
        try:
            names = uos.listdir(fp)
        except OSError as e:
            raise FSError('{}: listdir failed: {}'.format(path, e))

        base = self._norm(path)
        entries = []
        for name in sorted(names):
            child = (base + '/' + name).lstrip('/') if base else name
            try:
                entries.append(self.stat(child))
            except FSError:
                pass

        entries.sort(key=lambda e: (0 if e['type'] == 'dir' else 1, e['name']))
        return entries

    # Directories

    def mkdir(self, path):
        """Create directory, including missing parents."""
        path = self._norm(path)
        if self.exists(path):
            raise FSError('{}: already exists'.format(path))
        built = ''
        for part in path.split('/'):
            if not part:
                continue
            built = (built + '/' + part).lstrip('/')
            try:
                uos.mkdir(self._full(built))
            except OSError as e:
                if not self.is_dir(built):
                    raise FSError('mkdir {}: {}'.format(built, e))

    def rmdir(self, path, recursive=False):
        """Remove a directory.  recursive=True removes contents first."""
        path = self._norm(path)
        if not self.is_dir(path):
            raise FSError('{}: not a directory'.format(path))
        if recursive:
            self._rmdir_r(path)
        else:
            if self.list(path):
                raise FSError('{}: not empty'.format(path))
            try:
                uos.rmdir(self._full(path))
            except OSError as e:
                raise FSError('rmdir {}: {}'.format(path, e))

    def _rmdir_r(self, path):
        for e in self.list(path):
            if e['type'] == 'dir':
                self._rmdir_r(e['name'])
            else:
                self.delete(e['name'])
        try:
            uos.rmdir(self._full(path))
        except OSError as e:
            raise FSError('rmdir {}: {}'.format(path, e))

    # Files

    def delete(self, path):
        """Delete a file."""
        path = self._norm(path)
        if not self.is_file(path):
            raise FSError('{}: not a file'.format(path))
        try:
            uos.remove(self._full(path))
        except OSError as e:
            raise FSError('{}: delete failed: {}'.format(path, e))

    # Disk

    def disk_info(self):
        """Return {total, free} bytes on the SD card."""
        try:
            s = uos.statvfs(self.mp)
            total = s[0] * s[2]
            free  = s[0] * s[3]
            return {'total': total, 'free': free}
        except OSError as e:
            raise FSError('disk_info: {}'.format(e))


# ---------------------------------------------------------------------------

def mount_sd(cs=SD_CS, sck=SD_SCK, mosi=SD_MOSI, miso=SD_MISO,
             baud=SD_BAUD, mount=MOUNT):
    """Mount the SD card and return a FileServer instance."""
    try:
        spi = machine.SPI(0,
                          baudrate=baud, polarity=0, phase=0,
                          bits=8, firstbit=machine.SPI.MSB,
                          sck=machine.Pin(sck),
                          mosi=machine.Pin(mosi),
                          miso=machine.Pin(miso))
        sd  = sdcard.SDCard(spi, machine.Pin(cs, machine.Pin.OUT))
        vfs = uos.VfsFat(sd)
        uos.mount(vfs, mount)
        gc.collect()
        print('SD mounted at', mount)
    except OSError as e:
        raise FSError('SD mount failed: {}'.format(e))
    return FileServer(mount)
