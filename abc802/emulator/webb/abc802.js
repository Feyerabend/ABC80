/*
 * abc802.js -- emulatorn i webbläsaren, till ABC 802 inifrån
 *
 * Kärnan är WebAssembly (webb.c och karna/, make webb); här finns allt
 * runt den: skärmen, tangentbordet, pipet och knapparna. bygg.sh lägger
 * in WebAssembly-koden, ROM:en och teckengeneratorn som base64 i FILER,
 * så att sidan inte behöver hämta något. Sidan är byggd som ABC80:s
 * (abc80/emulator/webb/abc80.js), men mindre.
 *
 * Tiden: en bild är 20 ms och följer performance.now(). Medan det finns
 * kvar i tangentkön (inklistrad text, en fil, ?k=) går maskinen så fort
 * den hinner, som i SDL-fönstret.
 *
 * Skärmen ritas som i sdl/fonster.c: punkterna (480 x 240) blir 2 x 3
 * bildpunkter i en bild om 1056 x 792 med en svart kant. Det som tänds
 * lyser direkt, och det som släcks klingar av med tidskonstanten
 * EFTERLYS: varje gång bilden ritas läggs svart över med täckningen
 * 1 - exp(-dt / EFTERLYS), och sedan de tända punkterna. Skenet är en
 * kopia på canvasen sken, suddad med CSS (filter: blur, med radien
 * räknad om till skärmens storlek) och lagd ovanpå.
 *
 * Pipet: kod 7 till tangentbordet ger en ton. Tonen och längden är inte
 * kända (tangentbordets 2716 finns inte); 1 kHz i 100 ms antas (?), som
 * i fönstret.
 *
 * Skivminnet: en avbild i MF0 (skiva() i webb.c). Tidsproven och den
 * tomma skivan ligger packade med gzip i FILER och packas upp med
 * DecompressionStream. Katalogen (sektor 16-23, 16 bytes per fil, som
 * gor.py i prov/skivor läser den) visas i panelen och läses om en gång
 * i sekunden.
 *
 * Kassetten och kontakterna: bandet ligger i webb.c (band(),
 * tomt_band(), spola()); Spara hämtar det därifrån. Det som går ut på
 * CH A och CH B hämtas efter varje varv och sparas här per kontakt (som
 * bytes, för Spara), och den valda kontakten visas som text med å ä ö
 * för ABC 802:s koder. Det som skickas till en kontakt köas här och
 * lämnas till webb.c så fort maskinens kö har plats. Skrivarkortet hinner
 * 80 tecken i sekunden (skrivarens_fart() i webb.c), eller allt genast.
 *
 * Nätet: nodkortet och centralen i LUXNET (natkort.c); med nodkortet i
 * startar maskinen med nät-ROM:en (FILER.natrom). Filerna i UFD: ligger
 * i webb.c; text blir poster här när den läggs in och text igen när den
 * hämtas. Trafiken hämtas efter varje varv, som utdata.
 *
 * Lamporna i panelernas list (skivan, nätet, kassetten, kontakterna) och Av/på
 * är gjorda som i ABC80-emulatorn.
 *
 * Tangenterna går som i fönstret: Enter är RETURN ($0D), backsteg och
 * vänsterpil $08, högerpil och Tab $09, Esc $1B, Delete $7F, Ctrl och en
 * bokstav $01-$1A, och å ä ö é ü Å Ä Ö É Ü blir ABC 802:s koder. Text
 * som klistras in eller skrivs in från en fil får en paus på 200 ms
 * efter varje rad, som -f.
 */

'use strict';

const BILD = 0.02;                           /* sekunder per bild */
const SNABBT_MS = 14;                        /* körtid per skärmbild när det går fort */
const EFTERLYS = 0.020;                      /* efterlysningens tidskonstant, s */
const SKEN = 2.5;                            /* skenets standardavvikelse, bildpunkter */
const BREDD = 480, HOJD = 250;               /* ABC802_BREDD, ABC802_HOJD */
const KANT_X = 48, KANT_Y = 21;              /* svart kant runt bilden */
const SB = 2 * BREDD + 2 * KANT_X;           /* 1056 */
const SH = 3 * HOJD + 2 * KANT_Y;            /* 792 */
const FARG = [255, 176, 0];                  /* bärnsten, #FFB000 */
/* Färgerna 0-7 med valet Färg (om), som i ABC 800 C (utanför boken). */
const FARGER = [[0, 0, 0], [255, 40, 40], [40, 230, 40], [255, 240, 40],
                [70, 90, 255], [255, 40, 255], [40, 240, 240], [255, 255, 255]];
const PIP_HZ = 1000, PIP_MS = 100;           /* (?) */

const SVENSKA = { 'å': 0x7D, 'ä': 0x7B, 'ö': 0x7C, 'é': 0x60, 'ü': 0x7E,
                  'Å': 0x5D, 'Ä': 0x5B, 'Ö': 0x5C, 'É': 0x40, 'Ü': 0x5E, '¤': 0x24 };

const $ = (id) => document.getElementById(id);
const skarm = $('skarm');
const ritning = skarm.getContext('2d');
const sken = $('sken');
const sken_ritning = sken.getContext('2d');

let e;                                       /* WebAssemblys exporter */
let nasta = null;                            /* klockans tid för nästa bild */
const ROM = {};

function base64(text) {
    const s = atob(text);
    const b = new Uint8Array(s.length);
    for (let k = 0; k < s.length; k++)
        b[k] = s.charCodeAt(k);
    return b;
}

/* ---------------------------------------------------------------- */
/* Språket                                                           */

/* sv eller en, som i ABC80:s sida: mallens text står på båda med lang,
 * och CSS döljer den som inte gäller; knappar har engelskan i data-en
 * och title-texten i data-title-en. Valet sparas i webbläsaren; ?sprak=
 * går före, och utan något val gäller webbläsarens språk. */
const SPRAK_NYCKEL = 'abc802_sprak';
let sprak = 'sv';
let senaste = ['', ''];                      /* meddelandet på båda språken */

function t(sv, en) {
    return sprak === 'en' ? en : sv;
}

function meddela(sv = '', en = sv) {
    senaste = [sv, en];
    $('meddelande').textContent = t(sv, en);
}

function byt_sprak(nytt) {
    sprak = nytt === 'en' ? 'en' : 'sv';
    document.documentElement.lang = sprak;
    for (const el of document.querySelectorAll('[data-en]')) {
        if (el.dataset.sv === undefined)
            el.dataset.sv = el.textContent;
        el.textContent = t(el.dataset.sv, el.dataset.en);
    }
    for (const el of document.querySelectorAll('[data-title-en]')) {
        if (el.dataset.titleSv === undefined)
            el.dataset.titleSv = el.title;
        el.title = t(el.dataset.titleSv, el.dataset.titleEn);
    }
    $('sprak').textContent = t('EN', 'SV');
    $('meddelande').textContent = t(...senaste);
    visa_ljudet();
    visa_lagen();
    if (e) {
        visa_kassetten();
        visa_kontakterna();
    }
}

$('sprak').addEventListener('click', () => {
    byt_sprak(t('en', 'sv'));
    try { localStorage.setItem(SPRAK_NYCKEL, sprak); } catch (fel) { /* sparas inte */ }
    skarm.focus();
});

/* ---------------------------------------------------------------- */
/* Panelerna                                                         */

/* Vilka som är öppna sparas; från början bara Maskinen. i i listen
 * fäller ut panelen och visar eller döljer dess hjälptext. */
const PANEL_NYCKEL = 'abc802_paneler';
const paneler = [...document.querySelectorAll('details.panel')];

function paneler_spara() {
    try {
        localStorage.setItem(PANEL_NYCKEL,
            JSON.stringify(paneler.filter(p => p.open).map(p => p.id)));
    } catch (fel) { /* sparas inte */ }
}

function paneler_las_in() {
    let oppna = null;
    try { oppna = JSON.parse(localStorage.getItem(PANEL_NYCKEL)); } catch (fel) { /* inget */ }
    if (Array.isArray(oppna))
        for (const p of paneler)
            p.open = oppna.includes(p.id);
}

for (const p of paneler) {
    p.addEventListener('toggle', paneler_spara);
    const info = p.querySelector('.info');
    const text = p.querySelector('.hjalptext');
    info.addEventListener('click', (h) => {
        h.preventDefault();                  /* fäll inte ihop */
        const visa = !p.open || text.hidden;
        p.open = true;
        text.hidden = !visa;
        info.classList.toggle('pa', visa);
    });
}

function visa_lagen() {
    $('lage_maskin').textContent = t($('tecken').value + ' tecken', $('tecken').value + ' columns')
        + ' · ' + $('hz').value + ' Hz';
    $('lage_skarm').textContent = ($('farg').checked ? t('färg (om) · ', 'colour (what if) · ') : '')
        + ($('skarp').checked ? t('skarp', 'sharp') : t('fosfor', 'phosphor'));
}

/* ---------------------------------------------------------------- */
/* Maskinen                                                          */

/* Bytes in i WebAssemblys minne; pekaren. */
function lagg_i_minnet(data) {
    const p = e.minne(data.length);
    new Uint8Array(e.memory.buffer, p, data.length).set(data);
    return p;
}

/* Töm maskinen och starta med byglarna i valen. */
function starta() {
    const natet = $('natet').checked;
    const rom_data = natet ? ROM.natrom : ROM.rom;
    const rom = lagg_i_minnet(rom_data);
    const tecken = lagg_i_minnet(ROM.tecken);
    e.natet(natet ? 1 : 0);
    e.starta(rom, rom_data.length, tecken, ROM.tecken.length,
             $('tecken').value === '80' ? 1 : 0, $('hz').value === '60' ? 1 : 0);
    e.skrivarens_fart(+$('skrivarfart').value);
    e.slapp(rom);
    e.slapp(tecken);
    pipen = e.pip();
    nasta = null;
    in_ko[0] = [];
    in_ko[1] = [];
}

$('aterstall').addEventListener('click', () => {
    e.aterstall();
    meddela('RESET: programmet ligger kvar.', 'RESET: the program is kept.');
    skarm.focus();
});

$('nystart').addEventListener('click', () => {
    starta();
    meddela('Maskinen är startad om med tomt minne.', 'The machine has restarted with empty memory.');
    skarm.focus();
});

/* Av/på: sidan hämtas om utan frågan i adressen, så att inga val,
 * skivor, band eller kort följer med (som i ABC80-emulatorn). Språket,
 * panelerna och var lappen står ligger kvar i webbläsaren. */
$('strom').addEventListener('click', () => {
    if (!confirm(t('Stänga av och slå på? Allt i minnet, det som har skrivits på skivorna och bandet och det som har kommit ut försvinner, och alla val går tillbaka till början.',
                   'Switch off and on? Everything in memory, what has been written to the disks and the tape and what has come out is lost, and all choices go back to the start.')))
        return;
    location.replace(location.pathname);
});

for (const id of ['tecken', 'hz'])
    $(id).addEventListener('change', () => {
        starta();
        visa_lagen();
        meddela('Maskinen är startad om med den nya bygeln.', 'The machine has restarted with the new jumper.');
        skarm.focus();
    });

/* ---------------------------------------------------------------- */
/* Tangenterna                                                       */

function tangent_kod(tecken) {
    if (tecken in SVENSKA)
        return SVENSKA[tecken];
    const kod = tecken.charCodeAt(0);
    return kod < 0x80 ? kod : 0;
}

/* Text som tangenter; radslut blir RETURN med en paus efter. Med
 * flaggor gäller -k: \r, \n, \w ms, \xHH, \\. */
function skriv(text, flaggor = false) {
    for (let k = 0; k < text.length; k++) {
        const c = text[k];
        if (flaggor && c === '\\' && k + 1 < text.length) {
            const n = text[++k];
            if (n === 'r' || n === 'n')
                e.tangent(0x0D);
            else if (n === 'w') {
                const m = /^\d+/.exec(text.slice(k + 1));
                e.paus(m ? +m[0] : 0);
                k += m ? m[0].length : 0;
            } else if (n === 'x') {
                e.tangent(parseInt(text.substr(k + 1, 2), 16) & 0xFF);
                k += 2;
            } else
                e.tangent(tangent_kod(n));
            continue;
        }
        if (c === '\r')
            continue;
        if (c === '\n') {
            e.tangent(0x0D);
            e.paus(200);
            continue;
        }
        e.tangent(tangent_kod(c) || 0x3F);
    }
}

skarm.addEventListener('keydown', (h) => {
    if (h.metaKey)
        return;
    if (h.key === 'Escape' && $('hjalp').matches(':popover-open'))
        return;                              /* Esc stänger Kom igång */
    let kod = 0;
    if (h.ctrlKey && h.key.length === 1) {
        if (h.key === 'v' || h.key === 'V')
            return;                          /* klistra in */
        if (/^[a-z]$/i.test(h.key))
            kod = h.key.toUpperCase().charCodeAt(0) & 0x1F;
    } else if (h.key.length === 1)
        kod = tangent_kod(h.key);
    else
        kod = { Enter: 0x0D, Backspace: 0x08, ArrowLeft: 0x08,
                ArrowRight: 0x09, Escape: 0x1B, Tab: 0x09, Delete: 0x7F,
                /* PF1-PF8 som $C0-$C7 (options-PROM:ets manual s. 7, ?) */
                F1: 0xC0, F2: 0xC1, F3: 0xC2, F4: 0xC3,
                F5: 0xC4, F6: 0xC5, F7: 0xC6, F8: 0xC7 }[h.key] || 0;
    if (!kod)
        return;
    h.preventDefault();
    vack_ljudet();
    e.tangent(kod);
});

$('ram').addEventListener('mousedown', (h) => {
    if (h.target !== skarm) {
        h.preventDefault();
        skarm.focus();
    }
});

document.addEventListener('paste', (h) => {
    if (document.activeElement !== skarm)
        return;
    h.preventDefault();
    skriv(h.clipboardData.getData('text'));
});

/* ---------------------------------------------------------------- */
/* Program från en fil                                               */

async function skriv_in(fil) {
    const text = await fil.text();
    skriv(text);
    $('lage_program').textContent = fil.name;
    meddela(fil.name + ' skrivs in.', fil.name + ' is being typed.');
    skarm.focus();
}

$('skriv_fil').addEventListener('click', () => $('fil').click());
$('fil').addEventListener('change', () => {
    if ($('fil').files.length)
        skriv_in($('fil').files[0]);
    $('fil').value = '';
});

/* Släpp en fil på sidan: en skivavbild sätts i, annat skrivs in. */
document.addEventListener('dragover', (h) => {
    h.preventDefault();
    document.body.classList.add('slapp');
});
document.addEventListener('dragleave', (h) => {
    if (!h.relatedTarget)
        document.body.classList.remove('slapp');
});
document.addEventListener('drop', (h) => {
    h.preventDefault();
    document.body.classList.remove('slapp');
    const fil = h.dataTransfer.files[0];
    if (fil)
        ar_skiva(fil) ? skivfil(fil) : skriv_in(fil);
});

/* ---------------------------------------------------------------- */
/* Skivminnet                                                        */

let skivans_namn = '';
let katalogen = '';                          /* senast visad, som text */

async function packa_upp(text) {
    const strom = new Blob([base64(text)]).stream()
        .pipeThrough(new DecompressionStream('gzip'));
    return new Uint8Array(await new Response(strom).arrayBuffer());
}

/* En avbild i MF0; namnet visas i panelen och blir filnamnet vid Spara. */
function satt_i(data, namn) {
    if (!e.skiva(lagg_i_minnet(data), data.length)) {
        meddela(namn + ' är ingen skivavbild (hela sektorer om 256 bytes).',
                namn + ' is not a disk image (whole sectors of 256 bytes).');
        return false;
    }
    skivans_namn = namn;
    $('lage_skiva').textContent = namn;
    visa_katalogen();
    return true;
}

/* En CP/M-skiva (cpm/skiva.py): BIOS:ens hälsning i sektor 40-71. */
const CPM_HALSNING = 'CP/M 2.2 p';
function ar_cpm(d) {
    if (d.length < 80 * 256 + 128 * 32)
        return false;
    const system = String.fromCharCode(...d.subarray(40 * 256, 72 * 256));
    return system.includes(CPM_HALSNING);
}

/* CP/M:s katalog (spår 5, sektor 80): användare 0, första posten per fil. */
function cpm_filerna(d) {
    const namn = [];
    for (let p = 80 * 256; p < 80 * 256 + 128 * 32; p += 32) {
        if (d[p] !== 0 || d[p + 12] > 1)
            continue;
        const fil = String.fromCharCode(...d.subarray(p + 1, p + 9).map(b => b & 0x7F)).trimEnd();
        const typ = String.fromCharCode(...d.subarray(p + 9, p + 12).map(b => b & 0x7F)).trimEnd();
        if (!namn.some(([f, t]) => f === fil && t === typ))
            namn.push([fil, typ]);
    }
    return namn;
}

/* Filerna i katalogen, utan SYSDIR.SYS. */
function filerna() {
    const storlek = e.skivans_storlek();
    if (storlek < 24 * 256)
        return [];
    const hela = new Uint8Array(e.memory.buffer, e.skivans_data(), storlek);
    if (ar_cpm(hela))
        return cpm_filerna(hela);
    const d = hela.subarray(0, 24 * 256);
    const namn = [];
    for (let p = 16 * 256; p < 24 * 256; p += 16) {
        if (d[p] === 0xFF || d.subarray(p, p + 16).every(b => !b))
            continue;
        const fil = String.fromCharCode(...d.subarray(p + 4, p + 12)).trimEnd();
        const typ = String.fromCharCode(...d.subarray(p + 12, p + 15)).trimEnd();
        if (fil + '.' + typ !== 'SYSDIR.SYS')
            namn.push([fil, typ]);
    }
    return namn;
}

/* BAS och BAC går att klicka på: LOAD DR0:NAMN. */
function visa_katalogen() {
    const namn = filerna();
    const text = namn.map(n => n.join('.')).join(' ');
    if (text === katalogen)
        return;
    katalogen = text;
    const ruta = $('katalog');
    ruta.replaceChildren();
    for (const [fil, typ] of namn) {
        const program = typ === 'BAS' || typ === 'BAC';
        const el = document.createElement(program ? 'button' : 'span');
        el.textContent = typ === 'BAS' ? fil : fil + '.' + typ;
        if (program) {
            el.title = 'LOAD DR0:' + fil + '.' + typ;
            el.addEventListener('click', () => {
                skriv('LOAD DR0:' + fil + '.' + typ + '\n');
                meddela(fil + ' läses in; skriv RUN.', fil + ' is being loaded; type RUN.');
                skarm.focus();
            });
        }
        ruta.append(el);
    }
}

async function skivfil(fil) {
    const data = new Uint8Array(await fil.arrayBuffer());
    if (satt_i(data, fil.name))
        meddela(fil.name + ' sitter i DR0:.', fil.name + ' is in DR0:.');
    skarm.focus();
}

function ar_skiva(fil) {
    return /\.(img|dsk)$/i.test(fil.name);
}

/* Sidorna (bygg.sh, SIDA): abc802.html har tidsproven, abc802-forth.html
 * Forth och abc802-cpm.html CP/M. En skiva som sidan inte har finns på
 * en annan sida, och knappen går dit, med språket och 40 tecken. */
const SIDORNA = { grund: 'abc802.html', forth: 'abc802-forth.html', cpm: 'abc802-cpm.html' };

function till_sidan(sida, skiva) {
    const fragan = new URLSearchParams();
    if (skiva)
        fragan.set('skiva', skiva);
    fragan.set('sprak', sprak);
    if ($('tecken').value === '40')
        fragan.set('tecken', '40');
    if ($('natet').checked)
        fragan.set('nat', 'pa');
    location.href = SIDORNA[sida] + '?' + fragan;
}

$('skiva_prov').addEventListener('click', async () => {
    if (!FILER.tidsprov)
        return till_sidan('grund');
    if (satt_i(await packa_upp(FILER.tidsprov), 'tidsprov.img'))
        meddela('Tidsproven sitter i DR0:.', 'The benchmarks are in DR0:.');
    skarm.focus();
});

$('skiva_tom').addEventListener('click', async () => {
    if (satt_i(await packa_upp(FILER.tom), 'tom.img'))
        meddela('En tom skiva sitter i DR0:.', 'An empty disk is in DR0:.');
    skarm.focus();
});

/* Programskivan: BASICINI.SYS laddar Forth när maskinen startar om. */
$('skiva_program').addEventListener('click', async () => {
    if (!FILER.program)
        return till_sidan('grund', 'program');
    if (satt_i(await packa_upp(FILER.program), 'program.img')) {
        starta();
        meddela('Programskivan sitter i DR0:. RUN ORD är ordbehandlaren.',
                'The program disk is in DR0:. RUN ORD is the word processor.');
    }
    skarm.focus();
});

$('skiva_cpm').addEventListener('click', async () => {
    if (!FILER.cpm)
        return till_sidan('cpm');
    if (satt_i(await packa_upp(FILER.cpm), 'cpm.img')) {
        starta();
        meddela('CP/M 2.2 startar från DR0:.', 'CP/M 2.2 is starting from DR0:.');
    }
    skarm.focus();
});

/* Pascal-skivan (make pascal): CP/M med Pascal/MT+. Finns bara på
 * CP/M-sidan, och bara om den byggdes; annars syns knappen inte på
 * någon av sidorna (FILER.harpascal). */
$('skiva_pascal').addEventListener('click', async () => {
    if (FILER.sida !== 'cpm')
        return till_sidan('cpm', 'pascal');
    if (satt_i(await packa_upp(FILER.pascal), 'pascal.img')) {
        starta();
        if (FILER.mtplus)
            meddela('CP/M startar med Pascal/MT+ i DR0:. MTPLUS HEJ, LINKMT HEJ,FPREALS,PASLIB/S och HEJ.',
                    'CP/M is starting with Pascal/MT+ in DR0:. MTPLUS HEJ, LINKMT HEJ,FPREALS,PASLIB/S and HEJ.');
        else
            meddela('CP/M startar med Pascal i DR0:. PVM PC och HEJ.PAS gör HEJ.PK, och PVM HEJ kör den.',
                    'CP/M is starting with Pascal in DR0:. PVM PC and HEJ.PAS make HEJ.PK, and PVM HEJ runs it.');
    }
    skarm.focus();
});
if (!FILER.harpascal)
    $('skiva_pascal').hidden = $('hjalp_pascal').hidden = true;
/* Utan Pascal/MT+ (publikt/abc802) har skivan bara förrådets egen Pascal. */
for (const el of document.querySelectorAll(FILER.mtplus ? '.utan_mtplus' : '.mtplus'))
    el.hidden = true;
if (!FILER.mtplus) {
    const k = $('skiva_pascal');
    k.title = k.dataset.titleSv = 'Pascal, PVM med PC och PCS, under CP/M 2.2: skivan sätts i och maskinen startas om';
    k.dataset.titleEn = 'Pascal, PVM with PC and PCS, under CP/M 2.2: the disk is inserted and the machine restarted';
}

/* Forth (../forth/): skivan med FORTH.ABS sätts i och maskinen startas
 * om. BASICINI.SYS på skivan laddar Forth och startar det. */
$('skiva_forth').addEventListener('click', async () => {
    if (!FILER.forth)
        return till_sidan('forth');
    if (satt_i(await packa_upp(FILER.forth), 'forth.img')) {
        starta();
        meddela('Forth startar från DR0:. BYE går till BASIC.',
                'Forth is starting from DR0:. BYE returns to BASIC.');
    }
    skarm.focus();
});

$('skiva_valj').addEventListener('click', () => $('skivfil').click());
$('skivfil').addEventListener('change', () => {
    if ($('skivfil').files.length)
        skivfil($('skivfil').files[0]);
    $('skivfil').value = '';
});

$('skiva_spara').addEventListener('click', () => {
    const storlek = e.skivans_storlek();
    if (!storlek) {
        meddela('Det sitter ingen skiva i.', 'There is no disk in the drive.');
        return;
    }
    spara_fil(new Uint8Array(e.memory.buffer, e.skivans_data(), storlek).slice(),
              skivans_namn || 'skiva.img');
    skarm.focus();
});

/* ---------------------------------------------------------------- */
/* Nätet                                                             */

/* Nodkortet och centralen (natkort.c via webb.c). Med nodkortet i får
 * maskinen nät-ROM:en; filerna i UFD: ligger i webb.c och sitter kvar
 * när maskinen startas om. En text blir poster om 253 bytes med CR
 * efter raderna, $03 efter det sista i en full post och nollor efter
 * den sista posten (som SAVE gör); å ä ö é ü Å Ä Ö É Ü blir ABC 802:s
 * koder och tillbaka när filen hämtas, ¤ blir $24 och $24 blir $. Annat läggs in och hämtas som det är. */
const POST = 253;
/* Tillbaka blir $24 $ och inte ¤ (A$), som för kontakterna. */
const ABC_SVENSKA = Object.fromEntries(
    Object.entries(SVENSKA).filter(([c]) => c !== '¤').map(([c, k]) => [k, c]));
let nat_filerna = '';                         /* senast visad, som text */
let nat_sett = -1;                            /* nat_anrop() då */
const TRAFIK_RADER = 300;
let trafiken = [];

function ar_binar(namn) {
    return /\.(bac|abs|bin|img|dsk|sys|cmd)$/i.test(namn);
}

/* Text till poster. */
function till_poster(text) {
    const rader = text.replace(/\r\n?/g, '\n').split('\n');
    if (rader.length && rader[rader.length - 1] === '')
        rader.pop();
    const ut = [];
    let post = [];
    const full = () => {
        ut.push(...post, 0x03, ...new Array(POST - post.length - 1).fill(0));
        post = [];
    };
    for (const rad of rader) {
        const koder = [...rad].map(c => SVENSKA[c] ?? (c.charCodeAt(0) < 128 ? c.charCodeAt(0) : 0x3F));
        koder.push(0x0D);
        for (let i = 0; i < koder.length; i += POST - 1) {
            const bit = koder.slice(i, i + POST - 1);
            if (post.length + bit.length > POST - 1)
                full();
            post.push(...bit);
        }
    }
    ut.push(...post, ...new Array(POST - post.length).fill(0));
    return new Uint8Array(ut);
}

/* Poster som är text tillbaka till text; null om de inte är det. */
function fran_poster(d) {
    if (!d.length || d.length % POST)
        return null;
    let text = '';
    for (let p = 0; p < d.length; p += POST) {
        let i = p;
        for (; i < p + POST && d[i] !== 0x03 && d[i] !== 0; i++) {
            const b = d[i];
            if (b === 0x0D)
                text += '\n';
            else if (b === 0x0A)
                continue;
            else if (b < 0x20 && b !== 0x09 || b > 0x7E)
                return null;
            else
                text += ABC_SVENSKA[b] ?? String.fromCharCode(b);
        }
        if (i === p + POST)
            return null;                     /* posten tog inte slut */
    }
    return text;
}

/* Centralens klocka: d, eller nu. */
function nat_klockan(d = new Date()) {
    e.nat_klocka(d.getFullYear() - 1900, d.getMonth() + 1, d.getDate(),
                 d.getHours(), d.getMinutes(), d.getSeconds());
}

/* En fil från datorn till UFD:. */
async function nat_lagg_in(fil) {
    const namn = fil.name.toUpperCase();
    let data = new Uint8Array(await fil.arrayBuffer());
    if (!ar_binar(namn) && fran_poster(data) === null) {
        let text = null;
        try { text = new TextDecoder('utf-8', { fatal: true }).decode(data); }
        catch (fel) { text = new TextDecoder('latin1').decode(data); }
        if (!/[\x00-\x08\x0B\x0C\x0E-\x1F]/.test(text))
            data = till_poster(text);
    }
    const pnamn = lagg_i_minnet(new TextEncoder().encode(namn + '\0'));
    const pdata = lagg_i_minnet(data);
    nat_klockan(new Date(fil.lastModified));
    const ok = e.nat_fil(pnamn, pdata, data.length);
    nat_klockan();
    e.slapp(pnamn);
    e.slapp(pdata);
    if (!ok)
        meddela(fil.name + ' går inte in i UFD: (namnet är högst åtta tecken, en punkt och tre).',
                fil.name + ' does not go into UFD: (the name is at most eight characters, a dot and three).');
    return ok;
}

async function nat_lagg_in_alla(filer) {
    let antal = 0;
    for (const fil of filer)
        if (await nat_lagg_in(fil))
            antal++;
    if (antal)
        meddela(antal === 1 ? filer[0].name.toUpperCase() + ' ligger i UFD:.' : antal + ' filer ligger i UFD:.',
                antal === 1 ? filer[0].name.toUpperCase() + ' is in UFD:.' : antal + ' files are in UFD:.');
    visa_natet(true);
}

function nat_namn(k) {
    const p = e.nat_namn(k);
    if (!p)
        return null;
    const m = new Uint8Array(e.memory.buffer, p, 80);
    return new TextDecoder().decode(m.subarray(0, m.indexOf(0)));
}

function nat_lista() {
    const lista = [];
    for (let k = 0; k < e.nat_antal(); k++) {
        const namn = nat_namn(k);
        if (namn !== null)
            lista.push([k, namn, e.nat_langd(k)]);
    }
    return lista.sort((a, b) => a[1] < b[1] ? -1 : 1);
}

function nat_hamta(k, namn) {
    const d = new Uint8Array(e.memory.buffer, e.nat_data(k), e.nat_langd(k)).slice();
    const text = ar_binar(namn) ? null : fran_poster(d);
    const fil = namn.replace(/.*\//, '');
    if (text === null)
        spara_fil(d, fil);
    else
        spara_fil(new TextEncoder().encode(text), fil);
}

/* Listan och läget; listan byggs bara om när den har ändrats. */
function visa_natet(genast = false) {
    const pa = $('natet').checked;
    const anrop = e.nat_anrop();
    if (!genast && anrop === nat_sett)
        return;
    nat_sett = anrop;
    const lista = nat_lista();
    $('lage_natet').textContent = (pa ? '' : t('ur · ', 'out · '))
        + lista.length + t(lista.length === 1 ? ' fil' : ' filer', lista.length === 1 ? ' file' : ' files')
        + (anrop ? ' · ' + anrop + t(' anrop', ' calls') : '');
    const text = lista.map(([k, n, l]) => k + n + l).join(' ');
    if (text === nat_filerna)
        return;
    nat_filerna = text;
    const ruta = $('natfiler');
    ruta.replaceChildren();
    for (const [k, namn, langd] of lista) {
        const program = /\.BA[SC]$/.test(namn);
        const el = document.createElement(program ? 'button' : 'span');
        el.textContent = namn;
        if (program) {
            el.title = 'LOAD UFD:' + namn;
            el.addEventListener('click', () => {
                if (!$('natet').checked) {
                    meddela('Sätt i nodkortet först.', 'Put the node card in first.');
                    return;
                }
                skriv('LOAD UFD:' + namn + '\n');
                meddela(namn + ' läses in över nätet; skriv RUN.', namn + ' is being loaded over the network; type RUN.');
                skarm.focus();
            });
        }
        const storlek = document.createElement('span');
        storlek.className = 'storlek';
        storlek.textContent = Math.ceil(langd / POST);
        storlek.title = t(langd + ' bytes, poster om 253', langd + ' bytes, records of 253');
        const hamta = document.createElement('button');
        hamta.textContent = '↓';
        hamta.title = t('Hämta ', 'Fetch ') + namn;
        hamta.addEventListener('click', () => nat_hamta(k, namn));
        const bort = document.createElement('button');
        bort.textContent = '✕';
        bort.title = t('Ta bort ', 'Remove ') + namn;
        bort.addEventListener('click', () => {
            e.nat_ta_bort(k);
            meddela(namn + ' är borttagen ur UFD:.', namn + ' has been removed from UFD:.');
            visa_natet(true);
        });
        ruta.append(el, storlek, hamta, bort);
    }
}

/* Trafiken efter varje varv. */
function hamta_trafiken() {
    const antal = e.nat_logg_antal();
    if (!antal)
        return;
    const text = new TextDecoder().decode(new Uint8Array(e.memory.buffer, e.nat_logg(), antal));
    e.nat_logg_tom();
    trafiken.push(...text.split('\n').filter(r => r));
    if (trafiken.length > TRAFIK_RADER)
        trafiken = trafiken.slice(-TRAFIK_RADER);
    const ruta = $('trafik');
    if (!ruta.hidden) {
        ruta.textContent = trafiken.join('\n');
        ruta.scrollTop = ruta.scrollHeight;
    }
}

$('natet').addEventListener('change', () => {
    starta();
    meddela(...($('natet').checked
            ? ['Nodkortet sitter i och maskinen har startat med nätets DOS-PROM; prova LOAD UFD:NAT och RUN, eller RUN UFD:L som listar UFD:.',
               'The node card is in and the machine has started with the network DOS PROM; try LOAD UFD:NAT and RUN, or RUN UFD:L, which lists UFD:.']
            : ['Nodkortet är urtaget och maskinen har startat med DOS v.20.',
               'The node card is out and the machine has started with DOS v.20.']));
    visa_natet(true);
    skarm.focus();
});

$('nat_lagg').addEventListener('click', () => $('natfil').click());
$('natfil').addEventListener('change', async () => {
    const filer = [...$('natfil').files];
    $('natfil').value = '';
    await nat_lagg_in_alla(filer);
    skarm.focus();
});

$('nat_trafik_visa').addEventListener('click', () => {
    const ruta = $('trafik');
    ruta.hidden = !ruta.hidden;
    $('nat_trafik_visa').classList.toggle('pa', !ruta.hidden);
    if (!ruta.hidden) {
        ruta.textContent = trafiken.length ? trafiken.join('\n')
            : t('(inga anrop ännu)', '(no calls yet)');
        ruta.scrollTop = ruta.scrollHeight;
    }
});

/* Filer som släpps på panelen går till UFD:. */
$('panel_natet').addEventListener('dragover', (h) => { h.preventDefault(); h.stopPropagation(); });
$('panel_natet').addEventListener('drop', async (h) => {
    h.preventDefault();
    h.stopPropagation();
    document.body.classList.remove('slapp');
    await nat_lagg_in_alla([...h.dataTransfer.files]);
});

/* Ett litet program i UFD: från början. */
const NAT_PROV = [
    '10 REM Skriver en fil i UFD: och läser den',
    '20 PREPARE "UFD:HALSNING.TXT" AS FILE 1',
    '30 PRINT #1, "Hej från centralen!"',
    '40 CLOSE 1',
    '50 OPEN "UFD:HALSNING.TXT" AS FILE 1',
    '60 INPUT #1, A$',
    '70 CLOSE 1',
    '80 PRINT A$',
].join('\n') + '\n';

function nat_prov() {
    const data = till_poster(NAT_PROV);
    const pnamn = lagg_i_minnet(new TextEncoder().encode('NAT.BAS\0'));
    const pdata = lagg_i_minnet(data);
    e.nat_fil(pnamn, pdata, data.length);
    e.slapp(pnamn);
    e.slapp(pdata);
    /* Dataindustriers L (L.BAC, byggd av ABC-klubbens L.BAS), om sidan har den */
    if (FILER.lbac) {
        const lbac = base64(FILER.lbac);
        const lnamn = lagg_i_minnet(new TextEncoder().encode('L.BAC\0'));
        const ldata = lagg_i_minnet(lbac);
        e.nat_fil(lnamn, ldata, lbac.length);
        e.slapp(lnamn);
        e.slapp(ldata);
    }
}

/* ---------------------------------------------------------------- */
/* Kassetten                                                         */

let bandets_namn = '';

function visa_kassetten() {
    const langd = e.bandets_langd(), lage = e.bandets_lage();
    let text = '';
    if (!e.bandets_data())
        text = t('inget band', 'no tape');
    else if (e.spelar_inne())
        text = t('spelar in', 'recording') + ' · ' + langd + t(' tecken', ' characters');
    else
        text = (bandets_namn ? bandets_namn + ' · ' : '') + Math.min(lage, langd) + ' / ' + langd;
    if (e.motor())
        text = '▶ ' + text;
    $('lage_kassett').textContent = text;
}

$('band_tomt').addEventListener('click', () => {
    if (e.tomt_band()) {
        bandets_namn = '';
        meddela('Ett tomt band, inspelning: SAVE CAS:NAMN.', 'A blank tape, recording: SAVE CAS:NAME.');
    } else
        meddela('Minnet räcker inte till ett band.', 'There is not enough memory for a tape.');
    skarm.focus();
});

$('band_spola').addEventListener('click', () => {
    if (!e.bandets_data()) {
        meddela('Det finns inget band i kassetten.', 'There is no tape in the cassette.');
    } else {
        const inspelat = e.spelar_inne();
        e.spola();
        meddela(inspelat ? 'Bandet är tillbakaspolat och spelas upp: LOAD CAS:NAMN.'
                         : 'Bandet är tillbakaspolat.',
                inspelat ? 'The tape is rewound and plays back: LOAD CAS:NAME.'
                         : 'The tape is rewound.');
    }
    skarm.focus();
});

$('band_valj').addEventListener('click', () => $('bandfil').click());
$('bandfil').addEventListener('change', async () => {
    const fil = $('bandfil').files[0];
    $('bandfil').value = '';
    if (!fil)
        return;
    const data = new Uint8Array(await fil.arrayBuffer());
    e.band(lagg_i_minnet(data), data.length);
    bandets_namn = fil.name;
    meddela(fil.name + ' ligger i kassetten.', fil.name + ' is in the cassette.');
    skarm.focus();
});

$('band_spara').addEventListener('click', () => {
    const langd = e.bandets_langd();
    if (!e.bandets_data() || !langd) {
        meddela('Bandet är tomt.', 'The tape is empty.');
        return;
    }
    spara_fil(new Uint8Array(e.memory.buffer, e.bandets_data(), langd).slice(),
              bandets_namn || 'band.kas');
    if (e.bandet_fullt())
        meddela('Bandet tog slut under inspelningen.', 'The tape ran out while recording.');
    skarm.focus();
});

function spara_fil(data, namn) {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([data], { type: 'application/octet-stream' }));
    a.download = namn;
    a.click();
    setTimeout(() => URL.revokeObjectURL(a.href), 1000);
    meddela(namn + ' är sparad.', namn + ' has been saved.');
}

/* ---------------------------------------------------------------- */
/* CH A och CH B                                                     */

const UT_MAX = 1 << 20;                      /* bytes som sparas per kontakt */
const SKRIVAREN = 2;                         /* kontakt 2: skrivarkortet */
const utdata = [[], [], []];                 /* bitar (Uint8Array) per kontakt */
const utlangd = [0, 0, 0];
let ut_totalt = 0, in_totalt = 0;            /* bytes ut och in, för lampan */
let ut_annan = -Infinity;                    /* senast det kom på en kontakt som inte visas */
const in_ko = [[], [], []];                  /* bytes som väntar på maskinen */
const SKARM_TILL = Object.fromEntries(
    Object.entries(SVENSKA).filter(([c]) => c !== '¤').map(([c, k]) => [k, c]));

function som_text(bytes) {
    let text = '';
    for (const b of bytes) {
        const c = b & 0x7F;
        if (c === 0x0A)
            text += '\n';
        else if (c === 0x0C)
            text += '\f';
        else if (c === 0x09)
            text += '\t';
        else if (c >= 0x20 && c < 0x7F)
            text += SKARM_TILL[c] || String.fromCharCode(c);
    }
    return text;
}

function kontakten() {
    return +$('kontakt').value;
}

/* Utskriften: en lapp ovanpå sidan (popover manual där webbläsaren har
 * det, annars klassen oppen), som i ABC80-emulatorn. En ny utskrift på
 * CH A eller skrivarkortet tar fram den; på CH B är skärmen terminalen,
 * så där lyser bara knappen. Lappen flyttas genom att man drar i listen,
 * och var den står sparas i webbläsaren. */
const ut_lapp = $('ut_lapp');
const med_popover = typeof ut_lapp.showPopover === 'function';
const UT_PAUS = 2000;                        /* ms tyst innan en ny utskrift */
const ut_senast = [0, 0, 0];                 /* när det senast kom något */

function ut_oppen() {
    return med_popover ? ut_lapp.matches(':popover-open') : ut_lapp.classList.contains('oppen');
}

function ut_visa(visa) {
    if (visa === ut_oppen())
        return;
    if (med_popover)
        visa ? ut_lapp.showPopover() : ut_lapp.hidePopover();
    else
        ut_lapp.classList.toggle('oppen', visa);
}

function ut_rubrik() {
    $('ut_rubrik').textContent = t('Utskriften, ', 'Output, ')
        + ['CH A (PR:)', 'CH B (V24:)', t('skrivarkortet', 'the printer card')][kontakten()];
}

function ut_flytta(x, y) {
    const r = ut_lapp.getBoundingClientRect();
    x = Math.max(0, Math.min(x, innerWidth - r.width));
    y = Math.max(0, Math.min(y, innerHeight - 40));
    ut_lapp.style.left = x + 'px';
    ut_lapp.style.top = y + 'px';
    ut_lapp.style.right = 'auto';
}

const UT_NYCKEL = 'abc802_utskriften';
{
    const listen = ut_lapp.querySelector('header');
    let drag = null;                         /* {dx, dy} medan den dras */
    listen.addEventListener('pointerdown', h => {
        if (h.target.closest('button') || h.button !== 0)
            return;
        const r = ut_lapp.getBoundingClientRect();
        drag = { dx: h.clientX - r.left, dy: h.clientY - r.top };
        listen.setPointerCapture(h.pointerId);
        h.preventDefault();
    });
    listen.addEventListener('pointermove', h => {
        if (drag)
            ut_flytta(h.clientX - drag.dx, h.clientY - drag.dy);
    });
    const slapp = () => {
        if (!drag)
            return;
        drag = null;
        const r = ut_lapp.getBoundingClientRect();
        try {
            localStorage.setItem(UT_NYCKEL, JSON.stringify({ x: r.left, y: r.top }));
        } catch (fel) { /* sparas inte */ }
    };
    listen.addEventListener('pointerup', slapp);
    listen.addEventListener('pointercancel', slapp);
    addEventListener('resize', () => {
        if (ut_lapp.style.left)
            ut_flytta(parseFloat(ut_lapp.style.left), parseFloat(ut_lapp.style.top));
    });
}
/* Storleken: lappen dras större i hörnet (resize i CSS), och då sätter
 * webbläsaren width och height på den; de sparas som läget. */
const UT_STORLEK_NYCKEL = 'abc802_utskriften_storlek';
new ResizeObserver(() => {
    if (!ut_oppen() || !ut_lapp.style.width)
        return;
    try {
        localStorage.setItem(UT_STORLEK_NYCKEL,
                             JSON.stringify({ w: ut_lapp.offsetWidth, h: ut_lapp.offsetHeight }));
    } catch (fel) { /* sparas inte */ }
}).observe(ut_lapp);

ut_lapp.addEventListener('toggle', () => {
    if (!ut_oppen())
        return;
    try {
        const m = JSON.parse(localStorage.getItem(UT_STORLEK_NYCKEL));
        if (m && m.w > 0 && m.h > 0) {
            ut_lapp.style.width = Math.min(m.w, innerWidth - 16) + 'px';
            ut_lapp.style.height = Math.min(m.h, innerHeight - 16) + 'px';
        }
        const l = JSON.parse(localStorage.getItem(UT_NYCKEL));
        if (l && isFinite(l.x) && isFinite(l.y))
            ut_flytta(l.x, l.y);
    } catch (fel) { /* uppe till höger */ }
    $('ut_knapp').classList.remove('nytt');
    $('utskrift').scrollTop = $('utskrift').scrollHeight;
});
$('ut_stang').addEventListener('click', () => ut_visa(false));
if (!med_popover)
    $('ut_knapp').addEventListener('click', () => ut_visa(!ut_oppen()));

/* Det har kommit något på kontakten som visas. */
function ut_nytt(k) {
    const ny = performance.now() - ut_senast[k] > UT_PAUS;
    ut_senast[k] = performance.now();
    if (ny && k !== 1)
        ut_visa(true);
    if (!ut_oppen())
        $('ut_knapp').classList.add('nytt');
}

/* En sidmatning blir ett streck: FF, eller UT_TOMMA tomma rader eller
 * fler (PR: matar fram sidan med radmatningar), så att det som skrevs
 * står kvar synligt ovanför i stället för att rulla bort. */
const UT_TOMMA = 4;
const SIDBYTE = new RegExp('\\n*\\f\\n*|\\n{' + (UT_TOMMA + 1) + ',}', 'g');

function som_html(text) {
    const html = text.replace(/[&<>]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' })[c]);
    return html.replace(SIDBYTE, '<hr title="' + t('Ny sida', 'New page') + '">');
}

function visa_utskriften() {
    const k = kontakten();
    const ruta = $('utskrift');
    const nere = ruta.scrollTop + ruta.clientHeight >= ruta.scrollHeight - 4;
    ruta.innerHTML = som_html(utdata[k].map(som_text).join(''));
    if (nere)
        ruta.scrollTop = ruta.scrollHeight;
}

/* Det som maskinen har sänt sedan förra varvet. */
function hamta_ut() {
    let nytt = false;
    for (let k = 0; k < 3; k++) {
        const n = e.ut_antal(k);
        if (!n)
            continue;
        const bit = new Uint8Array(e.memory.buffer, e.ut_data(k), n).slice();
        e.ut_tomt(k);
        if (k === 1 && motpart && motpart.uppkopplad())
            for (const b of bit)
                motpart.tecken(b);
        utdata[k].push(bit);
        utlangd[k] += n;
        ut_totalt += n;
        if (k !== kontakten() && !(k === 1 && motpart && motpart.uppkopplad()))
            ut_annan = performance.now();
        while (utlangd[k] > UT_MAX && utdata[k].length > 1)
            utlangd[k] -= utdata[k].shift().length;
        if (k === kontakten()) {
            nytt = true;
            ut_nytt(k);
        }
    }
    if (nytt)
        visa_utskriften();
    kolla_linjen();
}

/* Det som väntar, så långt maskinens kö räcker. */
function fyll_in() {
    for (let k = 0; k < 2; k++) {
        let i = 0;
        while (i < in_ko[k].length && e.seriellt_in(k, in_ko[k][i]))
            i++;
        in_totalt += i;
        if (i)
            in_ko[k] = in_ko[k].slice(i);
    }
}

function visa_kontakterna() {
    ut_rubrik();
    const v = in_ko[kontakten()].length;
    $('lage_kontakter').textContent = ['PR:', 'V24:', t('kort 60', 'card 60')][kontakten()]
        + ' · ' + utlangd[kontakten()] + t(' ut', ' out')
        + (v ? ' · ' + v + t(' väntar', ' waiting') : '');
}

function skicka_bytes(bytes) {
    const k = kontakten();
    if (k === SKRIVAREN)                     /* skrivaren tar inte emot */
        return;
    for (const b of bytes)
        in_ko[k].push(b);
    fyll_in();
}

function skicka_text(text) {
    const bytes = [];
    for (const c of text.replace(/\r\n?/g, '\n'))
        bytes.push(c === '\n' ? 0x0D : (tangent_kod(c) || 0x3F));
    skicka_bytes(bytes);
}

/* Motparterna på CH B (datavision.js, bbs.js, stordator.js): det de svarar väntar i
 * in_ko[1], och det maskinen sänder får den som är vald. */
function till_ch_b(bytes) {
    for (const b of bytes)
        in_ko[1].push(b);
    if (e)
        fyll_in();
}
/* Det som ännu inte har gått in på CH B, här och i kärnans kö. */
function ch_b_vantar() {
    return in_ko[1].length + (e ? e.seriellt_vantar(1) : 0);
}
const motparter = {
    datavision: skapa_datavision(till_ch_b),
    bbs: skapa_bbs(till_ch_b, ch_b_vantar),
    stordator: skapa_stordator(till_ch_b, ch_b_vantar),
};
let motpart = null;

const RING_TEXT = {
    datavision: ['Datavision svarar på CH B. 40 tecken och LOAD V24:WEA10K24.40A.',
                 'Datavision answers on CH B. 40 columns and LOAD V24:WEA10K24.40A.'],
    bbs: ['Bytesboden svarar på CH B. 80 tecken och LOAD V24:TSA30B24.22A.',
          'Bytesboden answers on CH B. 80 columns and LOAD V24:TSA30B24.22A.'],
    stordator: ['Datacentralen svarar på CH B. 80 tecken och LOAD V24:TSA30B24.22A.',
                'Datacentralen answers on CH B. 80 columns and LOAD V24:TSA30B24.22A.'],
};

let uppringning = null;                     /* setTimeout medan det ringer */

function ring_upp() {
    const namn = $('motpart').value;
    if (motpart)
        motpart.lagg_pa();
    kolla_linjen();
    clearTimeout(uppringning);
    uppringning = null;
    motpart = motparter[namn] || null;
    in_ko[1] = [];
    if (!motpart)
        return;
    $('kontakt').value = '1';
    visa_skicka();
    visa_utskriften();
    visa_kontakterna();
    meddela('Ringer upp ...', 'Dialling ...');
    const vald = motpart;
    uppringning = setTimeout(() => {
        uppringning = null;
        if (motpart !== vald)
            return;
        vald.ring();
        kolla_linjen();
        meddela(...RING_TEXT[namn]);
    }, modemljud(namn) * 1000);
}

$('motpart').addEventListener('change', () => {
    const vald = $('motpart').value in motparter;
    $('ring').disabled = !vald;
    if (vald)
        ring_upp();
    else {
        clearTimeout(uppringning);
        uppringning = null;
        if (motpart)
            motpart.lagg_pa();
        kolla_linjen();
        motpart = null;
        in_ko[1] = [];
        visa_kontakterna();
    }
});
$('ring').addEventListener('click', ring_upp);

function visa_skicka() {
    const nej = kontakten() === SKRIVAREN;
    for (const id of ['rad_in', 'skicka', 'skicka_fil'])
        $(id).disabled = nej;
}

$('skrivarkort').addEventListener('change', () => {
    e.skrivarkortet($('skrivarkort').checked ? 1 : 0);
    if ($('skrivarkort').checked) {
        $('kontakt').value = String(SKRIVAREN);
        visa_utskriften();
        visa_skicka();
        visa_kontakterna();
    }
});

$('skrivarfart').addEventListener('change', () => {
    e.skrivarens_fart(+$('skrivarfart').value);
    skarm.focus();
});

$('kontakt').addEventListener('change', () => {
    visa_skicka();
    visa_utskriften();
    visa_kontakterna();
});

$('ut_tom').addEventListener('click', () => {
    utdata[kontakten()] = [];
    utlangd[kontakten()] = 0;
    visa_utskriften();
    visa_kontakterna();
});

$('ut_spara').addEventListener('click', () => {
    const k = kontakten();
    if (!utlangd[k]) {
        meddela('Inget har kommit ut än.', 'Nothing has come out yet.');
        return;
    }
    const data = new Uint8Array(utlangd[k]);
    let p = 0;
    for (const bit of utdata[k]) {
        data.set(bit, p);
        p += bit.length;
    }
    spara_fil(data, ['pr.txt', 'v24.txt', 'skrivaren.txt'][k]);
});

function skicka_raden() {
    skicka_text($('rad_in').value + '\n');
    $('rad_in').value = '';
}
$('skicka').addEventListener('click', skicka_raden);
$('rad_in').addEventListener('keydown', (h) => {
    if (h.key === 'Enter') {
        h.preventDefault();
        skicka_raden();
    }
});

$('skicka_fil').addEventListener('click', () => $('kontaktfil').click());
$('kontaktfil').addEventListener('change', async () => {
    const fil = $('kontaktfil').files[0];
    $('kontaktfil').value = '';
    if (!fil)
        return;
    skicka_bytes(new Uint8Array(await fil.arrayBuffer()));
    meddela(fil.name + ' skickas till ' + (kontakten() ? 'CH B.' : 'CH A.'),
            fil.name + ' is being sent to ' + (kontakten() ? 'CH B.' : 'CH A.'));
});

/* ---------------------------------------------------------------- */
/* Pipet                                                             */

/* Webbläsaren släpper inte fram ljudet förrän man har klickat eller
 * skrivit på sidan; då väcks det. Ett pip i taget, och inget som har
 * hopats medan maskinen gick fort. */
let ljud = null;
let ljud_valt = true;
let pipen = 0;                               /* pip() senast */
let pip_slut = 0;                            /* när det som låter tystnar (ljudets tid) */

try { ljud = new AudioContext(); } catch (fel) { ljud = null; }
if (ljud)
    ljud.addEventListener('statechange', () => visa_ljudet());

function vack_ljudet() {
    if (ljud_valt && ljud && ljud.state !== 'running')
        ljud.resume().catch(() => {});
}

function visa_ljudet() {
    const knapp = $('ljud');
    let text = t('Ljud: av', 'Sound: off');
    if (!ljud)
        text = t('Inget ljud', 'No sound');
    else if (ljud_valt)
        text = ljud.state === 'running' ? t('Ljud: på', 'Sound: on')
            : t('Ljud: klicka på skärmen', 'Sound: click the screen');
    knapp.textContent = text;
    knapp.title = t('Tangentbordets pip (kod 7) och summern (IN 5)', 'The keyboard beep (code 7) and the buzzer (IN 5)');
    knapp.classList.toggle('pa', ljud_valt && !!ljud);
}

for (const namn of ['pointerdown', 'keydown'])
    document.addEventListener(namn, vack_ljudet, true);

$('ljud').addEventListener('click', () => {
    ljud_valt = !ljud_valt;
    if (ljud)
        ljud_valt ? vack_ljudet() : ljud.suspend();
    visa_ljudet();
    skarm.focus();
});

function pip() {
    if (!ljud_valt || !ljud || ljud.state !== 'running' || ljud.currentTime < pip_slut)
        return;
    const ton = ljud.createOscillator();
    const styrka = ljud.createGain();
    ton.type = 'square';
    ton.frequency.value = PIP_HZ;
    styrka.gain.value = 0.15;
    ton.connect(styrka).connect(ljud.destination);
    const nu = ljud.currentTime;
    pip_slut = nu + PIP_MS / 1000;
    ton.start(nu);
    ton.stop(pip_slut);
}

/* Summern (IN 5): det som har låtit sedan förra bilden, i följd efter
 * det förra, lite fördröjt så att bitarna hänger ihop, och en bild
 * senare: ROM:ens BEEP (CHR$(7)) slår om summern 200 gånger och skickar
 * sedan kod 7 till tangentbordet. Kommer ett pip kastas summern från den
 * och förra bilden, så att BEEP bara blir pipet. */
let summer_slut = 0;
let summer_vantar = null;                    /* proven från förra bilden */
function summern(pipade) {
    const n = e.summer();
    const nya = n ? new Float32Array(e.memory.buffer, e.summern_prov(), n).slice() : null;
    const spela = pipade ? null : summer_vantar;
    summer_vantar = pipade ? null : nya;
    if (!spela || !ljud_valt || !ljud || ljud.state !== 'running')
        return;
    const nu = ljud.currentTime;
    if (summer_slut < nu + 0.05 || summer_slut > nu + 0.4)
        summer_slut = nu + 0.05;
    const buf = ljud.createBuffer(1, spela.length, 48000);
    const kanal = buf.getChannelData(0);
    for (let k = 0; k < spela.length; k++)
        kanal[k] = spela[k] * 0.15;
    const kalla = ljud.createBufferSource();
    kalla.buffer = buf;
    kalla.connect(ljud.destination);
    kalla.start(summer_slut);
    summer_slut += spela.length / 48000;
}

/* Modemet när man ringer upp, lågt: kopplingstonen (425 Hz), numret med
 * fingerskivan (en siffra d ger d+1 klick, 0 ett, 10 i sekunden; det
 * svenska sättet, ?), en signal, svarstonen (2 100 Hz) och bärvågen, och
 * sedan tyst, som när modemet slog av sin högtalare. Bärvågen är V.21
 * (980 och 1 650 Hz, 300 bit/s) eller för Datavision V.23 (1 300 Hz in
 * och 390 Hz tillbaka). Ett klick när linjen bryts. modemljud() ger hur
 * många sekunder uppringningen tar, med eller utan ljud. */
const MODEM_STYRKA = 0.04;
const MODEM_NUMMER = { datavision: '91', bbs: '472', stordator: '13' };

function modem_lat() {
    return ljud_valt && ljud && ljud.state === 'running';
}

function modem_ton(hz, fran, langd, styrka = MODEM_STYRKA) {
    const ton = ljud.createOscillator();
    const g = ljud.createGain();
    ton.frequency.value = hz;
    g.gain.setValueAtTime(0, fran);
    g.gain.linearRampToValueAtTime(styrka, fran + 0.01);
    g.gain.setValueAtTime(styrka, fran + langd - 0.01);
    g.gain.linearRampToValueAtTime(0, fran + langd);
    ton.connect(g).connect(ljud.destination);
    ton.start(fran);
    ton.stop(fran + langd);
}

function modem_klick(nar, styrka = 0.12) {
    const n = Math.floor(ljud.sampleRate * 0.012);
    const b = ljud.createBuffer(1, n, ljud.sampleRate);
    const d = b.getChannelData(0);
    for (let i = 0; i < n; i++)
        d[i] = (Math.random() * 2 - 1) * Math.exp(-i / (n / 5));
    const k = ljud.createBufferSource();
    const g = ljud.createGain();
    k.buffer = b;
    g.gain.value = styrka;
    k.connect(g).connect(ljud.destination);
    k.start(nar);
}

function modemljud(namn) {
    const lat = modem_lat();
    const t0 = lat ? ljud.currentTime + 0.05 : 0;
    let t = t0;
    if (lat)
        modem_klick(t);                      /* luren lyfts */
    t += 0.2;
    if (lat)
        modem_ton(425, t, 0.9);              /* kopplingstonen */
    t += 1.0;
    for (const siffra of MODEM_NUMMER[namn] || '1') {
        const klick = +siffra + 1;
        for (let i = 0; i < klick; i++)
            if (lat)
                modem_klick(t + i * 0.1, 0.08);
        t += klick * 0.1 + 0.5;
    }
    t += 0.4;
    if (lat)
        modem_ton(425, t, 1.0);              /* en signal */
    t += 1.6;
    if (lat)
        modem_ton(2100, t, 1.5);             /* svarstonen */
    t += 1.6;
    if (lat) {
        const [a, b] = namn === 'datavision' ? [1300, 390] : [1650, 980];
        modem_ton(a, t, 1.1, MODEM_STYRKA * 0.8);
        modem_ton(b, t + 0.3, 0.8, MODEM_STYRKA * 0.6);
    }
    t += 1.2;
    return t - t0;
}

/* Klicket när förbindelsen bryts, av motparten eller härifrån. */
let linjen_uppe = false;
function kolla_linjen() {
    const uppe = !!(motpart && motpart.uppkopplad());
    if (linjen_uppe && !uppe && modem_lat())
        modem_klick(ljud.currentTime + 0.02);
    linjen_uppe = uppe;
}

/* ---------------------------------------------------------------- */
/* Skärmen                                                           */

/* Punkterna i 480 x 250 (bärnsten eller genomskinligt) på en egen
 * canvas, som ritas förstorad på skärmen. */
const punktbild = document.createElement('canvas');
punktbild.width = BREDD;
punktbild.height = HOJD;
const punktritning = punktbild.getContext('2d');
const punktdata = punktritning.createImageData(BREDD, HOJD);
let ritad = 0;                               /* performance.now() när bilden senast ritades */

function rita() {
    const punkter = new Uint8Array(e.memory.buffer, e.punkter(), BREDD * HOJD);
    const d = punktdata.data;
    if ($('farg').checked)
        for (let k = 0, j = 0; k < punkter.length; k++, j += 4) {
            const f = FARGER[punkter[k] & 7];
            d[j] = f[0];
            d[j + 1] = f[1];
            d[j + 2] = f[2];
            d[j + 3] = punkter[k] ? 255 : 0;
        }
    else
        for (let k = 0, j = 0; k < punkter.length; k++, j += 4) {
            d[j] = FARG[0];
            d[j + 1] = FARG[1];
            d[j + 2] = FARG[2];
            d[j + 3] = punkter[k] ? 255 : 0;
        }
    punktritning.putImageData(punktdata, 0, 0);

    const nu = performance.now();
    const dt = ritad ? (nu - ritad) / 1000 : 1;
    ritad = nu;
    const skarp = $('skarp').checked;
    ritning.globalAlpha = skarp ? 1 : 1 - Math.exp(-dt / EFTERLYS);
    ritning.fillStyle = '#000';
    ritning.fillRect(0, 0, SB, SH);
    ritning.globalAlpha = 1;
    ritning.imageSmoothingEnabled = false;
    ritning.drawImage(punktbild, KANT_X, KANT_Y, 2 * BREDD, 3 * HOJD);
    if (!skarp) {
        sken_ritning.clearRect(0, 0, SB, SH);
        sken_ritning.drawImage(skarm, 0, 0);
    }
}

/* Skenets radie följer skärmens storlek på sidan. */
function skenets_radie() {
    sken.style.filter = 'blur(' + (SKEN * skarm.clientWidth / SB).toFixed(2) + 'px)';
}
new ResizeObserver(skenets_radie).observe(skarm);

$('farg').addEventListener('change', () => {
    e.farg($('farg').checked ? 1 : 0);
    e.bild(1);
    if ($('farg').checked)
        meddela('Färg (om): som om ABC 802 hade haft färg, som ABC 800 C. Maskinen själv visar inte färg.',
                'Colour (what if): as if the ABC 802 had colour, like the ABC 800 C. The machine itself shows no colour.');
    visa_lagen();
    skarm.focus();
});

$('skarp').addEventListener('change', () => {
    $('ram').classList.toggle('skarp', $('skarp').checked);
    visa_lagen();
    skarm.focus();
});

$('kopiera').addEventListener('click', async () => {
    const avkoda = new TextDecoder();
    const rader = [];
    for (let rad = 0; rad < e.rader(); rad++) {
        const p = e.textrad(rad);
        const minnet = new Uint8Array(e.memory.buffer, p, 80 * 4 + 1);
        rader.push(avkoda.decode(minnet.subarray(0, minnet.indexOf(0))));
    }
    while (rader.length && rader[rader.length - 1] === '')
        rader.pop();
    try {
        await navigator.clipboard.writeText(rader.join('\n') + '\n');
        meddela('Skärmens text ligger i urklipp.', 'The text on the screen is on the clipboard.');
    } catch (fel) {
        meddela('Webbläsaren släppte inte fram urklipp.', 'The browser did not allow the clipboard.');
    }
    skarm.focus();
});

/* ---------------------------------------------------------------- */
/* Lamporna                                                          */

/* En lampa i panelens list, som i ABC80-emulatorn: grön (pa) när något
 * är inkopplat, ljusare (rorelse) i LAMPAN_LYSER ms efter att raknare
 * har ändrats, och gul (varning) när varning är sant. */
const LAMPAN_LYSER = 250;
const lampor_sett = {}, lampor_tand = {};

function lampa(id, pa, raknare, varning = false) {
    const nu = performance.now();
    if (raknare !== lampor_sett[id]) {
        if (lampor_sett[id] !== undefined)
            lampor_tand[id] = nu;
        lampor_sett[id] = raknare;
    }
    const ror = nu - (lampor_tand[id] ?? -Infinity) < LAMPAN_LYSER;
    const lage = varning ? 'varning' : ror ? 'rorelse' : pa ? 'pa' : '';
    const el = $('lampa_' + id);
    const klass = 'lampa' + (lage ? ' ' + lage : '');
    if (el.className !== klass)
        el.className = klass;
    const titel = lage === 'varning'
        ? t('Det kommer något på en kontakt som inte visas; välj den i listan',
            'Something is coming out on a connector that is not shown; choose it in the list')
        : lage === 'rorelse' ? t('Används', 'In use')
        : lage === 'pa' ? t('Inkopplad', 'Connected') : '';
    if (el.title !== titel)
        el.title = titel;
}

function visa_lamporna() {
    lampa('skiva', !!e.skivans_data(), e.skivan_rord());
    lampa('natet', $('natet').checked, e.nat_anrop());
    lampa('kassett', !!e.bandets_data(), e.motor() ? performance.now() : 0);
    lampa('kontakter', $('skrivarkort').checked || !!(motpart && motpart.uppkopplad()),
          ut_totalt + in_totalt, performance.now() - ut_annan < 1000);
}

/* ---------------------------------------------------------------- */
/* Tiden                                                             */

let varv_antal = 0;

function varv() {
    const nu = performance.now() / 1000;
    if (!e.tangenter_klara()) {
        /* Tangentkön: så fort det går. */
        const slut = performance.now() + SNABBT_MS;
        while (!e.tangenter_klara() && performance.now() < slut)
            e.bild(0);
        e.bild(1);
        nasta = null;
    } else {
        /* Klockan bestämmer. */
        if (nasta === null || nu - nasta > 0.25)
            nasta = nu;
        while (nasta <= nu) {
            nasta += BILD;
            e.bild(nasta > nu ? 1 : 0);
        }
    }
    const pipade = e.pip() !== pipen;
    if (pipade) {
        pipen = e.pip();
        pip();
    }
    summern(pipade);
    rita();
    hamta_ut();
    hamta_trafiken();
    fyll_in();
    visa_lamporna();
    if (++varv_antal % 15 === 0) {
        visa_kassetten();
        visa_kontakterna();
    }
    if (varv_antal % 60 === 0)
        visa_katalogen();
    if (varv_antal % 15 === 0) {
        nat_klockan();
        visa_natet();
    }
    requestAnimationFrame(varv);
}

/* ---------------------------------------------------------------- */
/* Kom igång                                                         */

/* Rutan visas av sig själv första gången (sparas i webbläsaren). */
const HJALP_NYCKEL = 'abc802_kom_igang';
function kom_igang() {
    const hjalp = $('hjalp');
    if (typeof hjalp.showPopover !== 'function')
        return;
    try {
        if (localStorage.getItem(HJALP_NYCKEL))
            return;
        localStorage.setItem(HJALP_NYCKEL, '1');
    } catch (fel) { return; }
    hjalp.showPopover();
}
/* Utan popover: knappen och ✕ växlar klassen oppen. */
if (typeof $('hjalp').showPopover !== 'function')
    for (const knapp of [$('hjalp_visa'), $('hjalp').querySelector('.stang')])
        knapp.addEventListener('click', () => $('hjalp').classList.toggle('oppen'));
$('hjalp').addEventListener('toggle', h => {
    if (h.newState === 'closed')
        skarm.focus();
});

/* ---------------------------------------------------------------- */
/* Starten                                                           */

(async function () {
    const fraga = new URLSearchParams(location.search);
    let valt = fraga.get('sprak') || fraga.get('lang');
    if (!valt)
        try { valt = localStorage.getItem(SPRAK_NYCKEL); } catch (fel) { /* inget sparat */ }
    if (!valt)
        valt = /^sv/i.test(navigator.language || '') ? 'sv' : 'en';
    paneler_las_in();

    const { instance } = await WebAssembly.instantiate(base64(FILER.wasm), {});
    e = instance.exports;
    ROM.rom = base64(FILER.rom);
    ROM.natrom = base64(FILER.natrom);
    ROM.tecken = base64(FILER.tecken);

    if (fraga.get('tecken') === '40')
        $('tecken').value = '40';
    if (fraga.get('hz') === '60')
        $('hz').value = '60';
    if (fraga.get('skarp') === 'pa') {
        $('skarp').checked = true;
        $('ram').classList.add('skarp');
    }
    if (fraga.get('farg') === 'pa') {
        $('farg').checked = true;
        e.farg(1);
    }
    if (fraga.get('nat') === 'pa')
        $('natet').checked = true;
    nat_klockan();
    nat_prov();
    if (fraga.get('ljud') === 'av')
        ljud_valt = false;
    byt_sprak(valt);
    /* ?cpm=pa från förr: CP/M har en egen sida nu. */
    if (fraga.get('cpm') === 'pa' && !FILER.cpm) {
        fraga.delete('cpm');
        location.replace(SIDORNA.cpm + '?' + fraga);
        return;
    }
    starta();
    try {
        const skivan = { grund: ['tidsprov', 'tidsprov.img'], forth: ['forth', 'forth.img'],
                         cpm: ['cpm', 'cpm.img'] }[FILER.sida];
        if (fraga.get('skiva') === 'program' && FILER.program)
            skivan.splice(0, 2, 'program', 'program.img');
        if (fraga.get('skiva') === 'pascal' && FILER.pascal)
            skivan.splice(0, 2, 'pascal', 'pascal.img');
        satt_i(await packa_upp(FILER[skivan[0]]), skivan[1]);
    } catch (fel) {
        meddela('Webbläsaren kan inte packa upp skivorna.', 'The browser cannot unpack the disks.');
    }
    if (fraga.get('k')) {
        e.paus(1000);                        /* starten */
        skriv(fraga.get('k'), true);
    } else
        kom_igang();
    skenets_radie();
    skarm.focus();
    requestAnimationFrame(varv);
})();
