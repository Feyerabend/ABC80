#!/bin/sh
# Provet till CP/M 2.2 på ABC 802 (cpm/start.asm, cpm/bios.asm): bygger
# provskivorna i prov/ut och kör emulatorn i batchläget. make cpmprov
# bygger först abc802, systemet och cpm/a.img.
cd "$(dirname "$0")"
Z80ASM=../../dis/build/z80asm
UT=prov/ut
mkdir -p $UT
$Z80ASM -g 0x100 -o $UT/skarm.com prov/skarm.asm >/dev/null || exit 1
awk '{ printf "%s\r\n", $0 }' prov/hej.asm > $UT/hej.asm
python3 -I -c 'import sys; sys.stdout.buffer.write(bytes(k * 7 % 251 for k in range(70000)))' > $UT/stor.dat
python3 -I skiva.py - - - $UT/b.img $UT/skarm.com $UT/hej.asm $UT/stor.dat || exit 1
fel=0
finns() {                               # väntad text (en eller flera rader), tangenter, flaggor
  vantad=$1; tangenter=$2; shift 2
  if ../abc802 -t 3000 "$@" -k "$tangenter" | tr '\n' '|' | grep -qF "$(printf '%s' "$vantad" | tr '\n' '|')"
  then printf "rätt  %s: %s\n" "$tangenter" "$vantad"
  else printf "FEL   %s %s: inget \"%s\"\n" "$tangenter" "$*" "$vantad"; fel=1; fi
}
cp a.img $UT/a.img
A="-D $UT/a.img"
B="-D $UT/b.img@44.1"
finns "CP/M 2.2 på ABC 802, 64 KB" '' $A
finns "A: LOAD     COM : PIP      COM : STAT     COM" 'DIR\r\w800' $A
finns "    5: Reserved Tracks" 'STAT DSK:\r\w1500' $A
finns "Bytes Remaining On B: 222k" 'STAT B:\r\w1500' $A $B

# Skärmen: rad 0 är kommandoraden, rad 1-3 A-C, D bort, NY RAD före G.
finns "A>#:SKARM" 'B:SKARM\r\w1500' $A $B
finns "B0123TX" 'B:SKARM\r\w1500' $A $B
finns "C0123456789" 'B:SKARM\r\w1500' $A $B
finns "F0123456789012345678901234567890123456789
NY RAD
G0123456789" 'B:SKARM\r\w1500' $A $B
finns "J012345678901234567<

SLUT" 'B:SKARM\r\w1500' $A $B
if ../abc802 -t 3000 $A $B -k 'B:SKARM\r\w1500' | grep -q '^D0123'
then echo "FEL   ESC R: rad D finns kvar"; fel=1
else echo "rätt  ESC R"; fi

# ASM och LOAD, programmet, och ^C (varmstart, ^ är Ü) efteråt.
finns "HEJ FRåN ABC 802!" 'ASM B:HEJ\r\w4000LOAD B:HEJ\r\w2000B:HEJ\r\w500' $A $B
finns "A>ÜC
A>DIR B:HEJ.*
B: HEJ      ASM : HEJ      PRN : HEJ      HEX" \
  "ASM B:HEJ\r\w4000$(printf '\003')\w800DIR B:HEJ.*\r\w800" $A $B

# SKRIV: tre rader, rad två flyttas (^E ^E ^Y ^X ^U), ^K X sparar.
cp $UT/a.img $UT/skriv.img
../abc802 -t 3000 -D $UT/skriv.img -DS -k "SKRIV BREV.TXT\r\w2000rad ett\rrad tv{\rrad tre\r$(printf '\005\005\031\030\025\013')X\w2000" >/dev/null
finns "A>TYPE BREV.TXT
rad ett
rad tre
rad tvä" 'TYPE BREV.TXT\r\w800' -D $UT/skriv.img

# BBC BASIC: räkning, TIME under INKEY(200) (räknad, 10 ms per läsning),
# POS och VPOS ur BIOS:en.
finns "1.414213563.14159265" 'BBCBASIC\r\w2000PRINT 2^0.5, PI\r\w300' $A
finns "       202" 'BBCBASIC\r\w2000T=TIME:A=INKEY(200):PRINT TIME-T\r\w4000' $A
finns "                              MITT3410" 'BBCBASIC\r\w2000CLS:PRINT TAB(30,10);"MITT";POS;VPOS\r\w500' $A

# Zork I med ZTOLK: statusraden, ett drag, save och restore.
finns " West of House                                               Score: 0  Moves: 0" 'ZTOLK ZORK1\r\w4000' $A
cp $UT/a.img $UT/zork.img
../abc802 -t 3000 -D $UT/zork.img -DS -k 'ZTOLK ZORK1\r\w4000open mailbox\r\w1500take leaflet\r\w1500save\r\w1500\r\w3000' >/dev/null
finns "You are carrying:
  A leaflet" 'ZTOLK ZORK1\r\w4000restore\r\w1500\r\w3000inventory\r\w1500' -D $UT/zork.img

# En fil över 32 KB (tre extent), kopierad med PIP och kontrollerad.
../abc802 -t 3000 $A $B -DS -k 'PIP A:=B:STOR.DAT[V]\r\w20000' >/dev/null
finns "  547    70k    3 R/W A:STOR.DAT" 'STAT STOR.DAT\r\w1500' $A
if python3 -I - $UT/a.img $UT/stor.dat <<'EOF'
import sys
skiva, fil = open(sys.argv[1], "rb").read(), open(sys.argv[2], "rb").read()
data = 80 * 256
poster = sorted((skiva[data + k + 12], skiva[data + k + 15], skiva[data + k + 16:data + k + 32])
                for k in range(0, 128 * 32, 32)
                if skiva[data + k] == 0 and skiva[data + k + 1:data + k + 12] == b"STOR    DAT")
ut = b""
for ex, rc, block in poster:
    rader = (ex & 1) * 128 + rc
    ut += b"".join(skiva[data + b * 2048:data + b * 2048 + 2048] for b in block if b)[:rader * 128]
sys.exit(ut[:len(fil)] != fil)
EOF
then echo "rätt  STOR.DAT på A: som filen"
else echo "FEL   STOR.DAT på A: inte som filen"; fel=1; fi
exit $fel
