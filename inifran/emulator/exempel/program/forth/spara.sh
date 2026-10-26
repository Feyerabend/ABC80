#!/bin/sh
# Skriver in Forthaid II:s ord i varje NAMN.FS här i dess tolk och sparar
# dem med SAVEW som NAMN.FTH på exempeldisketten (../diskett/program.dsk),
# en omgång i taget: rader som börjar med \ hoppas över, och en tom rad
# sparar orden med SAVEW och laddar dem med LOADW, så att de flyttas under
# STKTOP och nästa omgång får de 272 byten på $FB70 (se VERKTYG.FS).
# En fil som redan finns på skivan skrivs över. Sist laddas varje fil med
# LOADW i en ny körning, som ska svara OK.
#
# En ordfil hör till den Forthaid II den gjordes med (LOADW prövar
# LATEST), så både ../gor.sh och ../../../../utanfor/forthaid2/gor.sh kör
# det här skriptet. Saknas emulatorn, DOS:en, skivan eller Forthaid II
# säger skriptet det och gör ingenting.
#
#     ./spara.sh            (alla *.FS)
#     ./spara.sh VERKTYG    (bara VERKTYG.FS)
cd "$(dirname "$0")" || exit 1
ABC80=../../../abc80
ROM=../../../roms/abc80new.rom
DOS=../../../roms/abcdos80.rom
SKIVA=../diskett/program.dsk
FA2=../../../../utanfor/forthaid2
banker=$(ls $FA2/forthaid2_b[0-9].bin 2>/dev/null | tr '\n' ',' | sed 's/,$//')
for f in "$ABC80" $ROM $DOS $SKIVA $FA2/forthaid2.bin; do
  [ -f "$f" ] || { echo "spara.sh: $f finns inte, inga ordfiler sparade"; exit 0; }
done
[ -n "$banker" ] || { echo "spara.sh: Forthaid II:s banker finns inte, inga ordfiler sparade"; exit 0; }
kor() {
  "$ABC80" -r $ROM -l $DOS@6000 -D $SKIVA@45 -DS \
    -l $FA2/forthaid2.bin@4000 -b $banker@5000 -k "$1" -t 1000
}
start='\w3000\.Z=CALL(16384,2)\r\w600\.FORTH\r\w600\.'
if [ $# -eq 0 ]; then set -- $(ls *.FS | sed 's/\.FS$//'); fi
for namn in "$@"; do
  [ -f $namn.FS ] || { echo "spara.sh: $namn.FS finns inte"; exit 1; }
  # LOADW tar längre tid ju större filen är.
  spara="SAVEW $namn.FTH\\r\\w6000\\.LOADW $namn.FTH\\r\\w6000\\."
  k=$start
  ny=
  while IFS= read -r r; do
    case "$r" in
      '\'*) ;;
      '') [ -n "$ny" ] && k="$k$spara"; ny= ;;
      *) k="$k$r\\r\\w500\\."; ny=1 ;;
    esac
  done < $namn.FS
  k="${k}SAVEW $namn.FTH\\r\\w6000\\.BYE\\r\\w600\\."
  kor "$k" > /dev/null || exit 1
  # Varje ord som filen definierar ska finnas: ' ger 0 för ett som
  # saknas, och 0 ' NAMN 0= OR ... . ska skriva 0 (sex namn på en rad).
  prov=$(grep -v '^\\' $namn.FS | tr ' ' '\n' | awk '
    BEGIN { printf "0\\r\\w500\\." }
    f { printf "\047 %s 0= OR ", $0; if (++n % 6 == 0) printf "\\r\\w500\\."; f = 0; next }
    $0 == ":" || $0 == "VARIABLE" || $0 == "CONSTANT" { f = 1 }
    END { if (n % 6) printf "\\r\\w500\\."; printf ".\\r\\w500\\." }')
  ut=$(kor "${start}LOADW $namn.FTH\\r\\w6000\\.$prov") || exit 1
  if ! echo "$ut" | grep -q "^LOADW $namn.FTH OK" || ! echo "$ut" | grep -q '^\. 0 OK'; then
    echo "spara.sh: $namn.FTH är inte hel:"; echo "$ut" | grep -v '^ *$' | tail -8; exit 1
  fi
  echo "spara.sh: $namn.FTH sparad på $(basename $SKIVA)"
done
