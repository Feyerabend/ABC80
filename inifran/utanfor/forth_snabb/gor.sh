#!/bin/sh
# Gör forth_snabb.bin: Forth-tolken från exempel 7 som krets på $4000
# (som e7ip.bin), men med en billigare stackprövning i BRANCH.
# Utanför boken: ett experiment om hur ABC80 kunde ha utvecklats, se
# LASMIG.md och ../README.md.
#
# Bokens filer ändras inte. Kärnan (e7k.inc), korskompilatorn (e7fc.py)
# och Forth-källorna kopieras till bygg/, BRANCH byts ut i kopian av
# kärnan, och e7fc.py körs där. Av de fem bilder den skriver tas bara
# e7ip.asm, tolken som krets, och assembleras med z80asm.
# Källorna och z80asm hittas i bokens arkiv (bok/band2/exempel, dis/)
# eller i det publika förrådet (inifran/forth, inifran/verktyg).
#
#     ./gor.sh              (skriver forth_snabb.bin och forth_snabb.lst)
cd "$(dirname "$0")" || exit 1
if [ -d ../../bok/band2/exempel ]; then
  BOK=../../bok/band2/exempel
  Z80ASM=../../dis/build/z80asm
  [ -x "$Z80ASM" ] || (cd ../../dis/04 && ./z80asm.sh ../build/z80asm) || exit 1
else
  PUBLIK=1
  BOK=../../forth
  Z80ASM=../../verktyg/z80asm
  [ -x "$Z80ASM" ] || make -C ../../verktyg z80asm > /dev/null || exit 1
fi
rm -rf bygg && mkdir bygg || exit 1
cp $BOK/e7fc.py $BOK/e7.fs $BOK/e7i.fs $BOK/e7c.fs bygg/ || exit 1

# BRANCH: bokens version prövar tre saker i varje slinga (datastacken
# inte tömd förbi S0, inte nere vid HEAP + 64, returstacken inte nere
# vid kanariefågeln), omkring 190 T-cykler. Här finns bara den farligaste
# kvar: att datastacken växer ned i BASIC:s variabler. Den prövas på
# den höga byten: fel om SP - 64 ligger på samma 256-bytessida som HEAP
# eller lägre. Prövningen slår alltså till upp till 256 bytes för tidigt,
# men kostar 48 T och behöver varken DE eller något RAM. De två andra
# fallen fångar ?STACK efter varje rad (QUIT), som förut.
python3 - $BOK/e7k.inc bygg/e7k.inc <<'PY' || exit 1
import re, sys
text = open(sys.argv[1], encoding="utf-8").read()
start = text.index(";W BRANCH BRANCH\n")
slut = text.index(";W 0BRANCH QBRAN BRANCH\n")
ny = """;W BRANCH BRANCH
; Utanför boken (utanfor/forth_snabb): bara datastacken mot BASIC:s
; variabler prövas, på den höga byten (fel om SP - 64 ligger på HEAP:s
; sida eller lägre). Resten prövar ?STACK efter varje rad.
BRANCH: LD   HL,-64
        ADD  HL,SP
        LD   A,(HEAP+1)
        CP   H
        JP   NC,STKERR
        EX   DE,HL
        LD   E,(HL)
        INC  HL
        LD   D,(HL)
        JP   NEXT

"""
open(sys.argv[2], "w", encoding="utf-8").write(text[:start] + ny + text[slut:])
PY

(cd bygg && python3 e7fc.py > /dev/null) || exit 1
if [ -n "$PUBLIK" ]; then
  # inifran/verktyg/z80asm: filen börjar vid lägsta ORG, $4000
  $Z80ASM -l forth_snabb.lst -o forth_snabb.bin bygg/e7ip.asm || exit 1
else
  # dis/build/z80asm: filen börjar på 0, listningen på stdout
  $Z80ASM -l -o bygg/e7ip.full bygg/e7ip.asm > forth_snabb.lst || exit 1
  tail -c +$((0x4000 + 1)) bygg/e7ip.full > forth_snabb.bin || exit 1
fi
rm -rf bygg
echo "gor.sh: forth_snabb.bin ($(wc -c < forth_snabb.bin | tr -d ' ') bytes)"
