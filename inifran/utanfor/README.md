# Utanför boken

Experiment som inte står i *ABC80 inifrån*, men som för bokens ärende
vidare: tankar om hur ABC80 och dess program kunde ha utvecklats,
byggda på bokens kod. Det som visar sig vara en förbättring behålls
här, men boken beskriver maskinen och programmen som de är, och ju
längre ett experiment går från dem, desto mindre säger det om ABC80
som den var. Därför står det i varje mapps `LASMIG.md` vad som är
ändrat och mot vad.

Experimenten är beskrivna i häftet *ABC80 inifrån. Utanför boken*
([`utanfor.pdf`](utanfor.pdf), 54 sidor), i bokens stil.

Bokens egna filer ändras aldrig härifrån: skripten kopierar det de
behöver (i det publika förrådet från `inifran/forth/` och
`inifran/verktyg/`) och ändrar kopian.

| Mapp           | Vad                                                        |
|----------------|------------------------------------------------------------|
| `comal/`       | ABC80-BASIC:en med COMAL:s strukturer (WHILE, REPEAT, block-IF, PROC/EXEC, LOCAL, CASE, LIST med indrag) i 2215 bytes på `$4000`, mätt mot GOTO, GOSUB och BASIC II; bakgrunden om COMAL och Compis i `BAKGRUND.md`; också på webbsidan (`?krets=comal`) |
| `forth_snabb/` | Forth-tolken från exempel 7 med en billigare stackprövning, mätt mot BASIC och mot bokens version |
| `forthaid/`    | en öppen Smartaid i Forth på `$4000`–`$5FFF`, med skärmeditor och stegning, mätt mot Smartaid 3; manualen i `MANUAL.md`, också på webbsidan (`?krets=forthaid`) |
| `forthaid2/`   | en Forthaid som fungerar med TKN80 i 80-teckensläget (planen i `PLAN.md`); steg 1 klart 2026-10-26: utan bankning under `$5800`, utan VAR, CHANGE, REN och OLD, prövad med och utan TKN80 (`LASMIG.md`); steg 2 klart: bankkortet (emulatorns `-b`, 2 KB på `$5000`, `kort/prov.sh`); steg 3 klart: fjärrorden (`bank/fc2.py`, `bank/prov.sh`, ett anrop över bankgränsen 754 T mot 335); steg 4 klart: alla kommandon i en fast del och tre banker (`banker.txt`), 150 prov ok med och utan TKN80, webbsidan `?krets=forthaid2`; steg 5 klart: editorn och LIST med 80 kolumner (radlängden ur radtabellen), 160 prov ok; steg 6 klart: mätningen (`matning.sh`), lika snabb som Forthaid, ett anrop över bankgränsen per kommando, 335 byte större men 1,8 KB lediga; steg 7 klart: kompilatorn i en fjärde bank, 443 byte lediga i den fasta delen i stället för 107, 166 prov ok; SAVEW och LOADW sparar de egna orden på skivan och laddar dem under STKTOP (bank 4, `prov/fil.sh` med DOS:en); steg 8 klart: manualen i `MANUAL.md`, `README.md` på engelska och webbsidans hjälptext |
| `tolkjamforelse/` | ABC80-BASIC:en och BASIC II mätta med samma prov, profilerade per rutin, och lappade ROM:ar med billigare satsslinga, uttryckskoder, variabler, fält och BCD-flyttal |

Annat utanför boken ligger hos det det hör till: CMOS-minnet och
*Forth med CMOS* (`e7c.fs` bland Forth-källorna och
`emulator/exempel/krets/forth_cmos.bin`).
