#!/bin/sh
# Bygg en kommenterad disassemblering ur mallen och ROM:en; allt görs av
# bygg.py (som går att köra direkt, också på Windows: py bygg.py abc80).
#   ./bygg.sh abc80 [ROM]      ->  ut/abc80rom.pdf   (ABC80:s BASIC-ROM, 11273)
#   ./bygg.sh basicii80 [ROM]  ->  ut/basicii80.pdf  (BASIC II för ABC80)
#   ./bygg.sh -m xelatex abc80 ->  med en annan TeX än den första som finns
exec python3 "$(dirname "$0")/bygg.py" "$@"
