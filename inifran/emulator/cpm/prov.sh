#!/bin/sh
# Provet till start-PROM:en (cpm/start.asm) och BIOS:en (cpm/bios.asm):
# bygger provskivorna och kör emulatorn med -cpm i batchläget. Z80ASM
# pekar på assemblern (verktyg/ i det publika förrådet); cpm22.bin och
# bios.bin bygger make.
cd "$(dirname "$0")"
Z80ASM=${Z80ASM:-$HOME/Documents/GitHub/ABC80/inifran/verktyg/z80asm}
ROM=../roms/abc80new.rom
$Z80ASM -o start.bin start.asm || exit 1
$Z80ASM -o prov/hej.bin prov/hej.asm || exit 1
python3 -I prov/skiva.py prov/hej.bin prov/prov.dsk || exit 1
head -c 81920 /dev/zero > prov/tom.dsk
fel=0
prova() {                               # väntad första rad, flaggor
  vantad=$1; shift
  rad=$(../abc80 -r $ROM -t 300 "$@" | sed -n '1{s/ *$//;p;}')
  if [ "$rad" = "$vantad" ]; then printf "rätt  %s\n" "$*"
  else printf "FEL   %s: \"%s\", väntat \"%s\"\n" "$*" "$rad" "$vantad"; fel=1; fi
}
prova "STARTEN GICK BRA" -cpm prov/prov.dsk
prova "FEL I SEKTORERNA" -cpm prov/prov.dsk -DQ '5+10=00'   # en byte fel i sektor 5
som_abc80() {                           # flaggor; hela skärmen som utan kortet
  if [ "$(../abc80 -r $ROM -t 300 -k 'PRINT 6*7\r' "$@")" = "$(../abc80 -r $ROM -t 300 -k 'PRINT 6*7\r')" ]
  then printf "rätt  %s (ABC80)\n" "$*"
  else printf "FEL   %s: inte som ABC80\n" "$*"; fel=1; fi
}
som_abc80 -cpm prov/tom.dsk                   # inget "CP/M"
som_abc80 -cpm prov/prov.dsk -DB 3            # läsfel i sektor 3
som_abc80 -cpm prov/prov.dsk -DQ '0+0=00'     # startblocket utan "CP/M"

# BIOS:en: en systemskiva med STAT, PIP och konsolprovet.
$Z80ASM -o prov/konsol.com prov/konsol.asm || exit 1
python3 -I systemskiva.py cpm22.bin bios.bin prov/system.dsk \
  dr/STAT.COM dr/PIP.COM prov/konsol.com || exit 1
python3 -I systemskiva.py cpm22.bin bios.bin prov/b.dsk || exit 1   # tom, formaterad
finns() {                               # väntad text, tangenter, flaggor
  vantad=$1; tangenter=$2; shift 2
  if ../abc80 -r $ROM -t 3000 "$@" -k "$tangenter" | grep -qF "$vantad"
  then printf "rätt  %s: %s\n" "$tangenter" "$vantad"
  else printf "FEL   %s %s: inget \"%s\"\n" "$tangenter" "$*" "$vantad"; fel=1; fi
}
S="-cpm prov/system.dsk"
finns "CP/M 2.2 på ABC80 med UNI-80" '' $S
finns "A>" '' $S
if ../abc80 -r $ROM -t 3000 -L $S -k '\w1000KONSOL\r' | cmp -s - prov/konsol.txt
then echo "rätt  KONSOL (skärmen och LST:)"
else echo "FEL   KONSOL: inte som prov/konsol.txt"; fel=1; fi
finns "Bytes Remaining On C: 14k" '\w1000STAT C:\r' $S
finns "Bytes Remaining On C: 30k" '\w1000STAT C:\r' $S -m 32
finns "Bytes Remaining On B: 70k" '\w1000STAT B:\r' $S -D prov/b.dsk@45.1
../abc80 -r $ROM -t 3000 $S -D prov/b.dsk@45.1 -DS -k '\w1000PIP B:=A:*.COM\r' >/dev/null
finns "B: STAT     COM : PIP      COM : KONSOL" '\w1000DIR B:\r' $S -D prov/b.dsk@45.1        # skrivet tillbaka
finns "Bdos Err On B: Bad Sector" '\w1000PIP B:=A:STAT.COM\r' $S -DP prov/b.dsk@45.1   # skrivskyddad
finns "A: STAT     COM : PIP      COM : KONSOL   COM" '\w1000DIR\r' $S -tkn80 ../roms/tkn80-iii.rom

# D: på kort 44 (640 KB): tom, skriven med PIP och utan kortet.
python3 -I systemskiva.py -44 prov/d.dsk || exit 1
finns "Bytes Remaining On D: 636k" '\w1000STAT D:\r' $S -D prov/d.dsk
../abc80 -r $ROM -t 3000 $S -D prov/d.dsk -DS -k '\w1000PIP D:=A:STAT.COM\r' >/dev/null
finns "D: STAT     COM" '\w1000DIR D:\r' $S -D prov/d.dsk
finns "Bdos Err On D: Select" '\w1000STAT D:\r' $S

# Programmen: ED skriver HEJ.ASM, ASM och LOAD gör HEJ.COM, som körs,
# sparas med SAVE (efter körningen, ur TPA) och ändras med ED:s S.
# Skivan skrivs tillbaka (-DS) mellan körningarna; RAM-skivan C: töms
# vid kallstarten, så PIP till C: och körningen där sker i samma körning.
python3 -I systemskiva.py cpm22.bin bios.bin prov/ed.dsk \
  dr/ED.COM dr/ASM.COM dr/LOAD.COM dr/PIP.COM || exit 1
steg() {                                # namn, väntad text, tangenter, flaggor
  namn=$1; vantad=$2; tangenter=$3; shift 3
  if ../abc80 -r $ROM -t 20000 -cpm prov/ed.dsk -DS "$@" -k "\\w1000$tangenter" | grep -qF "$vantad"
  then printf "rätt  %s: %s\n" "$namn" "$vantad"
  else printf "FEL   %s %s: inget \"%s\"\n" "$namn" "$*" "$vantad"; fel=1; fi
}
T='\x09'
steg "ED, ny fil" "NEW FILE" "ED HEJ.ASM\\r\\w1500I\\r\\w300${T}ORG${T}100H\\r${T}MVI${T}C,9\\r${T}LXI${T}D,TEXT\\r${T}CALL${T}5\\r${T}RET\\rTEXT:${T}DB${T}'HEJ FRAN ASM\$'\\r${T}END\\r\\x1a\\w300E\\r"
steg "ASM" "0116" 'ASM HEJ\r'
steg "LOAD" "BYTES READ    0016" 'LOAD HEJ\r'
steg "HEJ.COM" "HEJ FRAN ASM" 'HEJ\r'
steg "SAVE" "A>ERA HEJ.COM" 'HEJ\r\w1000SAVE 1 HEJ2.COM\r\w1000ERA HEJ.COM\r'
steg "HEJ2.COM" "HEJ FRAN ASM" 'HEJ2\r'
steg "ED, ändrad, och C: med 80 tecken" "HEJ IGEN ASM" 'ED HEJ.ASM\r\w1500#A\r\w300B\r\w300SFRAN\x1aIGEN\x1a\r\w300E\r\w2000ASM HEJ\r\w4000LOAD HEJ\r\w2000PIP C:=HEJ.COM\r\w2000C:\r\w500HEJ\r' -tkn80 ../roms/tkn80-iii.rom

# BBC BASIC på B: (cpm/b.dsk, make): bredden efter skärmen, TIME under
# INKEY, POS och VPOS ur BIOS:en, SAVE och LOAD på B:.
cp b.dsk prov/bbc.dsk || exit 1
basic() {                               # namn, väntad text, tangenter, flaggor
  namn=$1; vantad=$2; tangenter=$3; shift 3
  if ../abc80 -r $ROM -t 20000 -cpm prov/system.dsk "$@" -D prov/bbc.dsk@45.1 -DS \
       -k "\\w1000B:\\r\\w300BBCBASIC\\r\\w3000\\r$tangenter" | grep -qF "$vantad"
  then printf "rätt  BBC BASIC, %s: %s\n" "$namn" "$vantad"
  else printf "FEL   BBC BASIC, %s %s: inget \"%s\"\n" "$namn" "$*" "$vantad"; fel=1; fi
}
basic "start" "BBC BASIC (Z80) Version 5.00" ''
basic "bredden" "B40 42" 'PRINT "B";?&1F4;" ";6*7\r'
basic "80 tecken" "B80" 'PRINT "B";?&1F4\r' -tkn80 ../roms/tkn80-iii.rom
basic "INKEY och TIME" "T1" 'T%=TIME:A=INKEY(100):PRINT "T";(TIME-T%) DIV 100\r'
basic "POS och VPOS" "P6 2" 'CLS\r\w300PRINT TAB(5,2);:PRINT "P";POS;" ";VPOS\r'
basic "SAVE" "B: KVADRAT  BBC" '10 PRINT 12*12\r\w300SAVE "KVADRAT"\r\w1000*DIR\r'
basic "LOAD" "       144" 'LOAD "KVADRAT"\r\w1000RUN\r'

# Editorn SKRIV (cpm/skriv.com, make): en ny fil, en ändring med ^X ^F,
# rullning, ^Y och ^U, sökning, ^KQ och 80 tecken med vänsterkanten.
python3 -I systemskiva.py cpm22.bin bios.bin prov/skriv.dsk skriv.com || exit 1
skriv() {                               # namn, väntad text, tangenter, flaggor
  namn=$1; vantad=$2; tangenter=$3; shift 3
  if ../abc80 -r $ROM -t 20000 -cpm prov/skriv.dsk -DS "$@" -k "\\w1000$tangenter" | grep -qF "$vantad"
  then printf "rätt  SKRIV, %s: %s\n" "$namn" "$vantad"
  else printf "FEL   SKRIV, %s %s: inget \"%s\"\n" "$namn" "$*" "$vantad"; fel=1; fi
}
skriv "ny fil" "A:HEJ.TXT* rad 3 kol 7" 'SKRIV HEJ.TXT\r\w1500Hej v{rlden\rRad tv}\rTredje'
skriv "spara" "Rad två" 'SKRIV HEJ.TXT\r\w1500Hej v{rlden\rRad tv}\rTredje\x0bX\w1500TYPE HEJ.TXT\r'
skriv "ändra" "XXTredje" 'SKRIV HEJ.TXT\r\w1500\x18\x06\x06XX\x0bX\w1500TYPE HEJ.TXT\r'
skriv "BAK" "HEJ      BAK" 'DIR\r'
L=$(printf 'Rad %d\\r' $(seq 1 40))
skriv "40 rader" "Rad 40" "SKRIV LANG.TXT\\r\\w1500$L\\x0bX\\w3000TYPE LANG.TXT\\r"
skriv "sida ned" "rad 41 kol 1" 'SKRIV LANG.TXT\r\w1500\x03\x03'
skriv "rulla" "Rad 31" 'SKRIV LANG.TXT\r\w1500\x03\x12\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a'
if ../abc80 -r $ROM -t 20000 -cpm prov/skriv.dsk -k '\w1000SKRIV LANG.TXT\r\w1500\x19\x19\x18\x15' \
     | sed -n '1,4{s/ *$//;p;}' | tr '\n' '/' | grep -qF 'Rad 3/_ad 1/Rad 2/Rad 4/'
then echo "rätt  SKRIV, Y och U: Rad 3, Rad 1, Rad 2, Rad 4"
else echo "FEL   SKRIV, Y och U"; fel=1; fi
if ../abc80 -r $ROM -t 20000 -cpm prov/skriv.dsk -k '\w1000SKRIV LANG.TXT\r\w1500\x1bY\x1by\x1bX\x1bU' \
     | sed -n '1,4{s/ *$//;p;}' | tr '\n' '/' | grep -qF 'Rad 3/_ad 1/Rad 2/Rad 4/'
then echo "rätt  SKRIV, ESC Y ESC y ESC X ESC U: som med Ctrl"
else echo "FEL   SKRIV, ESC som Ctrl"; fel=1; fi
skriv "ESC K X" "Rad 40slutet" 'SKRIV LANG.TXT\r\w1500\x1bqc\x08slutet\x1bKX\w1500TYPE LANG.TXT\r\w1500'
skriv "sök" "rad 25 kol 1" 'SKRIV LANG.TXT\r\w1500\x11FRad 25\r'
skriv "sluta osparad" "Inte sparad!" 'SKRIV LANG.TXT\r\w1500x\x0bQ'
skriv "80 tecken" "cdefgh abcdefgh abcdefgh abcdefgh abcdefgh abcdefgh" "SKRIV LANG.TXT\\r\\w1500\\x11D$(printf ' abcdefgh%.0s' $(seq 8))" -tkn80 ../roms/tkn80-iii.rom

# Zork I med tolken ZTOLK (cpm/ztolk.com och cpm/d.dsk, make): starten,
# ett drag, tangenter i förväg, $verify, save och restore, quit och 80
# tecken.
python3 -I systemskiva.py cpm22.bin bios.bin prov/zork.dsk ztolk.com || exit 1
cp d.dsk prov/zorkd.dsk
zork() {                                # namn, väntad text, tangenter, flaggor
  namn=$1; vantad=$2; tangenter=$3; shift 3
  if ../abc80 -r $ROM -t 60000 -cpm prov/zork.dsk -D prov/zorkd.dsk -DS "$@" \
       -k "\\w1000ZTOLK D:ZORK1\\r\\w8000$tangenter" | grep -qF "$vantad"
  then printf "rätt  Zork, %s: %s\n" "$namn" "$vantad"
  else printf "FEL   Zork, %s %s: inget \"%s\"\n" "$namn" "$*" "$vantad"; fel=1; fi
}
zork "start" "West of House" ''
zork "statusraden" "Score:0 Moves:0" ''
zork "brevlådan" "leaflet" 'open mailbox\r\w3000'
zork "i förväg" "WELCOME TO ZORK" 'open mailbox\rtake leaflet\rread leaflet\r\w6000'
zork "verify" "The disk is correct." '$verify\r\w20000'
zork "save" "Ok." 'north\r\w3000save\r\w2000\r\w10000'
zork "restore" "North of House" 'restore\r\w2000\r\w10000look\r\w3000'
zork "quit" "A: ZTOLK    COM" 'quit\r\w2000y\r\w3000DIR\r\w2000'
zork "80 tecken" "Score: 0  Moves: 0" '' -tkn80 ../roms/tkn80-iii.rom
exit $fel
