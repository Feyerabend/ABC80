# ABC 802:s ROM, kommenterad

*(English below.)*

[`abc802rom.pdf`](abc802rom.pdf) (330 sidor) är hela ABC 802:s ROM,
$0000–$7FFF, genomgången instruktion för instruktion med namn och
kommentarer på svenska: BASIC II (skriven av DIAB för Luxor), DOS v.20 och
tilläggsprogrammen. Källtexten bakom den assembleras till exakt samma
bytes som ROM:en.

Listningen börjar med en inledning och systemvariablerna, och sedan följer
koden avsnitt för avsnitt: RST-vektorerna och kallstarten,
kommandoloopen, skärmen och 6845:an, tangentbordet, grafiken,
nyckelordstabellerna och hopptabellerna, tolken, flyttalen, DOS:en och
resten. Adresserna är länkar: ett hopp eller ett anrop leder till raden
där målet står.

Koden är upphovsrättsligt skyddad och hör till sina upphovsmän. PDF:en
ligger här för att visa hur maskinen fungerar inifrån och tas bort om
rättighetsinnehavaren invänder. ROM:en själv hämtas med
[`../roms/hamta.py`](../roms/hamta.py) och används av
[emulatorn](../emulator/).

Samma sorts listningar till ABC80 och BASIC II för ABC80 finns i
[`../../inifran/disass/`](../../inifran/disass/).

---

# The ABC 802 ROM, commented (English)

[`abc802rom.pdf`](abc802rom.pdf) (330 pages) is the complete ABC 802 ROM,
$0000–$7FFF, disassembled instruction by instruction, with names and
comments in Swedish: BASIC II (written by DIAB for Luxor), DOS v.20 and the
option programs. Its source reassembles to exactly the same bytes as the
ROM, and addresses in the listing are links to the lines they refer to.

The code is copyrighted by its authors. The PDF is here to show how the
machine works and will be removed if the rights holder objects. The ROM
itself is fetched with [`../roms/hamta.py`](../roms/hamta.py) for the
[emulator](../emulator/).
