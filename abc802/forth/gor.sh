#!/bin/sh
# Bygg Forth för ABC 802: forth.asm (ffc.py), forth.bin ($8000-) och
# skivan forth.img med FORTH.ABS.
set -e
cd "$(dirname "$0")"
# Assemblern: ../verktyg/z80asm.c, byggs första gången.
Z80ASM=${Z80ASM:-../verktyg/z80asm}
[ -x "$Z80ASM" ] || cc -O2 -o "$Z80ASM" ../verktyg/z80asm.c
# Forths radbuffert tar 79 tecken, och resten av en längre rad skulle
# tappas utan att något märks.
python3 -c '
import sys
f = [(n, i) for n in sys.argv[1:] for i, r in enumerate(open(n, encoding="utf-8"), 1)
     if len(r.rstrip("\n")) > 79]
for n, i in f: print("%s:%d: raden är längre än 79 tecken" % (n, i))
sys.exit(1 if f else 0)' kalkyl.fs verk.fs
python3 ffc.py
$Z80ASM -l -o forth.full forth.asm > forth.lst
tail -c +32769 forth.full > forth.bin
rm forth.full
start=$(awk '/LADDAD:/ {print $1; exit}' forth.lst)
python3 abs.py forth.bin 8000 $start FORTH ../emulator/skivor/tom.img forth.img
# Masken i maskinkod: MASKMK.ABS på $A800.
$Z80ASM -l -o mask.full mask.asm > mask.lst
tail -c +43009 mask.full > mask.bin
rm mask.full
start=$(awk '/LADDAD:/ {print $1; exit}' mask.lst)
python3 abs.py mask.bin A800 $start MASKMK forth.img forth.img
# DEMO, MASK och ORD på skivan: emulatorn laddar Forth, skriver in dem och
# sparar dem med SAVE DR0:DEMO osv. (-DS skriver tillbaka skivan).
LADDA='POKE 32768,14,255,195,30,96,70,79,82,84,72,32,32,32,65,66,83:;CALL(32768,32773)'
../emulator/abc802 -DS -D forth.img -K 80,80 -k "$LADDA\\r\\w3000NEW\\r\\w500" \
    -L 400 -f demo.bas -k 'SAVE DR0:DEMO\r\w3000NEW\r\w500' \
    -f mask.bas -k 'SAVE DR0:MASK\r\w3000NEW\r\w500' \
    -f ord.bas -k 'SAVE DR0:ORD\r\w3000' -t 1300 > /dev/null
# Sist BASICINI.SYS, som laddar Forth och startar det när maskinen
# startar (basicini.asm). Den läggs på efter inskrivningen ovan, som
# skriver laddningen i BASIC.
$Z80ASM -l -o basicini.full basicini.asm > basicini.lst
tail -c +62724 basicini.full > basicini.bin
rm basicini.full
python3 abs.py basicini.bin 0 0 BASICINI.SYS forth.img forth.img
# Programskivan program.img till webbsidan: FORTH.ABS och ORD, och
# BASICINI.SYS med PROGRAM EQU 1, som laddar Forth och visar programmen.
python3 abs.py forth.bin 8000 $(awk '/LADDAD:/ {print $1; exit}' forth.lst) FORTH ../emulator/skivor/tom.img program.img
LADDA='POKE 32768,14,255,195,30,96,70,79,82,84,72,32,32,32,65,66,83:;CALL(32768,32773)'
../emulator/abc802 -DS -D program.img -K 80,80 -k "$LADDA\\r\\w3000NEW\\r\\w500" \
    -L 400 -f ord.bas -k 'SAVE DR0:ORD\r\w3000' -t 1300 > /dev/null
sed 's/^PROGRAM EQU  0/PROGRAM EQU  1/' basicini.asm > program_ini.asm
$Z80ASM -l -o program_ini.full program_ini.asm > program_ini.lst
tail -c +62724 program_ini.full > program_ini.bin
rm program_ini.full program_ini.asm
python3 abs.py program_ini.bin 0 0 BASICINI.SYS program.img program.img
# KALKYL.ABS på programskivan: Forth startas, TIB:s tal (gränsen mot
# BASIC) sätts till 55296 ($D800), och kalkyl.fs skrivs in. kabs.py tar
# minnet ($8000 till HERE, abc802 -M) och ger LADDAD samma gräns. Sist
# laddaren KALKYL (kalkyl.bas), som läser in KALKYL.ABS och startar det.
../emulator/abc802 -D program.img -K 40,40 \
    -k "\\w2500Z=CALL(32768)\\r\\w1500 55296 ' TIB 5 + !\\r\\w500" \
    -L 300 -f kalkyl.fs -t 1500 -M kalkyl.mem > /dev/null
python3 kabs.py kalkyl.mem D800 kalkyl.fs forth.lst kalkyl.bin
rm kalkyl.mem
python3 abs.py kalkyl.bin 8000 $(awk '/LADDAD:/ {print $1; exit}' forth.lst) KALKYL program.img program.img
# Överlägg 2 som KALKYL.OVL: kabs.py tar det från RAM-skivan till
# kalkylovl.bin, med RET:en sist som startadress.
read ovb ovs < kalkylovl.adr
python3 abs.py kalkylovl.bin $ovb $ovs KALKYL.OVL program.img program.img
rm kalkylovl.bin kalkylovl.adr
../emulator/abc802 -DS -D program.img -K 80,80 -k "\\w2500NEW\\r\\w500" \
    -L 400 -f kalkyl.bas -k 'SAVE DR0:KALKYL\r\w3000' -t 1300 > /dev/null
# VERK.ABS och laddaren VERK (verk.bas) på samma sätt: kalkyl.fs till
# raden "Hit är KALKYL och VERK lika" och sedan verk.fs, som är ORD:s
# ord, fälten och menyn (se VERK.md). Gränsen mot BASIC är 60672
# ($ED00), och ingång 1 blir VERK. Tabellen i överlägg 2 är bara i KALKYL.
# Det delade i kalkyl.fs ("Det delade" till "Hit det delade") går till
# VERK:s överlägg 2, till "i överlägg 2", efter raden "gor.sh lägger det
# delade här", och till överlägg 4, utom det mellan "i båda" och "i
# överlägg 2", efter "gor.sh lägger läsningen här".
sed -e '/---- Tabellen: bara i KALKYL/,/---- Tabellen slut/d' \
    -e '/sedan verk.fs till VERK.ABS/q' kalkyl.fs > verkin.fs
sed -n '/---- Det delade:/,/---- Hit det delade i överlägg 2/p' kalkyl.fs > delat2.fs
sed -n -e '/---- Det delade:/,/---- Hit det delade i båda/p' \
    -e '/---- Hit det delade i överlägg 2/,/---- Hit det delade ----/p' kalkyl.fs > delat4.fs
sed -e '/gor.sh lägger det delade här/r delat2.fs' \
    -e '/gor.sh lägger läsningen här/r delat4.fs' verk.fs >> verkin.fs
rm delat2.fs delat4.fs
# Emulatorns tangentkö tar 65536 tecken: rader som bara är en kommentar
# skrivs inte in, utom de med ---- (kabs.py söker dem).
sed -E -e '/----/!{' -e '/^\( [^)]*\)$/d' -e '}' verkin.fs > verkin2.fs
mv verkin2.fs verkin.fs
../emulator/abc802 -D program.img -K 40,40 \
    -k "\\w2500Z=CALL(32768)\\r\\w1500 60672 ' TIB 5 + !\\r\\w500" \
    -L 500 -f verkin.fs -t 3000 -M verk.mem > /dev/null
python3 kabs.py verk.mem ED00 verkin.fs forth.lst verk.bin VERK
rm verk.mem verkin.fs
python3 abs.py verk.bin 8000 $(awk '/LADDAD:/ {print $1; exit}' forth.lst) VERK program.img program.img
# Överlägg 4 som VERK.OVL: kabs.py tar det från RAM-skivan till
# verkovl.bin, med en RET sist som startadress, och skriver adresserna
# i verkovl.adr.
read ovb ovs < verkovl.adr
python3 abs.py verkovl.bin $ovb $ovs VERK.OVL program.img program.img
rm verkovl.bin verkovl.adr
# Introduktionen som VERK.HLP (verk.hlp), som VERK läser till PB när den
# visas: kabs.py skriver PB:s adress i verkhlp.adr.
python3 hlp.py verk.hlp verkhlp.adr verkhlp.bin
read hlpa hlps < verkhlp.adr
python3 abs.py verkhlp.bin $hlpa $hlps VERK.HLP program.img program.img
rm verkhlp.bin verkhlp.adr
../emulator/abc802 -DS -D program.img -K 80,80 -k "\\w2500NEW\\r\\w500" \
    -L 400 -f verk.bas -k 'SAVE DR0:VERK\r\w3000' -t 1300 > /dev/null
# Brevet OFFERT.VRK (offert.vrk) till VERK: BASIC skriver det med
# PRINT #1, högst 50 tecken i taget (; utan radslut efter en bit).
python3 -c '
import sys
print("PREPARE \"OFFERT.VRK\" AS FILE 1")
for r in open(sys.argv[1], encoding="utf-8").read().split("\n")[:-1]:
    b = [r[i:i + 50] for i in range(0, len(r), 50)] or [""]
    for i, s in enumerate(b):
        assert chr(34) not in s
        print("PRINT #1,\"%s\"%s" % (s, ";" if i < len(b) - 1 else ""))
print("CLOSE")' offert.vrk > offert.tmp
../emulator/abc802 -DS -D program.img -K 80,80 -k "\\w2500" -L 500 -f offert.tmp \
    -k '\w3000' -t 1300 > /dev/null
rm offert.tmp
# Exemplen OFFERT.KAL och BUDGET.KAL (offert.kal och budget.kal): KALKYL
# startas, raderna skrivs in med SATT (FORMAT för
# raderna med %, TRAD för tabellens rader med %=) och sparas med SPARA.
for f in offert budget; do
    sed -e '/^%/!s/^/SATT /' -e 's/^%=/TRAD /' -e 's/^%/FORMAT /' $f.kal > $f.tmp
    echo "SPARA $(echo $f | tr a-z A-Z)" >> $f.tmp
    ../emulator/abc802 -DS -D program.img -K 80,80 -k "\\w2500RUN KALKYL\\r\\w6000" \
        -L 1500 -f $f.tmp -k '\w4000' -t 2000 > /dev/null
    rm $f.tmp
done
