# roms/

The Sinclair ZX81 ROM is not part of this repository. Fetch it with

```bash
python3 roms/fetch_rom.py               # download
python3 roms/fetch_rom.py --from FILE   # or use a ROM file you already have
```

The script checks the file (8 KB, SHA-256 `14ad84f4…7828dfa`, the
"improved" version 2 ROM) and saves it as `roms/zx81.rom`. CMake turns it
into `src/zx81_rom_data.h` when the firmware is built.

A commented disassembly of the ROM:
https://github.com/ZXSpectrumVault/rom-disassemblies
