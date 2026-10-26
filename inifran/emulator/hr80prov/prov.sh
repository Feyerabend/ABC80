#!/bin/sh
# Provet till grafikkortet i HR-80:s anda (karna/hr80.c; utanför boken).
# Varje rad nedan är ett namn och flaggorna till ../hr80; bilden blir
# ut/namn.png och dess SHA-256 jämförs med den i summor.txt. Med -n
# skrivs summor.txt om (titta på bilderna först).
#   a  text, förgrund och bakgrund, en ram, fyllning, en diagonal, markören
#   b  semigrafiken (sammanhängande och åtskild), färgekvivalensen och
#      de relativa kommandona med fyllning
#   c  80 tecken svartvitt, raden bryts, två diagonaler
#   d  makron: ett med instruktioner (en kvadrat) och en bitmapp (ett X)
cd "$(dirname "$0")"
mkdir -p ut
fel=0
: > ut/summor.txt
while IFS='|' read -r namn flaggor; do
  case "$namn" in ''|'#'*) continue ;; esac
  eval "../hr80 $flaggor ut/$namn.png" || { echo "FEL    $namn"; fel=1; continue; }
  echo "$namn $(shasum -a 256 ut/$namn.png | cut -d' ' -f1)" >> ut/summor.txt
done <<'FALL'
a|-m -s 'HR-80 I ABC80 INIFR\x5bN\r\n' -s '\x14\x01R\x14\x02G\x14\x03V\x14\x01\x15\x02 OMV\x15\x00 \x14\x03' -s '\r\n\x01\x01\x1c\x14\x14\x04\xeb\x14\x04\xeb\xc8\x04\x14\xc8\x04\x14\x14\x01\x02\x05\x80\x60\x01\x03\x1c\x00\x00\x04\xff\xff'
b|-s '\x10SEMIGRAFIK: \x20\x21\x22\x23\x24\x7f\x3f\x6f\r\n\x11\x14\x02\x7f\x7f\x3f\x6f\x16 TEXT\r\n' -s '\e0\x04\e3\x03\x01\x02\x1c\x80\x80\x1e\x30\x00\x1e\x00\x30\x1e\xd0\x00\x1e\x00\xd0\x01\x01\x1f\x08\x08'
c|-s '\e8' -s 'ATTIO TECKEN PER RAD: 0123456789012345678901234567890123456789012345678901234567890123456789\r\n' -s '\x1c\x00\x00\x04\xff\xff\x1c\x00\xff\x04\xff\x00'
d|-s '\eD\x80R\x1e\x10\x00\x1e\x00\x10\x1e\xf0\x00\x1e\x00\xf0\xfe' -s '\eD\x81B\x07\x41\x22\x14\x08\x14\x22\x41\xfe' -s '\x01\x01\x1c\x20\x20\x80\x01\x02\x1c\x60\x60\x80\x01\x03\x1c\xa0\xa0\x80\x1c\x40\xe0\x81\x1c\x50\xe0\x81'
FALL
if [ "$1" = -n ]; then cp ut/summor.txt summor.txt; echo "summor.txt omskriven"; exit $fel; fi
while read -r namn summa; do
  if grep -qx "$namn $summa" summor.txt; then echo "lika   $namn"
  else echo "OLIKA  $namn (se ut/$namn.png)"; fel=1; fi
done < ut/summor.txt
exit $fel
