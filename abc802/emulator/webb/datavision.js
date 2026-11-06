/*
 * datavision.js -- en Datavisionscentral att ringa till från CH B, till
 * ABC 802 inifrån
 *
 * En REKONSTRUKTION, inte Televerkets tjänst: sidorna, numren och
 * texterna är gjorda här för att visa hur samtalet gick till. Det som
 * står om Datavision (1 200/75 bit/s, 30 öre i minuten, samma textgrafik
 * som text-tv) är hämtat ur ABC-bladet 1983:3 och 1985:1 och 3, via ABC80
 * inifrån band 1, kapitlet Marknaden.
 *
 * ABC 802 är terminalen: LOAD V24:WEA10K24.40A (options-PROM:et,
 * manualen s. 7-8, typ W; ROM:en $7000, TERM och DVTAB i
 * dis/build/abc802rom.asm). Det maskinen tar emot visas på skärmen:
 *     FF ($0C)           töm skärmen (till CON:)
 *     ^^ ($1E)           markören hem
 *     ^J ($0A), ^K ($0B) ned, upp (från rad 0 till rad 23)
 *     ^M ($0D)           till kolumn 0 (till CON:)
 *     ^Q ($11), ^T ($14) markören på, av
 *     ESC x              attributet (x AND $1F) OR $80, som i text-tv:
 *                        $81-$87 text i färg, $91-$97 mosaik i färg,
 *                        $9C svart bakgrund, $9D ny bakgrund
 *     STX ($02)          slut på terminalen; resten av raden till CR blir
 *                        det som INPUT #1 läser
 * Det tangenterna ger kommer tillbaka med paritet i bit 7, som tas bort.
 * Som i Prestel: *nn# går till sidan nn, *# tillbaka, ** ångrar, en
 * siffra är ett val på sidan och # nästa ram eller innehållet.
 *
 * Varje rad skrivs med högst 40 celler, där ett attribut tar en cell.
 * Raden slutar med ^M ^J när den är kortare än 40, så att det inte spelar
 * någon roll om terminalen själv byter rad ([6] bit 1); en full rad följs
 * av ^^ och så många ^J som behövs. Rad 23 är raden där det man skriver
 * syns.
 *
 * skapa_datavision(sand) ger { ring, lagg_pa, tecken, uppkopplad }: sand()
 * får bytes till maskinen, och tecken() tar ett tecken från maskinen.
 */

/* Tecknen som inte är ASCII, som i ABC 802:s svenska teckensats. */
const DV_SVENSKA = { 'å': 0x7D, 'ä': 0x7B, 'ö': 0x7C, 'é': 0x60, 'ü': 0x7E,
                     'Å': 0x5D, 'Ä': 0x5B, 'Ö': 0x5C, 'É': 0x40, 'Ü': 0x5E };

/* Attributen i sidtexten: {r} röd text, {R} röd mosaik osv. */
const DV_FARG = 'rgybmcw';
const DV_ATTR = { nb: 0x9D, sb: 0x9C, fl: 0x88, st: 0x89, sep: 0x9A, hel: 0x99 };

/* En rad i sidtexten som celler: varje cell är en lista med bytes. */
function dv_celler(text) {
    const celler = [];
    let i = 0;
    while (i < text.length) {
        const c = text[i];
        if (c === '{') {
            const slut = text.indexOf('}', i);
            const namn = text.slice(i + 1, slut);
            i = slut + 1;
            let attr;
            if (namn.length === 1 && DV_FARG.includes(namn))
                attr = 0x81 + DV_FARG.indexOf(namn);
            else if (namn.length === 1 && DV_FARG.includes(namn.toLowerCase()))
                attr = 0x91 + DV_FARG.indexOf(namn.toLowerCase());
            else if (namn in DV_ATTR)
                attr = DV_ATTR[namn];
            if (attr !== undefined)
                celler.push([0x1B, 0x40 | (attr & 0x1F)]);
            else if (namn[0] === '#')       /* {#7f}: en byte som den är */
                celler.push([parseInt(namn.slice(1), 16) & 0x7F]);
            else
                throw new Error('okänt attribut {' + namn + '}');
            continue;
        }
        const k = DV_SVENSKA[c] ?? c.charCodeAt(0);
        celler.push([k >= 0x20 && k < 0x7F ? k : 0x3F]);
        i++;
    }
    while (celler.length && celler[celler.length - 1][0] === 0x20)
        celler.pop();
    if (celler.length > 40)
        throw new Error('raden är längre än 40: ' + text);
    return celler;
}

/* En bild ('#' och '.') som mosaik: 2 x 3 punkter per tecken. */
function dv_mosaik(bild) {
    const bredd = Math.max(...bild.map(r => r.length));
    const satt = (x, y) => (bild[y] || '')[x] === '#';
    const rader = [];
    for (let y = 0; y < bild.length; y += 3) {
        let rad = '';
        for (let x = 0; x < bredd; x += 2) {
            let b = 0x20;
            if (satt(x, y)) b |= 0x01;
            if (satt(x + 1, y)) b |= 0x02;
            if (satt(x, y + 1)) b |= 0x04;
            if (satt(x + 1, y + 1)) b |= 0x08;
            if (satt(x, y + 2)) b |= 0x10;
            if (satt(x + 1, y + 2)) b |= 0x40;
            rad += '{#' + b.toString(16) + '}';
        }
        rader.push(rad);
    }
    return rader;
}

/* Ordet DATAVISION i mosaik, fem punkter högt och dubbelt så högt. */
function dv_logga() {
    const typ = {
        D: ['##.', '#.#', '#.#', '#.#', '##.'],
        A: ['.#.', '#.#', '###', '#.#', '#.#'],
        T: ['###', '.#.', '.#.', '.#.', '.#.'],
        V: ['#.#', '#.#', '#.#', '#.#', '.#.'],
        I: ['#', '#', '#', '#', '#'],
        S: ['###', '#..', '###', '..#', '###'],
        O: ['###', '#.#', '#.#', '#.#', '###'],
        N: ['#..#', '##.#', '#.##', '#..#', '#..#'],
    };
    const rader = ['', '', '', '', ''];
    for (const bokstav of 'DATAVISION')
        typ[bokstav].forEach((r, y) => { rader[y] += r + '.'; });
    const hog = [];
    for (const r of rader)
        hog.push('..' + r, '..' + r);
    return dv_mosaik(hog);
}

/* ---------------------------------------------------------------- */
/* Sidorna. Rad 1-21 står i rader; rad 0 är huvudet och rad 22 foten. */

const DV_SIDOR = {
    '0': {
        rader: [
            '',
            ...dv_logga().map(r => '{nb}{B}' + r),
            '',
            '{y}      Välkommen till Datavision',
            '',
            '{w}  Det här är en rekonstruktion av en',
            '{w}  teledatatjänst från 1980-talet, gjord',
            '{w}  för emulatorn. Sidorna är nya.',
            '',
            '{c}  ABC 802 är terminalen: tecknen tolkas',
            '{c}  av options-PROM:et (typ W).',
            '',
            '',
            '{g}  Tryck {w}#{g} för innehållet.',
        ],
        nasta: '1',
    },
    '1': {
        rader: [
            '{nb}{b}{y} INNEHÅLL                           ',
            '',
            '{w} 2 {c}Nytt om modem och Datavision',
            '{w} 3 {c}Vädret (exempel)',
            '{w} 4 {c}ABC-klubben',
            '{w} 5 {c}Om Datavision',
            '{w} 6 {c}Så använder du tangenterna',
            '{w} 7 {c}Bildtest: färger och mosaik',
            '{w} 9 {c}Hämta data till ett BASIC-program',
            '',
            '{y} Välj med en siffra, eller skriv',
            '{y} {w}*nn#{y} för att gå till sidan nn.',
        ],
        val: { 2: '2', 3: '3', 4: '4', 5: '5', 6: '6', 7: '7', 9: '9' },
    },
    '2': {
        rader: [
            '{nb}{m}{w} NYTT                               ',
            '',
            '{y} Modemmonopolet släpps',
            '{w} Från den 1 juli 1983 får man själv',
            '{w} köpa typgodkända modem för 300 bit/s',
            '{w} och för 1 200 bit/s med returkanal på',
            '{w} 75 bit/s. Ett modem för 1 200 åt båda',
            '{w} hållen hyr man av Televerket.',
            '',
            '{y} 1 200 och 75',
            '{w} Hastigheterna kom till för Datavision.',
            '{w} Sidan kommer i 1 200 bit/s, och det',
            '{w} man skriver går tillbaka i 75 bit/s.',
            '',
            '{g} Källa: ABC-bladet 1983:3.',
        ],
        nasta: '1',
    },
    '3': {
        rader: [
            '{nb}{b}{w} VÄDRET                {y}  EXEMPEL     ',
            '',
            '{w} Påhittade siffror, för att visa hur',
            '{w} en sida med en tabell kunde se ut.',
            '',
            '{c} Ort          Idag       Imorgon',
            '{w} Malmö      {y} +4 {w}mulet  {y} +5 {w}regn',
            '{w} Göteborg   {y} +3 {w}regn   {y} +4 {w}mulet',
            '{w} Stockholm  {y} +1 {w}mulet  {y} +2 {w}sol',
            '{w} Östersund  {c} -6 {w}snö    {c} -8 {w}sol',
            '{w} Luleå      {c}-12 {w}sol    {c}-10 {w}snö',
            '',
            '{G}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}{#7f}',
        ],
        nasta: '1',
    },
    '4': {
        rader: [
            '{nb}{g}{b} ABC-KLUBBEN                        ',
            '',
            '{w} Klubbens monitor är en obemannad ABC80',
            '{w} med en skivenhet i klubblokalen i',
            '{w} Alvik. Den har gått sedan den 29',
            '{w} januari 1981; man ringer upp och',
            '{w} hämtar och lämnar program.',
            '',
            '{w} Under oktober och november 1981',
            '{w} loggade medlemmarna in 1 832 gånger.',
            '{w} Knappt en femtedel hade ett modem för',
            '{w} 300 bit/s.',
            '',
            '{w} Klubben har också fått några sidor',
            '{w} här i Datavision.',
            '',
            '{g} Källa: ABC-bladet 1981 och 1985.',
        ],
        nasta: '1',
    },
    '5': {
        rader: [
            '{nb}{c}{b} OM DATAVISION                      ',
            '',
            '{w} Datavision har samma textgrafik som',
            '{w} text-tv: 24 rader med 40 tecken, sju',
            '{w} färger och mosaik i 2 x 3 punkter.',
            '',
            '{w} Man behöver ett konto hos en',
            '{w} informationslämnare. Det kostar 30 öre',
            '{w} i minuten och 15 kronor i kvartalet,',
            '{w} och samtalet kostar lokaltaxa var man',
            '{w} än ringer ifrån.',
            '',
            '{w} Postverkets POSTEL är den enda tjänst',
            '{w} som vänder sig till allmänheten.',
            '',
            '{g} Källa: ABC-bladet 1983:3, 1985:3.',
        ],
        nasta: '1',
    },
    '6': {
        rader: [
            '{nb}{y}{b} TANGENTERNA                        ',
            '',
            '{w} {y}*nn#{w}  går till sidan nn',
            '{w} {y}*#{w}    tillbaka till sidan innan',
            '{w} {y}**{w}    ångrar det du har skrivit',
            '{w} {y}#{w}     nästa sida, eller innehållet',
            '{w} {y}0-9{w}   ett val på sidan',
            '{w} {y}RETURN{w} som #',
            '',
            '{c} Det du skriver går ut på CH B med',
            '{c} paritet och syns på rad 23 när',
            '{c} centralen har tagit emot det.',
            '',
            '{c} Ur terminalen kommer du med RESET,',
            '{c} eller med en PF-tangent som inte är',
            '{c} bestämd (F1-F8 här; fel 53).',
        ],
        nasta: '1',
    },
    '7': {
        rader: [
            '{nb}{w}{b} BILDTEST                           ',
            '',
            '{r}Röd {g}Grön {y}Gul {b}Blå {m}Magenta {c}Cyan {w}Vit',
            '',
            '{nb}{r}{nb}{g}{nb}{y}{nb}{b}{nb}{m}{nb}{c}{nb}{w}{nb}{sb}',
            '',
            '{R}{#7f}{#7f}{#7f}{G}{#7f}{#7f}{#7f}{Y}{#7f}{#7f}{#7f}{B}{#7f}{#7f}{#7f}{M}{#7f}{#7f}{#7f}{C}{#7f}{#7f}{#7f}{W}{#7f}{#7f}{#7f}',
            '',
            '{W}{#21}{#22}{#24}{#28}{#30}{#60}{#23}{#2c}{#70}{#35}{#6a}{#3f}{#7e}{#7f}',
            '',
            '{fl}{y} Blinkar{st}{w} står still',
            '',
            '{c} ABC 802 har en svartvit skärm, så',
            '{c} färgerna och bakgrunden på rad 4',
            '{c} syns inte; attributen blir tomma',
            '{c} celler. Rad 6 har hela mosaikceller',
            '{c} och rad 8 de enskilda punkterna.',
        ],
        nasta: '1',
    },
    '9': {
        rader: [
            '{nb}{g}{b} HÄMTA DATA                         ',
            '',
            '{w} Ett BASIC-program kan läsa från',
            '{w} Datavision: centralen skickar STX',
            '{w} ($02), och raden efter den, till CR,',
            '{w} blir det som INPUT läser.',
            '',
            '{c} 5 DIM A$=80',
            '{c} 10 OPEN "V24:WEA10K24.40A" AS FILE 1',
            '{c} 20 INPUT #1,A$',
            '{c} 30 PRINT "FICK: ";A$',
            '',
            '{w} Kör programmet, välj sidan 9 och',
            '{w} tryck {y}1{w}.',
            '',
            '{y} 1 {w}Skicka vädret till programmet',
        ],
        val: { 1: '91' },
        nasta: '1',
    },
    '91': {
        rader: [
            '{nb}{g}{b} HÄMTA DATA                         ',
            '',
            '{w} Vädret skickas efter STX.',
        ],
        data: 'STOCKHOLM +1 MULET  IMORGON +2 SOL',
        nasta: '1',
    },
};

/* ---------------------------------------------------------------- */

function skapa_datavision(sand) {
    let uppkopplad = false;
    let sida = '0';
    let historik = [];
    let inmatning = '';

    function bytes_av(celler) {
        const ut = [];
        for (const cell of celler)
            ut.push(...cell);
        return ut;
    }

    /* Rad 0: tjänsten, sidnumret och priset. */
    function huvud(nr) {
        const n = (nr + 'a').padStart(4);
        return '{w}DATAVISION {y}rekonstr. {c}' + n + '{w}   0 öre';
    }

    function fot() {
        return '{nb}{b}{y} #{w} vidare {y}*nn#{w} sida {y}*#{w} bakåt';
    }

    /* Rad 23 med text, och resten av raden tom (högst 39 celler).
     * Markören står sedan först på raden, där BASIC:en skriver efter STX. */
    function rad23(text) {
        const celler = dv_celler(text);
        while (celler.length < 39)
            celler.push([0x20]);
        sand([0x1E, 0x0B, ...bytes_av(celler.slice(0, 39)), 0x1E, 0x0B]);
    }

    function visa_rad23() {
        rad23(inmatning ? '{c}Sida: {w}' + inmatning : '');
    }

    function visa(nr, bakat = false) {
        const s = DV_SIDOR[nr];
        if (!s) {
            rad23('{r}Sidan ' + nr + ' finns inte');
            return;
        }
        if (!bakat && nr !== sida)
            historik.push(sida);
        sida = nr;
        const rader = [huvud(nr), ...s.rader];
        while (rader.length < 22)
            rader.push('');
        rader.length = 22;
        rader.push(fot());
        const ut = [0x14, 0x0C];             /* markören av, töm skärmen */
        rader.forEach((text, rad) => {
            const celler = dv_celler(text);
            ut.push(...bytes_av(celler));
            if (celler.length < 40)
                ut.push(0x0D, 0x0A);
            else {
                ut.push(0x1E);
                for (let i = 0; i <= rad; i++)
                    ut.push(0x0A);
            }
        });
        sand(ut);
        inmatning = '';
        visa_rad23();
        if (s.data !== undefined) {
            const data = [];
            for (const c of s.data)
                data.push(DV_SVENSKA[c] ?? c.charCodeAt(0));
            sand([0x02, ...data, 0x0D]);
        }
    }

    function ring() {
        uppkopplad = true;
        historik = [];
        inmatning = '';
        sida = '0';
        visa('0', true);
    }

    function lagg_pa() {
        uppkopplad = false;
    }

    function tecken(b) {
        if (!uppkopplad)
            return;
        const c = String.fromCharCode(b & 0x7F);
        const slut = c === '#' || c === '_' || c === '\r';
        if (inmatning) {
            if (c === '*' && inmatning === '*')
                inmatning = '';
            else if (/[0-9]/.test(c) && inmatning.length < 4)
                inmatning += c;
            else if (c === '\b')
                inmatning = inmatning.slice(0, -1);
            else if (slut) {
                const nr = inmatning.slice(1).replace(/^0+(?=\d)/, '');
                inmatning = '';
                if (nr)
                    visa(nr);
                else if (historik.length)
                    visa(historik.pop(), true);
                else
                    visa_rad23();
                return;
            } else
                return;
            visa_rad23();
            return;
        }
        const s = DV_SIDOR[sida];
        if (c === '*') {
            inmatning = '*';
            visa_rad23();
        } else if (slut)
            visa(s.nasta || '1');
        else if (/[0-9]/.test(c)) {
            if (c === '0')
                visa('1');
            else if (s.val && s.val[c])
                visa(s.val[c]);
            else
                rad23('{r}Inget val ' + c + ' på sidan');
        }
    }

    return { ring, lagg_pa, tecken, uppkopplad: () => uppkopplad };
}

if (typeof module !== 'undefined')
    module.exports = { skapa_datavision, dv_celler, DV_SIDOR };
