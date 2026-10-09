# ROM:arna

De som webbsidan byggs med (`make webb`) och som exemplen i README
använder:

- `abc80new.rom`, `abc80old.rom`: ABC80:s BASIC, den nya (11273) och
  den gamla (9913), 16 KB; samma som i `../disass/abc80/`, där de är
  kommenterade.
- `abcdos80.rom`: ABC-DOS för ABC80, 4 KB på $6000, till disketten i
  FD2 (`-l roms/abcdos80.rom@6000`).

Hämtade 2026-09-23 från <https://www.abc80.net/archive/luxor/Prom/fw/ABC80/>
(`ABCDOS80.bin`, SHA1 a6b3a9587714f8db807c05bee6c71c0684363744).
