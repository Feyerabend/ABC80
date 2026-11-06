/*
 * bbs.js -- en BBS att ringa till från CH B, till ABC 802 inifrån
 *
 * En REKONSTRUKTION: Bytesboden har aldrig funnits. Namnet, inläggen,
 * användarna och filerna är påhittade här för att visa hur ett samtal
 * till en BBS (bulletin board system) gick till i början av 1980-talet:
 * man ringde med ett modem på 300 bit/s, loggade in med namn och
 * lösenord, läste och skrev inlägg och hämtade program.
 *
 * ABC 802 är terminalen: LOAD V24:TSA30B24.22A (options-PROM:et,
 * manualen s. 7-8, typ T, ADM-3A, 300 bit/s åt båda hållen; ROM:en
 * $7000, TERM och ADMTAB i dis/build/abc802rom.asm). Det BBS:en sänder:
 *     ^Z ($1A)           töm skärmen
 *     ^M ^J              ny rad
 *     ^H ($08)           ett steg vänster
 * Terminalen visar inte själv det man skriver, så BBS:en skickar
 * tillbaka varje tecken (eko), och * för lösenordet. Det tangenterna ger
 * kommer med paritet i bit 7, som tas bort. Tecknen är 7 bitar, med
 * å ä ö som } { | i den svenska teckensatsen.
 *
 * Ett program hämtas så: BBS:en säger vad man ska göra, väntar tills
 * texten har gått ut och sedan en stund till, medan man tar sig ur
 * terminalen (PF1, fel 53) och skriver LOAD V24:VSA30B24.22A. Sedan
 * sänds programmet som text, rad för rad med CR, och BREAK avslutar
 * LOAD. Med LOAD V24:TSA30B24.22A och en tangent är man tillbaka.
 *
 * skapa_bbs(sand, ko_langd) ger { ring, lagg_pa, tecken, uppkopplad }:
 * sand() får bytes till maskinen, ko_langd() säger hur många som ännu
 * inte har gått ut, och tecken() tar ett tecken från maskinen.
 */

/* Tecknen som inte är ASCII, som i ABC 802:s svenska teckensats. */
const BBS_SVENSKA = { 'å': 0x7D, 'ä': 0x7B, 'ö': 0x7C, 'é': 0x60, 'ü': 0x7E,
                      'Å': 0x5D, 'Ä': 0x5B, 'Ö': 0x5C, 'É': 0x40, 'Ü': 0x5E };
const BBS_TILLBAKA = {};
for (const [c, b] of Object.entries(BBS_SVENSKA))
    BBS_TILLBAKA[b] = c;

/* Text till bytes: \n blir CR LF (eller bara CR i ett program). */
function bbs_bytes(text, radslut = [0x0D, 0x0A]) {
    const ut = [];
    for (const c of text) {
        if (c === '\n')
            ut.push(...radslut);
        else if (c in BBS_SVENSKA)
            ut.push(BBS_SVENSKA[c]);
        else {
            const k = c.charCodeAt(0);
            ut.push(k < 0x80 ? k : 0x3F);
        }
    }
    return ut;
}

/* Programmen att hämta. De är prövade i emulatorn (BASIC II). */
const BBS_FILER = [
    { namn: 'HEJ.BAS', om: 'Säger hej, det minsta som går', text: [
        '10 REM HEJ, FRÅN BYTESBODEN (REKONSTRUKTION)',
        '20 PRINT "HEJ! DET HÄR PROGRAMMET KOM ÖVER TELEFONEN."',
        '30 PRINT "300 BIT/S, UNGEFÄR 30 TECKEN I SEKUNDEN."',
        '40 END'] },
    { namn: 'TABELL.BAS', om: 'Multiplikationstabellen med PRINT USING', text: [
        '10 REM MULTIPLIKATIONSTABELL',
        '20 FOR I=1 TO 9',
        '30 FOR J=1 TO 9',
        '40 PRINT USING "###",I*J;',
        '50 NEXT J',
        '60 PRINT',
        '70 NEXT I',
        '80 END'] },
    { namn: 'GISSA.BAS', om: 'Gissa talet mellan 1 och 100', text: [
        '10 REM GISSA TALET',
        '20 RANDOMIZE',
        '30 T=INT(RND*100)+1',
        '40 N=0',
        '50 PRINT "JAG TÄNKER PÅ ETT TAL 1-100."',
        '60 INPUT "DIN GISSNING";G',
        '70 N=N+1',
        '80 IF G<T THEN PRINT "FÖR LITET." : GOTO 60',
        '90 IF G>T THEN PRINT "FÖR STORT." : GOTO 60',
        '100 PRINT "RÄTT PÅ ";N;"FÖRSÖK!"',
        '110 END'] },
];

/* Inläggen som finns när man ringer. Avsändarna är påhittade alias. */
const BBS_INLAGG = [
    { fran: 'SYSOP', arende: 'Välkommen till Bytesboden', text:
        'Hej och välkommen! Bytesboden är öppen dygnet runt på en linje,\n' +
        'så fatta dig kort och lägg på när du är klar, andra vill också in.\n' +
        'Under F finns program att hämta. Skriv gärna ett inlägg om vad du\n' +
        'kör på din ABC.' },
    { fran: 'MODEMKATTEN', arende: '300 eller 1200?', text:
        'Är det någon som har fått 1200 bit/s att fungera mot en ABC 802?\n' +
        'Med 300 tar en sida en halv minut, men det går ju att läsa medan\n' +
        'den kommer.' },
    { fran: 'SYSOP', arende: 'Sv: 300 eller 1200?', text:
        'Bytesboden kör 300 åt båda hållen. 1200 in och 75 ut är vad\n' +
        'Datavision använder, och det går att ställa in på options-PROM:et\n' +
        '(typ W). Här räcker LOAD V24:TSA30B24.22A.' },
    { fran: 'BITSMEDEN', arende: 'Skärmen blir tom', text:
        'Tips: det är ^Z som tömmer skärmen i terminalen, som på en ADM-3A.\n' +
        'Kommer du ur terminalen med fel 53 så var det en PF-tangent.' },
    { fran: 'NATTUGGLAN', arende: 'GISSA.BAS', text:
        'Hämtade GISSA i går natt. Klarade det på sex försök. Går det att\n' +
        'göra en variant där datorn gissar i stället?' },
];

/* De andra som "är inne" eller var det nyss. */
const BBS_SENAST = [
    ['NATTUGGLAN', '23.12'], ['BITSMEDEN', '21.40'], ['MODEMKATTEN', '19.05'],
];

function skapa_bbs(sand, ko_langd) {
    let uppkopplad = false;
    let namn = '';
    let start = 0;
    let rad = '';                            /* det som skrivs just nu */
    let dold = false;                        /* eko med * (lösenordet) */
    let max = 70;
    let nar_rad = null;                      /* får raden vid RETURN */
    let nar_tangent = null;                  /* får nästa tangent */
    let vantar = false;                      /* tangenterna räknas inte */
    let klocka = null;
    let anrop = 1283;
    const inlagg = BBS_INLAGG.map(i => ({ ...i }));

    function skriv(text) {
        sand(bbs_bytes(text));
    }

    function fraga(text, gor, { hemlig = false, langd = 70 } = {}) {
        skriv(text);
        rad = '';
        dold = hemlig;
        max = langd;
        nar_rad = gor;
        nar_tangent = null;
    }

    function meny() {
        skriv('\nHUVUDMENY\n' +
              '  M  Mötet, ' + inlagg.length + ' inlägg\n' +
              '  F  Filer att hämta\n' +
              '  V  Vem är inne\n' +
              '  T  Tiden\n' +
              '  H  Hjälp\n' +
              '  A  Avsluta och lägg på\n');
        fraga('Val: ', val_meny, { langd: 10 });
    }

    function val_meny(svar) {
        const c = svar.trim().toUpperCase()[0];
        if (c === 'M')
            return motet();
        if (c === 'F')
            return filer();
        if (c === 'V') {
            skriv('\nINNE NU\n  Linje 1  ' + namn + ', du\n\nSENAST INNE\n' +
                  BBS_SENAST.map(([n, k]) => '  ' + n.padEnd(14) + k).join('\n') + '\n');
            return meny();
        }
        if (c === 'T') {
            const d = new Date();
            const tid = String(d.getHours()).padStart(2, '0') + '.'
                      + String(d.getMinutes()).padStart(2, '0');
            const min = Math.floor((Date.now() - start) / 60000);
            skriv('\nKlockan är ' + tid + '. Du har varit inne i ' + min +
                  (min === 1 ? ' minut' : ' minuter') + '.\n' +
                  'Linjen går med 300 bit/s åt båda hållen, ungefär 30 tecken i sekunden.\n');
            return meny();
        }
        if (c === 'H') {
            skriv('\nHJÄLP\n' +
                  'Skriv bokstaven för det du vill göra och tryck RETURN. Med vänsterpilen\n' +
                  'går det att sudda. Allt du skriver kommer tillbaka från BBS:en: terminalen\n' +
                  'visar inte själv det du trycker.\n' +
                  'Ur terminalen kommer du med PF1 (fel 53), och tillbaka hit med\n' +
                  'LOAD V24:TSA30B24.22A. Bytesboden är en rekonstruktion, inte en\n' +
                  'BBS som har funnits.\n');
            return meny();
        }
        if (c === 'A')
            return avsluta();
        if (c)
            skriv('Det finns inget val ' + c + '. Skriv H för hjälp.\n');
        fraga('Val: ', val_meny, { langd: 10 });
    }

    function motet() {
        skriv('\nMÖTET: ABC ALLMÄNT\n Nr  Från          Ärende\n' +
              inlagg.map((i, n) => String(n + 1).padStart(3) + '  ' +
                         i.fran.slice(0, 12).padEnd(14) + i.arende).join('\n') + '\n' +
              'Ett nummer läser inlägget, S skriver ett nytt, RETURN går till menyn.\n');
        fraga('Mötet: ', val_motet, { langd: 10 });
    }

    function val_motet(svar) {
        svar = svar.trim().toUpperCase();
        if (!svar)
            return meny();
        if (svar[0] === 'S') {
            skriv('\nNytt inlägg från ' + namn + '.\n');
            return fraga('Ärende: ', (arende) => {
                arende = arende.trim();
                if (!arende) {
                    skriv('Inget ärende, inget inlägg.\n');
                    return motet();
                }
                skriv('Skriv texten. En rad med bara en punkt avslutar.\n');
                const rader = [];
                const nasta = (r) => {
                    if (r.trim() === '.') {
                        inlagg.push({ fran: namn, arende, text: rader.join('\n') });
                        skriv('Inlägget är nummer ' + inlagg.length + '. Tack!\n');
                        return motet();
                    }
                    rader.push(r);
                    fraga('', nasta);
                };
                fraga('', nasta);
            }, { langd: 40 });
        }
        const n = parseInt(svar, 10);
        if (n >= 1 && n <= inlagg.length) {
            const i = inlagg[n - 1];
            skriv('\nInlägg ' + n + ' av ' + inlagg.length + '\nFrån:   ' + i.fran +
                  '\nÄrende: ' + i.arende + '\n\n' + i.text + '\n');
        } else
            skriv('Det finns inget inlägg ' + svar + '.\n');
        fraga('Mötet: ', val_motet, { langd: 10 });
    }

    function storlek(f) {
        return f.text.reduce((s, r) => s + r.length + 1, 0);
    }

    function filer() {
        skriv('\nFILER\n Nr  Namn          Tecken  Om\n' +
              BBS_FILER.map((f, n) => String(n + 1).padStart(3) + '  ' +
                            f.namn.padEnd(12) + String(storlek(f)).padStart(8) +
                            '  ' + f.om).join('\n') + '\n' +
              'Numret hämtar filen, RETURN går till menyn.\n');
        fraga('Filer: ', val_filer, { langd: 10 });
    }

    function val_filer(svar) {
        svar = svar.trim();
        if (!svar)
            return meny();
        const n = parseInt(svar.replace(/^D\s*/i, ''), 10);
        if (!(n >= 1 && n <= BBS_FILER.length)) {
            skriv('Det finns ingen fil ' + svar + '.\n');
            return fraga('Filer: ', val_filer, { langd: 10 });
        }
        hamta(BBS_FILER[n - 1]);
    }

    function hamta(f) {
        const s = storlek(f);
        skriv('\nDu hämtar ' + f.namn + ', ' + f.text.length + ' rader och ' + s +
              ' tecken, ungefär ' + Math.ceil(s / 30) + ' sekunder.\n' +
              'Gör så här:\n' +
              '  1. Tryck PF1. Terminalen slutar med fel 53.\n' +
              '  2. Skriv LOAD V24:VSA30B24.22A och RETURN.\n' +
              '  3. När programmet har kommit och inget mer händer, tryck BREAK.\n' +
              '  4. LIST visar det och RUN kör det.\n' +
              '  5. Skriv LOAD V24:TSA30B24.22A och tryck en tangent för att\n' +
              '     komma tillbaka hit.\n' +
              'Sändningen börjar om 30 sekunder.\n');
        nar_rad = null;
        nar_tangent = null;
        vantar = true;
        /* Först när texten har gått ut börjar väntan. */
        klocka = setInterval(() => {
            if (ko_langd() > 0)
                return;
            clearInterval(klocka);
            klocka = setTimeout(() => {
                klocka = null;
                sand(bbs_bytes(f.text.join('\n') + '\n', [0x0D]));
                vantar = false;
                nar_tangent = () => {
                    sand([0x1A]);
                    skriv('Välkommen tillbaka, ' + namn + '.\n');
                    meny();
                };
            }, 30000);
        }, 250);
    }

    function avsluta() {
        const min = Math.max(1, Math.round((Date.now() - start) / 60000));
        skriv('\nTack för samtalet, ' + namn + '. Du var inne i ' + min +
              (min === 1 ? ' minut' : ' minuter') + '.\nVälkommen åter!\n\n' +
              '(Bytesboden lägger på.)\n');
        lagg_pa();
    }

    function stoppa_klockan() {
        if (klocka !== null) {
            clearInterval(klocka);
            clearTimeout(klocka);
            klocka = null;
        }
    }

    function ring() {
        stoppa_klockan();
        uppkopplad = true;
        vantar = false;
        start = Date.now();
        anrop++;
        sand([0x1A]);
        skriv('BYTESBODEN, en BBS för ABC 80, 800 och 802\n' +
              '==========================================\n' +
              'En rekonstruktion: den här BBS:en har inte funnits, och allt\n' +
              'i den är påhittat för att visa hur ett samtal gick till.\n\n' +
              'Linje 1, 300 bit/s. Du är uppringare nummer ' + anrop + '.\n' +
              'Vilket namn och lösenord som helst går bra.\n\n');
        fraga('Namn: ', function nar_namn(n) {
            n = n.trim();
            if (!n)
                return fraga('Namn: ', nar_namn, { langd: 20 });
            namn = n.toUpperCase();
            fraga('Lösenord: ', () => {
                skriv('\nVälkommen, ' + namn + '! Det finns ' + inlagg.length +
                      ' inlägg i Mötet.\n');
                meny();
            }, { hemlig: true, langd: 20 });
        }, { langd: 20 });
    }

    function lagg_pa() {
        stoppa_klockan();
        uppkopplad = false;
        nar_rad = null;
        nar_tangent = null;
    }

    function tecken(b) {
        b &= 0x7F;
        if (!uppkopplad || vantar || b === 0x11 || b === 0x13)
            return;
        if (nar_tangent) {
            const f = nar_tangent;
            nar_tangent = null;
            return f(b);
        }
        if (!nar_rad)
            return;
        if (b === 0x0D) {
            skriv('\n');
            const f = nar_rad;
            nar_rad = null;
            return f(rad);
        }
        if (b === 0x08 || b === 0x7F) {
            if (rad) {
                rad = rad.slice(0, -1);
                sand([0x08, 0x20, 0x08]);
            }
            return;
        }
        if (b < 0x20 || rad.length >= max)
            return;
        const c = BBS_TILLBAKA[b] || String.fromCharCode(b);
        rad += c;
        sand(dold ? [0x2A] : [b]);
    }

    return { ring, lagg_pa, tecken, uppkopplad: () => uppkopplad };
}

if (typeof module !== 'undefined')
    module.exports = { skapa_bbs, bbs_bytes, BBS_FILER };
