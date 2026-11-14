#!/bin/sh
# bygg PDF:en: pdf/bygg.sh  ->  pdf/kassett.pdf (med tectonic)
cd "$(dirname "$0")" && tectonic --keep-logs kassett.tex 2>&1 |
  grep -v '^note:\|^warning: .*\(Overfull\|Underfull\)'
pdfinfo kassett.pdf 2>/dev/null | grep Pages
