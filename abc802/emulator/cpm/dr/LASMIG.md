# Digital Researchs CP/M 2.2

Filerna här är Digital Researchs CP/M 2.2, oförändrade, ur den fria
utgåvan på The Unofficial CP/M Web Site. CCP:n och BDOS:en byggs ur
källan och programmen läggs på systemskivan (`make`, se `../cpm22.py`
och `../systemskiva.py`). Start-PROM:en och BIOS:en (`../start.asm`,
`../bios.asm`) är skrivna till emulatorn; ingenting här eller där
kommer från MyAB.

- `CPM22.Z80`: källan till CCP:n och BDOS:en med Z80-koder, återskapad
  ur ett minnesutdrag av Clark A. Calkins 1981. `CPM22.READ.ME` är
  arkivets `READ.ME`. Ur `cpm2-asm.zip` (”CP/M 2.2 ASM SOURCE”),
  <http://www.cpm.z80.de/download/cpm2-asm.zip>, SHA1
  9e183ef9d191d68e2481d414372ff301373972d7.
- `ASM.COM`, `DDT.COM`, `DUMP.COM`, `ED.COM`, `LOAD.COM`, `PIP.COM`,
  `STAT.COM`, `SUBMIT.COM`, `XSUB.COM`: programmen ur `cpm22-b.zip`
  (”CP/M 2.2 BINARY”, distributionsskivan till Xerox 1800),
  <http://www.cpm.z80.de/download/cpm22-b.zip>, SHA1
  6b9f578599bcb8467624d763aad55cb9751de05d.

Jämförda byte för byte med arkiven 2026-10-12. `../cpm22.py` gör om
källan till z80asm:s form när den byggs och sätter minnet till 63 KB
(CCP:n på $E000). Prompten och CCP:ns meddelanden skrivs med versaler,
som i DR:s `CPM.SYS` i `cpm22-b.zip`; Calkins hade gemener. Filerna här
ändras inte.

## Tillståndet

Rättigheterna till CP/M gick från Digital Research via Novell och
Caldera till Lineo, och ägs nu av DRDOS, Inc. Lineos vd Bryan Sparks gav
2001 ett tillstånd att använda, sprida och ändra CP/M ”as part of the
'Unofficial CP/M Web Site'”, vilket en del läste som att det bara fick
spridas därifrån. 2022 skrev han, nu som vd för DRDOS, Inc., om det
utan den begränsningen:

> Let this paragraph represent a right to use, distribute, modify,
> enhance, and otherwise make available in a nonexclusive manner CP/M
> and its derivatives. This right comes from the company, DRDOS, Inc.'s
> purchase of Digital Research, the company and all assets, dating back
> to the mid-1990's. DRDOS, Inc. and I, Bryan Sparks, President of
> DRDOS, Inc. as its representative, is the owner of CP/M and the
> successor in interest of Digital Research assets.

Båda breven finns på <http://www.cpm.z80.de/license.html> (uppdaterad
9 juli 2022).
