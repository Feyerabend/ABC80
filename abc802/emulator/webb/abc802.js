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
 * lämnas till webb.c så fort maskinens kö har plats.
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
    $('lage_skarm').textContent = $('skarp').checked ? t('skarp', 'sharp') : t('fosfor', 'phosphor');
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
    const rom = lagg_i_minnet(ROM.rom);
    const tecken = lagg_i_minnet(ROM.tecken);
    e.starta(rom, ROM.rom.length, tecken, ROM.tecken.length,
             $('tecken').value === '80' ? 1 : 0, $('hz').value === '60' ? 1 : 0);
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

$('skiva_prov').addEventListener('click', async () => {
    if (satt_i(await packa_upp(FILER.tidsprov), 'tidsprov.img'))
        meddela('Tidsproven sitter i DR0:.', 'The benchmarks are in DR0:.');
    skarm.focus();
});

$('skiva_tom').addEventListener('click', async () => {
    if (satt_i(await packa_upp(FILER.tom), 'tom.img'))
        meddela('En tom skiva sitter i DR0:.', 'An empty disk is in DR0:.');
    skarm.focus();
});

$('skiva_cpm').addEventListener('click', async () => {
    if (satt_i(await packa_upp(FILER.cpm), 'cpm.img')) {
        starta();
        meddela('CP/M 2.2 startar från DR0:.', 'CP/M 2.2 is starting from DR0:.');
    }
    skarm.focus();
});

/* Forth (../forth/): skivan med FORTH.ABS sätts i och maskinen startas
 * om. BASICINI.SYS på skivan laddar Forth och startar det. */
$('skiva_forth').addEventListener('click', async () => {
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
const in_ko = [[], [], []];                  /* bytes som väntar på maskinen */
const SKARM_TILL = Object.fromEntries(
    Object.entries(SVENSKA).filter(([c]) => c !== '¤').map(([c, k]) => [k, c]));

function som_text(bytes) {
    let text = '';
    for (const b of bytes) {
        const c = b & 0x7F;
        if (c === 0x0A || c === 0x0C)
            text += '\n';
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
ut_lapp.addEventListener('toggle', () => {
    if (!ut_oppen())
        return;
    try {
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

function visa_utskriften() {
    const k = kontakten();
    const ruta = $('utskrift');
    const nere = ruta.scrollTop + ruta.clientHeight >= ruta.scrollHeight - 4;
    ruta.textContent = utdata[k].map(som_text).join('');
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
    knapp.title = t('Tangentbordets pip (kod 7)', 'The keyboard beep (code 7)');
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
    if (e.pip() !== pipen) {
        pipen = e.pip();
        pip();
    }
    rita();
    hamta_ut();
    fyll_in();
    if (++varv_antal % 15 === 0) {
        visa_kassetten();
        visa_kontakterna();
    }
    if (varv_antal % 60 === 0)
        visa_katalogen();
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
    ROM.tecken = base64(FILER.tecken);

    if (fraga.get('tecken') === '40')
        $('tecken').value = '40';
    if (fraga.get('hz') === '60')
        $('hz').value = '60';
    if (fraga.get('skarp') === 'pa') {
        $('skarp').checked = true;
        $('ram').classList.add('skarp');
    }
    if (fraga.get('ljud') === 'av')
        ljud_valt = false;
    byt_sprak(valt);
    starta();
    try {
        if (fraga.get('cpm') === 'pa')
            satt_i(await packa_upp(FILER.cpm), 'cpm.img');
        else
            satt_i(await packa_upp(FILER.tidsprov), 'tidsprov.img');
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
