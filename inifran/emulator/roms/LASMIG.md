# ROM:arna

ROM-filerna ligger inte i förrådet. Den färdiga webbsidan
`../webb/abc80.html` (också på GitHub Pages) har dem inbyggda; för att
bygga den eller emulatorn själv hämtas de i `emulator/` med

    python3 roms/hamta.py        # eller: make roms   (Windows: py roms\hamta.py)

Skriptet hämtar dem från abc80.net:s arkiv och från abc80sim, sätter
ihop ABC80:s fyra kretsar och kontrollerar varje fil mot sin SHA-256;
en fil som redan finns och stämmer hämtas inte igen. `make webb` och
`make prov` hämtar dem själva om de saknas. Verktygen i `../../verktyg/`
och försöken i `../../utanfor/` läser dem här, så hämta dem först.

- `abc80old.rom`, `abc80new.rom`: ABC80:s BASIC, den gamla (11273) och
  den nya (9913), 16 KB, ur kretsarna ZA3506–ZA3509 i arkivet
  (`ABC80-ZA3506-11273.bin` osv.). Den kommenterade listningen i
  `../../disass/` byggs ur den gamla.
- `basicii80.rom`: BASIC II för ABC80, 32 KB (`-r roms/basicii80.rom -m 32`),
  ur abc80sim (`roms/abc80/basicii80.rom`); den kommenterade listningen
  i `../../disass/` byggs ur den.
- `abcdos80.rom`: ABC-DOS för ABC80, 4 KB på $6000, till disketten i
  FD2 (`-l roms/abcdos80.rom@6000`), ur arkivet (`ABCDOS80.bin`).
- `p40.rom`: drivrutinen till nålskrivaren P40, 1 KB på $7800
  (`-l roms/p40.rom@7800`, med `-CP`), ur arkivet (`P40.bin`, CRC
  43665eba; arkivets text: "PROM on controller in ABC-P40 printer,
  2708"). Beskriven i band 2, kapitlet Skrivarna.
- `tkn80-iii.rom`: EPROM:en på MyAB:s 80-teckenskort TKN80-III, 4 KB
  (`-tkn80 roms/tkn80-iii.rom`), med de block som ersätter $0000–$03FF
  och $2000–$23FF i 80-teckensläget, ett par för vardera BASIC-versionen;
  emulatorn väljer det som hör till ROM:en. Ur arkivet (`TKN80-III.bin`,
  CRC f0d2e4fa, samma som MAME:s `tkn80`). Beskriven i band 2, kapitlet
  Skärmen.

Arkivet: <https://www.abc80.net/archive/luxor/Prom/fw/ABC80/>;
abc80sim: <https://github.com/hpax/abc80sim>.
