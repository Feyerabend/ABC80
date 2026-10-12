# Utanför boken

Experiment som inte står i *ABC80 inifrån*, men som för bokens ärende
vidare: tankar om hur ABC80 och dess program kunde ha utvecklats,
byggda på bokens kod. Det som visar sig vara en förbättring behålls
här, men boken beskriver maskinen och programmen som de är, och ju
längre ett experiment går från dem, desto mindre säger det om ABC80
som den var. Därför står det i varje mapps `LASMIG.md` vad som är
ändrat och mot vad.

Bokens egna filer ändras aldrig härifrån: skripten kopierar det de
behöver (i det publika förrådet från `inifran/forth/` och
`inifran/verktyg/`) och ändrar kopian.

| Mapp           | Vad                                                        |
|----------------|------------------------------------------------------------|
| `comal/`       | ABC80-BASIC:en med COMAL:s strukturer (WHILE, REPEAT, block-IF, PROC/EXEC, LOCAL, CASE, LIST med indrag) i 2215 bytes på `$4000`, mätt mot GOTO, GOSUB och BASIC II; bakgrunden om COMAL och Compis i `BAKGRUND.md` |
| `forth_snabb/` | Forth-tolken från exempel 7 med en billigare stackprövning, mätt mot BASIC och mot bokens version |
| `tolkjamforelse/` | ABC80-BASIC:en och BASIC II mätta med samma prov, profilerade per rutin, och lappade ROM:ar med billigare satsslinga, uttryckskoder, variabler, fält och BCD-flyttal |

Annat utanför boken ligger hos det det hör till: CMOS-minnet och
*Forth med CMOS* (`e7c.fs` bland Forth-källorna och
`emulator/exempel/krets/forth_cmos.bin`).
