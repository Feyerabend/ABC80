/*
 * abc80.js -- emulatorn i webbläsaren, till boken ABC80 inifrån
 *
 * Kärnan är WebAssembly (webb.c och karna/, make webb); här finns allt
 * runt den: skärmen på en canvas, tangentbordet, ljudet med Web Audio,
 * filerna och knapparna. bygg.sh lägger in WebAssembly-koden, ROM:arna,
 * exempeldisketten, Forth-kretsarna, Forthaid, bokens exempel 1-6,
 * MÅLARE-bandet och Genesis-demot (de sex om de finns) och CP/M:s start-PROM och
 * systemskiva som base64 i FILER, så att sidan inte behöver hämta något.
 *
 * Tiden: en bild är 20 ms. Med ljudet på följer emulatorn ljudets
 * klocka, och varje bilds prov läggs i en AudioBuffer som spelas precis
 * efter den förra; utan ljud följer den performance.now(). Bilderna
 * körs lite före (LJUD_FORE), så att ljudet inte hackar. Medan
 * kassettens motor går körs maskinen fortare om "snabbt" är ikryssat.
 *
 * Bandets eget ljud: WAV-filen läses här (avkoda_bandet) och räknas om
 * till provtakten, och medan bandet går (och inte snabbt) läggs det som
 * passerar huvudet till bildens prov. ▶ är PLAY (nere eller uppe);
 * utan "fjärrstyrd" stannar bandet inte när ROM:en slår av motorn, så
 * hörs musiken efter programmet i Genesis-demots band. ⏪ och ⏩
 * spolar (SPOLFART gånger vanlig fart) det band som spelas upp, och
 * räkneverket räknar varv på upptagningsspolen, som på en bandspelare.
 *
 * Disketten: kortet skriver i avbilden i WebAssemblys minne, så det som
 * sparas med SAVE DR0: finns där tills maskinen startas om med en annan
 * diskett. "Spara disketten" laddar ner avbilden som den är nu, och
 * "Katalogen" visar filerna, läst ur katalogen i sektor 16-23 som
 * ABC-DOS ordnar den (band 2, kapitlet om FD2).
 *
 * CP/M: UNI-80 med vår start-PROM (cpm/start.asm), som -cpm i
 * terminalen. Systemskivan sitter i FD2 enhet 0 (A:) och skivan med BBC
 * BASIC (cpm/b.dsk) i enhet 1 (B:); C: är ABC80:s eget RAM och D: skivan
 * med Zork I (cpm/d.dsk) på kort 44 (ABC 832, 640 KB). Den ligger i sidan
 * utan de $E5 som avslutar den. Skivorna i det läge som inte
 * är valt ligger undan, så att ABC-DOS-disketten och CP/M-skivorna
 * finns kvar när man byter fram och tillbaka.
 *
 * CMOS-minnet: 2 KB RAM på $5000, som Super Smartaids med batteri. Det
 * läggs in efter varje start och sparas i webbläsaren (localStorage),
 * en gång i sekunden när det har ändrats och när sidan stängs. Forth
 * med CMOS är tolken från exempel 7 med de egna orden där.
 *
 * Skärmen är en vit fosforskärm som i SDL-fönstret (se fosfor()),
 * eller med rutan skarp skarpa punkter.
 *
 * Tangenterna går som i terminalen (vard/terminal.c): Enter är RETURN
 * ($0D), backsteg och vänsterpil $08, högerpil $09, Ctrl-C BREAK ($03),
 * och å ä ö é ü Å Ä Ö É Ü blir ABC80:s koder. Ett program som skrivs in
 * får en paus på 200 ms efter varje rad, som -f.
 */

'use strict';

const BILD = 0.02;                           /* sekunder per bild */
const LJUD_FORE = 0.06;                      /* så långt före ljudet körs vi */
const SNABBT_MS = 14;                        /* körtid per skärmbild när det går fort */

const SVENSKA = { 'å': 0x7D, 'ä': 0x7B, 'ö': 0x7C, 'é': 0x60, 'ü': 0x7E,
                  'Å': 0x5D, 'Ä': 0x5B, 'Ö': 0x5C, 'É': 0x40, 'Ü': 0x5E, '¤': 0x24 };

const $ = (id) => document.getElementById(id);
const skarm = $('skarm');
const ritning = skarm.getContext('2d');

let e;                                       /* WebAssemblys exporter */
let ljud = null;                             /* AudioContext, eller null utan Web Audio */
let provtakt = 48000;                        /* ljudets prov/s (PROVTAKT) */
let nasta = null;                            /* klockans tid för nästa bild */
let skivan = null;                           /* disketten (bytes), eller null */
let skivans_minne = 0;                       /* avbilden i WebAssemblys minne */
let skivans_namn = 'diskett.dsk';            /* namnet när den sparas */
let skiva_b = null;                          /* CP/M:s B: (FD2 enhet 1), eller null */
let skiva_b_minne = 0;
let skiva_b_namn = 'b.dsk';
let skiva_d = null;                          /* CP/M:s D: (kort 44 enhet 0), eller null */
let skiva_d_minne = 0;
let skiva_d_namn = 'd.dsk';
let bandljudet = null;                       /* bandets ljud i provtakten, eller null */

function base64(text) {
    const s = atob(text);
    const b = new Uint8Array(s.length);
    for (let k = 0; k < s.length; k++)
        b[k] = s.charCodeAt(k);
    return b;
}

/* Språket: sv eller en. Mallens text står på båda med lang, och CSS
 * döljer den som inte gäller; knappar och val har engelskan i data-en och
 * title-texten i data-title-en. Här byts de, och texterna i koden går
 * genom t(svenska, engelska). Valet sparas i webbläsaren; ?sprak= går
 * före, och utan något val gäller webbläsarens språk. */
const SPRAK_NYCKEL = 'abc80_sprak';
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
    if (e) {
        visa_ljudet();
        papper_rubrik();
        if (!$('katalog').hidden)
            visa_katalogen();
        visa_lagen(true);
    }
}

$('sprak').addEventListener('click', () => {
    byt_sprak(t('en', 'sv'));
    try { localStorage.setItem(SPRAK_NYCKEL, sprak); } catch (fel) { /* sparas inte */ }
    skarm.focus();
});

/* ---------------------------------------------------------------- */
/* Panelerna                                                         */

/* Vilka som är öppna sparas; från början bara Maskinen. ⓘ i listen
 * fäller ut panelen och visar eller döljer dess hjälptext. */
const PANEL_NYCKEL = 'abc80_paneler';
const paneler = [...document.querySelectorAll('details.panel')];

function paneler_spara() {
    try {
        localStorage.setItem(PANEL_NYCKEL,
                             JSON.stringify(paneler.filter(p => p.open).map(p => p.id)));
    } catch (fel) { /* sparas inte */ }
}

function paneler_las_in() {
    try {
        const oppna = JSON.parse(localStorage.getItem(PANEL_NYCKEL));
        if (Array.isArray(oppna))
            for (const p of paneler)
                p.open = oppna.includes(p.id);
    } catch (fel) { /* som i mallen */ }
}

/* Fäll ut en panel (när något händer i den). */
function oppna_panel(id) {
    const p = $(id);
    if (p && !p.open) {
        p.open = true;
        paneler_spara();
    }
}

for (const p of paneler) {
    p.addEventListener('toggle', paneler_spara);
    const info = p.querySelector('.info');
    const text = p.querySelector('.hjalptext');
    info?.addEventListener('click', (h) => {
        h.preventDefault();                  /* annars fälls panelen */
        h.stopPropagation();
        text.hidden = p.open ? !text.hidden : false;
        p.open = true;
        info.classList.toggle('pa', !text.hidden);
        if (!text.hidden)
            text.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
    });
}

/* Lägena i panelernas listar, så att de syns också ihopfällda. */
let lagen_nu = '';

function visa_lagen(alltid = false) {
    const b2 = b2_valt();
    const delar = [b2 ? 'BASIC II'
                   : t($('rom').value === 'gammal' ? 'gamla ROM:en' : 'nya ROM:en',
                       $('rom').value === 'gammal' ? 'old ROM' : 'new ROM')];
    if ($('tkn80').checked && !b2)
        delar.push('TKN80' + ($('fast').checked ? t(', fast', ', steady') : ''));
    if ($('skarp').checked)
        delar.push(t('skarp', 'sharp'));
    if (cpm_valt())
        delar.push('CP/M');
    const maskin = delar.join(' · ');
    let disk = '';
    if (skivan)
        disk = cpm_valt() ? [skivans_namn, skiva_b_namn, skiva_d_namn].join(' · ') : skivans_namn;
    const krets = $('krets');
    const program = krets && krets.value && !b2 ? krets.options[krets.selectedIndex].textContent : '';
    const skrivare = $('p40').checked && !b2 ? 'P40' : $('centronics').checked ? 'Centronics' : '';
    const skrivaren_text = skrivare ? skrivare + (papper_rader
        ? ' · ' + papper_rader + t(papper_rader === 1 ? ' rad' : ' rader',
                                    papper_rader === 1 ? ' line' : ' lines') : '') : '';
    const cmos_text = $('cmos').checked && !b2 ? t('på', 'on') : '';
    const nytt = [maskin, disk, program, skrivaren_text, cmos_text].join('|');
    if (!alltid && nytt === lagen_nu)
        return;
    lagen_nu = nytt;
    $('lage_maskin').textContent = maskin;
    $('lage_disk').textContent = disk;
    $('lage_program').textContent = program;
    $('lage_skrivaren').textContent = skrivaren_text;
    $('lage_cmos').textContent = cmos_text;
    /* Fast markör gäller bara TKN80. Med BASIC II går varken kretsarna,
     * CMOS-minnet, TKN80, CP/M, P40 eller kassetten: ROM:en har ingen
     * CAS: (området är nollor, CAS: ger fel 21), som Ekmans lappar. */
    for (const namn of ['tkn80', 'cpm', 'krets', 'cmos', 'p40',
                        'band', 'spelain', 'spara', 'play', 'bakat', 'framat', 'malare'])
        if ($(namn))
            $(namn).disabled = b2;
    $('fast').disabled = b2 || !$('tkn80').checked;
}

/* Bytes in i WebAssemblys minne (extra nollor efter); pekaren. */
function lagg_i_minnet(data, extra = 0) {
    const p = e.minne(data.length + extra);
    const minnet = new Uint8Array(e.memory.buffer, p, data.length + extra);
    minnet.set(data);
    minnet.fill(0, data.length);
    return p;
}

/* ---------------------------------------------------------------- */
/* CMOS-minnet                                                       */

const CMOS_ADRESS = 0x5000, CMOS_STORLEK = 0x800, CMOS_NYCKEL = 'abc80-cmos';
let cmos = new Uint8Array(CMOS_STORLEK);    /* det som är sparat */
let cmos_pekare = 0;                        /* i WebAssemblys minne, 0 = av */
let cmos_sparat = 0;                        /* performance.now() */

function cmos_las_in() {
    try {
        const t = localStorage.getItem(CMOS_NYCKEL);
        if (t) {
            const b = base64(t);
            if (b.length === CMOS_STORLEK)
                cmos = b;
        }
    } catch (fel) { /* utan localStorage: tomt */ }
}

function cmos_nu() {
    return new Uint8Array(e.memory.buffer, cmos_pekare, CMOS_STORLEK);
}

function cmos_skriv() {
    try {
        localStorage.setItem(CMOS_NYCKEL, btoa(String.fromCharCode(...cmos)));
    } catch (fel) { /* fullt eller förbjudet: bara i minnet */ }
}

/* Det som står i minnet nu sparas, om det har ändrats. */
function cmos_spara() {
    if (!cmos_pekare)
        return;
    const nu = cmos_nu();
    if (nu.every((b, k) => b === cmos[k]))
        return;
    cmos = nu.slice();
    cmos_skriv();
}

/* Nytt innehåll (en fil eller nollor), också i maskinen om det är på. */
function cmos_byt(data) {
    cmos = new Uint8Array(CMOS_STORLEK);
    cmos.set(data.subarray(0, CMOS_STORLEK));
    if (cmos_pekare)
        cmos_nu().set(cmos);
    cmos_skriv();
}

/* ---------------------------------------------------------------- */
/* Maskinen                                                          */

const ROM = {};

/* Disketten som den är nu, med det som kortet har skrivit, eller null. */
function skivan_nu() {
    if (!skivans_minne)
        return null;
    return new Uint8Array(e.memory.buffer, skivans_minne, skivan.length).slice();
}

function skiva_b_nu() {
    if (!skiva_b_minne)
        return null;
    return new Uint8Array(e.memory.buffer, skiva_b_minne, skiva_b.length).slice();
}

function skiva_d_nu() {
    if (!skiva_d_minne)
        return null;
    return new Uint8Array(e.memory.buffer, skiva_d_minne, skiva_d.length).slice();
}

const CPM_SKIVA = 40 * 8 * 256;              /* FD2: 80 KB per enhet */
const KORT44_SKIVA = 2560 * 256;             /* kort 44: 640 KB */

/* BASIC II för ABC80 (32 KB, med egen DOS och PR:) i stället för ABC80-ROM:en. */
function b2_valt() {
    return $('rom').value === 'b2';
}

function cpm_valt() {
    return !!$('cpm')?.checked && !b2_valt();
}

/* En CP/M-systemskiva börjar med startblocket, "CP/M" (cpm/start.asm). */
function ar_cpm(d) {
    return d && d.length >= 4 && String.fromCharCode(...d.subarray(0, 4)) === 'CP/M';
}


/* Starta maskinen; med en diskett sitter ABC-DOS på $6000 och
 * disketten i FD2 (kort 45). Utan argument sitter samma diskett kvar,
 * med det som har sparats på den. En krets sitter på $4000, som
 * Smartaid (-l forth.bin@4000); Forth med CMOS slår på CMOS-minnet.
 * Med CP/M är disketten A: (utan den systemskivan), b B: (utan den
 * skivan med BBC BASIC) och d D: (utan den skivan med Zork I), och
 * ABC-DOS sitter inte i. Med BASIC II (b2_valt) är RAM:et 32 KB, som i
 * Ekmans 64K med karta 1, tolkens egen DOS läser FD2 som MO0:, och
 * kretsarna, CMOS-minnet och TKN80 sitter inte i. ROM:en bygger på
 * Ekmans lappar (band 2, avs:abc800:ekman), men vem som gjorde den vet
 * vi inte; därför heter valet BASIC II (64K), inte efter Ekman. */
function starta(diskett = skivan_nu(), b = skiva_b_nu(), d = skiva_d_nu()) {
    cmos_spara();                            /* innan minnet töms */
    const cpm = cpm_valt();
    const b2 = b2_valt();
    const rom = ROM[$('rom').value];
    const p = lagg_i_minnet(rom);
    e.starta(b2 ? 32 : 16, p, rom.length, provtakt);
    e.slapp(p);
    knappar();
    if (skivans_minne)                       /* kortet är tomt nu */
        e.slapp(skivans_minne);
    if (skiva_b_minne)
        e.slapp(skiva_b_minne);
    if (skiva_d_minne)
        e.slapp(skiva_d_minne);
    skivans_minne = skiva_b_minne = skiva_d_minne = 0;
    if (cpm && !diskett) {
        diskett = ROM.cpm_skiva.slice();
        skivans_namn = 'cpm.dsk';
    }
    if (cpm && !b) {
        b = ROM.cpm_b.slice();
        skiva_b_namn = 'b.dsk';
    }
    skivan = diskett;
    if (cpm && !d) {
        d = ROM.cpm_d.slice();
        skiva_d_namn = 'd.dsk';
    }
    skiva_b = cpm ? b : null;
    skiva_d = cpm ? d : null;
    $('katalog').hidden = true;
    if (skivan) {
        if (!cpm && !b2) {
            const dos = lagg_i_minnet(ROM.dos);
            e.krets(0x6000, dos, ROM.dos.length);
            e.slapp(dos);
        }
        /* Kortet skriver i avbilden, så den ligger kvar (256 bytes över). */
        skivans_minne = lagg_i_minnet(skivan, 256);
        e.skiva(45, 0, skivans_minne, skivan.length, 0);
    }
    if (skiva_b) {
        skiva_b_minne = lagg_i_minnet(skiva_b, 256);
        e.skiva(45, 1, skiva_b_minne, skiva_b.length, 0);
    }
    if (skiva_d) {
        skiva_d_minne = lagg_i_minnet(skiva_d, 256);
        e.skiva(44, 0, skiva_d_minne, skiva_d.length, 0);
    }
    const val = b2 ? '' : $('krets')?.value;
    const ex = EXEMPEL[val];
    if (ex) {
        for (const namn of ex.filer) {
            const data = FILER.exempel[namn];
            const p = lagg_i_minnet(data);
            if (ex.filer.length > 1)
                e.bank(ex.adress, p, data.length);
            else
                e.krets(ex.adress, p, data.length);
            e.slapp(p);
        }
    } else if (val === 'forthaid2') {
        /* Den fasta delen på $4000 och bankerna i fönstret på $5000. */
        const data = FILER.forthaid2;
        const krets = lagg_i_minnet(data);
        e.krets(0x4000, krets, data.length);
        e.slapp(krets);
        for (const bank of FILER.forthaid2_banker) {
            const p = lagg_i_minnet(bank);
            e.bank(0x5000, p, bank.length);
            e.slapp(p);
        }
    } else if (val) {
        const data = val === 'cmos' ? FILER.krets_cmos
                   : val === 'forthaid' ? FILER.forthaid : FILER.krets;
        const krets = lagg_i_minnet(data);
        e.krets(0x4000, krets, data.length);
        e.slapp(krets);
        if (val === 'cmos')
            $('cmos').checked = true;
    }
    cmos_pekare = 0;
    if ($('cmos').checked && !b2) {
        const p = lagg_i_minnet(cmos);
        cmos_pekare = e.cmos(CMOS_ADRESS, p, CMOS_STORLEK);
        e.slapp(p);
    }
    if ($('tkn80').checked && !b2) {
        const p = lagg_i_minnet(ROM.tkn80);
        e.tkn80(p, ROM.tkn80.length);
        e.slapp(p);
    }
    if (cpm) {
        const p = lagg_i_minnet(ROM.cpm_start);
        e.cpm(p, ROM.cpm_start.length);
        e.slapp(p);
    }
    papper_tomt();
    skrivaren_koppla();
    e.aterstall();
    nasta = null;
}

/* ---------------------------------------------------------------- */
/* Tangenterna                                                       */

function tangent_kod(tecken) {
    if (tecken in SVENSKA)
        return SVENSKA[tecken];
    const kod = tecken.charCodeAt(0);
    return kod < 0x80 ? kod : 0;
}

/* Text som tangenter; radslut blir RETURN med en paus efter. Med
 * flaggor gäller -k: \r, \w ms, \xHH, \\. Med v24 går texten till
 * V24 i stället, som -s (utan pauser). */
function skriv(text, flaggor = false, v24 = false) {
    const ut = v24 ? e.v24 : e.tangent;
    const paus = v24 ? () => {} : e.paus;
    for (let k = 0; k < text.length; k++) {
        const c = text[k];
        if (flaggor && c === '\\' && k + 1 < text.length) {
            const n = text[++k];
            if (n === 'r')
                ut(0x0D);
            else if (n === 'w') {
                const m = /^\d+/.exec(text.slice(k + 1));
                paus(m ? +m[0] : 0);
                k += m ? m[0].length : 0;
            } else if (n === 'x') {
                ut(parseInt(text.substr(k + 1, 2), 16) & 0xFF);
                k += 2;
            } else if (n !== '.')
                ut(tangent_kod(n));
            continue;
        }
        if (c === '\r')
            continue;
        if (c === '\n') {
            ut(0x0D);
            paus(200);
            continue;
        }
        const kod = tangent_kod(c);
        ut(kod || 0x3F);
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
        kod = tangent_kod(h.key.toUpperCase()) & 0x1F;   /* Ctrl-Ä är ESC */
    } else if (h.key.length === 1)
        kod = tangent_kod(h.key);
    else
        kod = { Enter: 0x0D, Backspace: 0x08, ArrowLeft: 0x08,
                ArrowRight: 0x09, Escape: 0x1B, Tab: 0x09, Delete: 0x7F }[h.key] || 0;
    if (!kod)
        return;
    h.preventDefault();
    if (h.repeat)
        return;                              /* bit 7 är redan satt */
    vack_ljudet();
    nere_kod = h.code;
    e.tangent_ned(kod);
});

/* Bit 7 i port $38 är satt medan tangenten hålls nere, som på maskinen
 * (program som läser INP(56) ser den då, och BASIC II upprepar själv).
 * Webbläsarens upprepning läggs inte i kön. */
let nere_kod = null;
function slapp_tangenten() {
    if (nere_kod !== null) {
        nere_kod = null;
        e.tangent_upp();
    }
}
skarm.addEventListener('keyup', (h) => {
    if (h.code === nere_kod)
        slapp_tangenten();
});
skarm.addEventListener('blur', slapp_tangenten);

/* Ett klick i ramen runt bilden gäller också skärmen. */
$('ram')?.addEventListener('mousedown', (h) => {
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
/* Ljudet                                                            */

/* Ljudet är på från början, men webbläsaren släpper inte fram det
 * förrän man har klickat eller skrivit på sidan; då väcks det. Provtakten
 * är fast, 48 kHz (kärnans ljud får den i starta), och webbläsaren räknar
 * om till ljudkortets takt. WebKit är nyckfullt: ett ljud som skapas
 * innan man har rört sidan kan stå som igång men vara tyst, också efter
 * en omladdning. Därför skapas ljudet om, och låses upp med ett tyst
 * prov, i själva klicket eller tangenttrycket, om det inte går eller om
 * dess klocka står still (ljudet_star). */
const PROVTAKT = 48000;
let ljud_valt = true;
let ljud_upplast = false;                    /* ett tyst prov har spelats i en användarhandling */
let ljud_sedd = null;                        /* {ljud, tid, nu}: när currentTime senast ändrades */

function nytt_ljud() {
    let nytt = null;
    try {
        nytt = new AudioContext({ sampleRate: PROVTAKT });
    } catch (fel) {
        try { nytt = new AudioContext(); } catch (fel2) { return null; }
    }
    nytt.addEventListener('statechange', visa_ljudet);
    return nytt;
}

ljud = nytt_ljud();
provtakt = ljud ? ljud.sampleRate : PROVTAKT;   /* 48 kHz, om webbläsaren inte vägrar */

/* Ett nytt ljud i stället för det gamla, om det får samma provtakt. */
function byt_ljud() {
    const gammalt = ljud;
    const nytt = nytt_ljud();
    if (nytt && nytt.sampleRate === provtakt) {
        ljud = nytt;
        ljud_upplast = false;
        ljud_sedd = null;
        nasta = null;
        tysta_kallorna();
        if (gammalt.state !== 'closed')
            gammalt.close().catch(() => {});
    } else if (nytt)
        nytt.close().catch(() => {});
}

/* Anropas i klick och tangenttryck (användarhandlingar). */
function vack_ljudet() {
    if (!ljud_valt || !ljud)
        return;
    if (ljud.state === 'closed' || ljudet_star())
        byt_ljud();
    if (ljud.state !== 'running')
        ljud.resume().catch(() => {});
    if (!ljud_upplast) {
        const tyst = ljud.createBufferSource();
        tyst.buffer = ljud.createBuffer(1, 1, provtakt);
        tyst.connect(ljud.destination);
        tyst.start(0);
        ljud_upplast = true;
    }
    visa_ljudet();
}

/* Proven som ligger och väntar i ljudet; de stoppas när ljudet stängs av
 * eller byts. */
const kallorna = new Set();

function tysta_kallorna() {
    for (const kalla of kallorna) {
        try { kalla.stop(); } catch (fel) {}
        kalla.disconnect();
    }
    kallorna.clear();
}

/* Igång men tyst: klockan har stått still i en halv sekund. */
function ljudet_star() {
    if (!ljud || ljud.state !== 'running')
        return false;
    const nu = performance.now();
    if (!ljud_sedd || ljud_sedd.ljud !== ljud || ljud_sedd.tid !== ljud.currentTime) {
        ljud_sedd = { ljud, tid: ljud.currentTime, nu };
        return false;
    }
    return nu - ljud_sedd.nu > 500;
}

function visa_ljudet() {
    const knapp = $('ljud');
    let text = t('Ljud: av', 'Sound: off');
    if (!ljud)
        text = t('Inget ljud', 'No sound');
    else if (ljud_valt)
        text = ljud.state === 'running' && !ljudet_star() ? t('Ljud: på', 'Sound: on')
            : t('Ljud: klicka på skärmen', 'Sound: click the screen');
    if (knapp.textContent !== text)
        knapp.textContent = text;
    knapp.title = t('Ljudet från SN76477 (OUT 6) och bandet', 'The sound from the SN76477 (OUT 6) and the tape') +
        (ljud ? ' (' + ljud.state + ', ' + ljud.sampleRate + ' Hz)' : '');
    knapp.classList.toggle('pa', ljud_valt && !!ljud);
}

for (const namn of ['pointerdown', 'mousedown', 'click', 'touchend', 'keydown', 'keyup'])
    document.addEventListener(namn, vack_ljudet, true);
/* Tillbaka till fliken: Safari kan ha avbrutit ljudet (interrupted). */
document.addEventListener('visibilitychange', () => {
    if (!document.hidden && ljud_valt && ljud && ljud.state !== 'running')
        ljud.resume().catch(() => {});
    nasta = null;
});

/* Knappen. På: ett helt nytt ljud, upplåst i klicket. Av: proven som
 * väntar stoppas och ljudet stängs, så att inget hörs förrän det slås på
 * igen. */
$('ljud').addEventListener('click', () => {
    ljud_valt = !ljud_valt;
    if (ljud) {
        if (ljud_valt) {
            byt_ljud();
            vack_ljudet();
        } else {
            tysta_kallorna();
            if (ljud.state !== 'closed')
                ljud.close().catch(() => {});
        }
    }
    nasta = null;
    visa_ljudet();
    skarm.focus();
});

function ljudet_gar() {
    return ljud_valt && ljud && ljud.state === 'running' && !ljudet_star();
}

/* Bildens prov från tiden tid; bandets ljud från fran sekunder in på
 * bandet, om fran inte är null. */
function spela(antal, tid, fran) {
    const buffert = ljud.createBuffer(1, antal, provtakt);
    const ut = buffert.getChannelData(0);
    const prov = new Int16Array(e.memory.buffer, e.ljudprov(), antal);
    for (let k = 0; k < antal; k++)
        ut[k] = prov[k] / 32768;
    if (fran !== null) {
        const forsta = Math.round(fran * provtakt);
        const n = Math.min(antal, bandljudet.length - forsta);
        for (let k = 0; k < n; k++)
            ut[k] += 0.8 * bandljudet[forsta + k];
    }
    const kalla = ljud.createBufferSource();
    kalla.buffer = buffert;
    kalla.connect(ljud.destination);
    kalla.onended = () => kallorna.delete(kalla);
    kallorna.add(kalla);
    kalla.start(tid);
}

/* ---------------------------------------------------------------- */
/* Tiden och skärmen                                                 */

function klockan() {
    return ljudet_gar() ? ljud.currentTime : performance.now() / 1000;
}

/* ---------------------------------------------------------------- */
/* Skrivaren P40                                                     */

/* Papperet ritas som -CP i terminalen (karna/p40.c): en kolumn är 4
 * punkter bred, en nålrad 4 hög och raderna 40 isär, med en kant om 8.
 * Anslagen läses ur WebAssemblys minne, 12 bytes vart: x i fjärdedels
 * kolumner, pappersraden och nålarna (bit 0 översta). */
const P40_BANAN = 262, P40_KANT = 8, P40_NALRAD = 4, P40_RADHOJD = 40, P40_ANSLAG = 12;
const pappret = $('pappret');
const papperet_ritas = pappret.getContext('2d');
let papper_ritat = 0;                        /* anslag som är ritade */
let papper_forsta = 0;                       /* pappersraden överst */
let papper_rader = 0;                        /* rader som får plats */
let papper_senast = 0;                       /* när det senast kom anslag (ms) */
const PAPPER_PAUS = 1500;                    /* så länge tyst: en ny utskrift */

/* Lappen: popover där webbläsaren har det, annars klassen oppen. */
const papperet = $('papper');
const med_popover = typeof papperet.showPopover === 'function';

function papper_oppet() {
    return med_popover ? papperet.matches(':popover-open') : papperet.classList.contains('oppen');
}

function papper_visa(visa) {
    if (visa === papper_oppet())
        return;
    if (med_popover)
        visa ? papperet.showPopover() : papperet.hidePopover();
    else
        papperet.classList.toggle('oppen', visa);
    if (visa)
        $('papper_knapp').classList.remove('nytt');
}

/* Skrivaren och drivrutinen in eller ur, som rutorna säger: P40 med
 * sin drivrutin eller Centronicskortet med 9704, båda på kort 60. Den
 * som kopplas ur först, så att den andras drivrutin får ligga kvar. */
/* Med BASIC II skriver PR: i ROM:en till kort 60, så 9704 behövs inte,
 * och P40 kopplas inte in. */
function skrivaren_koppla() {
    const b2 = b2_valt();
    for (const namn of ['p40', 'centronics'])
        if (!$(namn).checked || (b2 && namn === 'p40'))
            e[namn](0);
    for (const [namn, rom] of [['p40', ROM.p40], ['centronics', ROM.pr9704]])
        if ($(namn).checked && !(b2 && namn === 'p40')) {
            if (!b2) {
                const p = lagg_i_minnet(rom);
                e.krets(0x7800, p, rom.length);
                e.slapp(p);
            }
            e[namn](1);
        }
    if ($('centronics').checked)
        text_starta();
}

function papper_tomt() {
    text_tom();
    papper_ritat = 0;
    papper_rader = 0;
    papper_senast = 0;
    papper_visa(false);
    $('papper_knapp').disabled = true;
    $('papper_knapp').classList.remove('nytt');
    papper_rubrik();
}

function papper_rubrik() {
    $('papper_rubrik').textContent = !papper_rader ? t('Papperet', 'The paper')
        : t('Papperet, ' + papper_rader + (papper_rader === 1 ? ' rad' : ' rader'),
            'The paper, ' + papper_rader + (papper_rader === 1 ? ' line' : ' lines'));
}

function papper_anslag(fran, till) {
    return new DataView(e.memory.buffer, e.p40_anslag() + fran * P40_ANSLAG,
                        (till - fran) * P40_ANSLAG);
}

/* Rita det som har kommit till skrivaren sedan sist. */
function papper_rita() {
    if ($('p40').checked)
        p40_rita();
    else if ($('centronics').checked)
        text_rita();
}

/* Rita de anslag som har kommit sedan sist. Behövs fler rader görs
 * bilden om och allt ritas igen. */
function p40_rita() {
    const antal = e.p40_antal();
    if (antal === papper_ritat)
        return;
    if (antal < papper_ritat)
        papper_tomt();
    if (antal === 0)
        return;
    const ny_utskrift = performance.now() - papper_senast > PAPPER_PAUS;
    papper_senast = performance.now();
    let d = papper_anslag(papper_ritat, antal);
    if (papper_ritat === 0)
        papper_forsta = d.getInt32(4, true);
    const sista = d.getInt32((antal - papper_ritat - 1) * P40_ANSLAG + 4, true);
    if (sista - papper_forsta + 1 > papper_rader) {
        papper_rader = sista - papper_forsta + 1;
        pappret.width = P40_BANAN * 4 + 2 * P40_KANT;
        pappret.height = papper_rader * P40_RADHOJD + 2 * P40_KANT;
        papperet_ritas.fillStyle = '#fbfaf5';
        papperet_ritas.fillRect(0, 0, pappret.width, pappret.height);
        papper_ritat = 0;
        d = papper_anslag(0, antal);
    }
    papperet_ritas.fillStyle = '#202020';
    for (let k = 0; k < antal - papper_ritat; k++) {
        const x = d.getInt32(k * P40_ANSLAG, true);
        const rad = d.getInt32(k * P40_ANSLAG + 4, true) - papper_forsta;
        const nalar = d.getUint8(k * P40_ANSLAG + 8);
        for (let n = 0; n < 7; n++)
            if (nalar >> n & 1)
                papperet_ritas.fillRect(P40_KANT + x,
                                        P40_KANT + rad * P40_RADHOJD + n * P40_NALRAD, 3, 3);
    }
    papper_ritat = antal;
    papper_nytt(ny_utskrift);
    if (e.p40_fullt())
        meddela('Papperet är fullt: riv av det.', 'The paper is full: tear it off.');
}

/* Det har kommit något på papperet. En ny utskrift tar fram lappen;
 * stängs den under utskriften lyser bara knappen. */
function papper_nytt(ny_utskrift) {
    $('papper_knapp').disabled = false;
    papper_rubrik();
    if (ny_utskrift)
        papper_visa(true);
    else if (!papper_oppet())
        $('papper_knapp').classList.add('nytt');
    const ruta = $('papper_rulle');
    if (ny_utskrift || ruta.scrollTop + ruta.clientHeight >= ruta.scrollHeight - 80)
        ruta.scrollTop = ruta.scrollHeight;
}

/* ---------------------------------------------------------------- */
/* Centronicsskrivaren                                               */

/* En skrivare som Epsons MX-80 (och FX-80) med den svenska
 * teckenuppsättningen (som ABC80:s skärm, karna/tecken.c). Vagnen går i
 * 1/120 tum, och raden är 8 tum: 80 tecken i pica (10 per tum), 96 i
 * elite (12), 137 smala (17) och hälften så många breda. CR för vagnen
 * till början, LF skriver raden och matar fram papperet (och nästa rad
 * börjar till vänster, som på Epson), VT som LF, FF ger en ny sida, BS
 * och HT flyttar vagnen, CAN tömmer raden, och en full rad bryts.
 * SO gör resten av raden bred (DC4 tar bort det), SI smal (DC2 tar bort
 * det). ESC-följderna tas emot med sina argument, så att inget av dem
 * hamnar på papperet: ESC E/F fet, ESC G/H dubbelslag, ESC 4/5 kursiv,
 * ESC - n understruken, ESC W n bred, ESC M/P elite och pica, ESC ! n
 * allt på en gång, ESC R n landet, ESC D tabbarna, ESC $ läget och ESC @
 * som återställer. Grafiken (ESC K, L, Y, Z, * och ^) flyttar vagnen
 * men ritas inte. Ett tecken som slås över ett annat blir understruket
 * om det ena är _, och fett om de är lika. Papperet är sidorna, var och
 * en en lista med rader, och raderna tecknen med läge och stil. */
const TEXT_BREDD = 960, TEXT_RADER = 5000;
const TEXT_FET = 1, TEXT_DUBBEL = 2, TEXT_UNDER = 4, TEXT_KURSIV = 8;
/* Länderna (ESC R n) för # $ @ [ \ ] ^ ` { | } ~. */
const TEXT_PLATSER = [0x23, 0x24, 0x40, 0x5B, 0x5C, 0x5D, 0x5E, 0x60, 0x7B, 0x7C, 0x7D, 0x7E];
const TEXT_LAND = ['#$@[\\]^`{|}~', '#$à°ç§^`éùè¨', '#$§ÄÖÜ^`äöüß', '£$@[\\]^`{|}~',
                   '#$@ÆØÅ^`æøå~', '#¤ÉÄÖÅÜéäöåü', '#$@°\\é^ùàòèì', '₧$@¡Ñ¿^`¨ñ}~',
                   '#$@[¥]^`{|}~'];
const TEXT_SVERIGE = 5;
let text_sidor = [[]];
let text_rad = [];                           /* raden under vagnen */
let text_x = 0;                              /* vagnen, i 1/120 tum */
let text_fullt = false;                      /* sagt att papperet är fullt */
let text_stil, text_elite, text_smal, text_bred, text_bred_raden, text_land, text_tabbar;
let text_foljd = null;                       /* ESC-följden som tas emot */

/* Skrivaren som när den slås på (och ESC @). */
function text_starta() {
    text_stil = 0;
    text_elite = text_smal = text_bred = text_bred_raden = false;
    text_land = TEXT_SVERIGE;
    text_tabbar = null;                      /* var åttonde tecken */
    text_foljd = null;
}
text_starta();

function text_tom() {
    text_sidor = [[]];
    text_rad = [];
    text_x = 0;
    text_fullt = false;
    $('papper_text').textContent = '';
}

/* Ett teckens bredd i 1/120 tum. */
function text_steg() {
    const steg = text_smal ? 7 : text_elite ? 10 : 12;
    return text_bred || text_bred_raden ? 2 * steg : steg;
}

function text_antal_rader() {
    let antal = text_rad.length ? 1 : 0;
    for (const sida of text_sidor)
        antal += sida.length;
    return antal;
}

function text_radslut() {
    text_sidor[text_sidor.length - 1].push({ tecken: text_rad, html: text_html(text_rad) });
    text_rad = [];
    text_x = 0;
    text_bred_raden = false;
}

/* Lägg tecknet c där vagnen står. Står ett tecken redan där slås det
 * nya över det. */
function text_skriv(c) {
    const w = text_steg();
    if (text_x + w > TEXT_BREDD)
        text_radslut();
    const ny = { c: c, x: text_x, w: w, stil: text_stil };
    text_x += w;
    let i = text_rad.length;
    while (i > 0 && text_rad[i - 1].x > ny.x)
        i--;
    const gammal = i > 0 && text_rad[i - 1].x === ny.x ? text_rad[i - 1] : null;
    if (!gammal) {
        if (c !== ' ' || ny.stil & TEXT_UNDER)
            text_rad.splice(i, 0, ny);
        return;
    }
    gammal.stil |= ny.stil;
    if (c === '_')
        gammal.stil |= TEXT_UNDER;
    else if (gammal.c === '_' || gammal.c === ' ') {
        if (gammal.c === '_')
            gammal.stil |= TEXT_UNDER;
        gammal.c = c;
    } else if (gammal.c === c)
        gammal.stil |= TEXT_FET;
    else if (c !== ' ')
        gammal.c = c;
}

/* Är ESC-följden k med argumenten a klar? */
function text_foljd_klar(k, a) {
    const n = a.length;
    const antal = (i, j) => a[i] + 256 * a[j];
    switch (k) {
    case '-': case 'W': case 'R': case 'Q': case 'l': case 'N': case 'A': case 'J':
    case '3': case 'S': case 'U': case 'j': case 'p': case 's': case '!': case 'i':
    case 'a': case 'k': case 'x': case '/': case 'w': case 'r': case 't':
        return n >= 1;
    case 'C': return n >= 2 || (n === 1 && a[0] !== 0);
    case '$': case '\\': case '%': case 'e': case 'f': case '?': return n >= 2;
    case ':': return n >= 3;
    case 'K': case 'L': case 'Y': case 'Z': return n >= 2 && n >= 2 + antal(0, 1);
    case '*': return n >= 3 && n >= 3 + antal(1, 2);
    case '^': return n >= 3 && n >= 3 + 2 * antal(1, 2);
    case '&': return n >= 3 && n >= 3 + 12 * Math.max(0, a[2] - a[1] + 1);
    case 'D': case 'B': return (n >= 1 && a[n - 1] === 0) || n > 32;
    case 'b': return (n >= 2 && a[n - 1] === 0) || n > 17;
    default: return true;                    /* utan argument */
    }
}

/* Gör det ESC-följden k med argumenten a säger. På och av tar både 0
 * och 1 och '0' och '1'. */
function text_foljd_gor(k, a) {
    const pa = (a[0] & 1) === 1;
    const stil = (bit, satt) => text_stil = satt ? text_stil | bit : text_stil & ~bit;
    switch (k) {
    case '@': text_starta(); break;
    case 'E': case 'F': stil(TEXT_FET, k === 'E'); break;
    case 'G': case 'H': stil(TEXT_DUBBEL, k === 'G'); break;
    case '4': case '5': stil(TEXT_KURSIV, k === '4'); break;
    case '-': stil(TEXT_UNDER, pa); break;
    case 'W': text_bred = pa; break;
    case '\x0E': text_bred_raden = true; break;
    case '\x0F': text_smal = true; break;
    case 'M': case 'P': text_elite = k === 'M'; break;
    case '!':
        text_elite = (a[0] & 1) !== 0;
        text_smal = (a[0] & 4) !== 0;
        text_bred = (a[0] & 32) !== 0;
        stil(TEXT_FET, a[0] & 8);
        stil(TEXT_DUBBEL, a[0] & 16);
        stil(TEXT_KURSIV, a[0] & 64);
        stil(TEXT_UNDER, a[0] & 128);
        break;
    case 'R': if (a[0] < TEXT_LAND.length) text_land = a[0]; break;
    case 'D':
        text_tabbar = a.slice(0, -1).filter(n => n > 0).map(n => n * text_steg());
        break;
    case '$': text_x = Math.min(TEXT_BREDD, 2 * (a[0] + 256 * a[1])); break;
    case '\\': text_x = Math.max(0, Math.min(TEXT_BREDD, text_x + ((a[0] + 256 * a[1]) << 16 >> 16))); break;
    case 'K': case 'L': case 'Y': case 'Z': case '*': case '^': {
        const tathet = k === '*' || k === '^' ? a[0] : 'KLYZ'.indexOf(k);
        const prickar = k === '*' || k === '^' ? a[1] + 256 * a[2] : a[0] + 256 * a[1];
        const steg = [2, 1, 1, 0.5, 1.5, 120 / 72, 120 / 90, 120 / 144][tathet & 7];
        text_x = Math.min(TEXT_BREDD, text_x + prickar * steg);
        break;
    }
    }
}

function text_tecken(b) {
    if (text_foljd) {
        if (text_foljd.k === null)
            text_foljd.k = String.fromCharCode(b & 0x7F);
        else
            text_foljd.a.push(b);
        if (text_foljd_klar(text_foljd.k, text_foljd.a)) {
            text_foljd_gor(text_foljd.k, text_foljd.a);
            text_foljd = null;
        }
        return;
    }
    const k = b & 0x7F;
    switch (k) {
    case 0x1B: text_foljd = { k: null, a: [] }; return;
    case 0x0D: text_x = 0; return;
    case 0x0A: case 0x0B: text_radslut(); return;
    case 0x0C:
        if (text_rad.length)
            text_radslut();
        if (text_sidor[text_sidor.length - 1].length)
            text_sidor.push([]);
        text_x = 0;
        return;
    case 0x08: text_x = Math.max(0, text_x - text_steg()); return;
    case 0x09: {
        const steg = text_steg();
        const tabb = text_tabbar ? text_tabbar.find(t => t > text_x)
                   : (Math.floor(text_x / (8 * steg)) + 1) * 8 * steg;
        if (tabb !== undefined)
            text_x = Math.min(TEXT_BREDD, tabb);
        return;
    }
    case 0x0E: text_bred_raden = true; return;
    case 0x14: text_bred_raden = false; return;
    case 0x0F: text_smal = true; return;
    case 0x12: text_smal = false; return;
    case 0x18: text_rad = []; text_x = 0; return;
    }
    if (k < 0x20 || k === 0x7F)
        return;
    const plats = TEXT_PLATSER.indexOf(k);
    text_skriv(plats >= 0 ? [...TEXT_LAND[text_land]][plats] : String.fromCharCode(k));
}

/* En rad som text: varje tecken i den kolumn (om 10 per tum) där det
 * börjar, eller efter det förra, med minst ett mellanrum där det är
 * luft emellan. */
function text_rad_text(rad) {
    let s = '', slut = 0;
    for (const t of rad) {
        const kolumn = Math.max(Math.floor(t.x / 12), t.x - slut >= 6 ? s.length + 1 : 0);
        if (s.length < kolumn)
            s += ' '.repeat(kolumn - s.length);
        s += t.c;
        slut = t.x + t.w;
    }
    return s.trimEnd();
}

/* En rad som HTML: tecknen i följder med samma stil och bredd, som
 * dras ut eller trycks ihop till sin bredd (1ch är ett pica-tecken). */
function text_html(rad) {
    const ch = n => +(n / 12).toFixed(4) + 'ch';
    const skydda = c => c === '<' ? '&lt;' : c === '>' ? '&gt;' : c === '&' ? '&amp;' : c;
    let html = '', x = 0;
    for (let i = 0; i < rad.length;) {
        const forsta = rad[i];
        let j = i + 1;
        while (j < rad.length && rad[j].stil === forsta.stil && rad[j].w === forsta.w
               && rad[j].x === rad[j - 1].x + forsta.w)
            j++;
        if (forsta.x !== x)
            html += '<span style="margin-left:' + ch(forsta.x - x) + '"></span>';
        const text = rad.slice(i, j).map(t => skydda(t.c)).join('');
        const klasser = [];
        if (forsta.stil & (TEXT_FET | TEXT_DUBBEL)) klasser.push('fet');
        if (forsta.stil & TEXT_UNDER) klasser.push('under');
        if (forsta.stil & TEXT_KURSIV) klasser.push('kursiv');
        const attr = klasser.length ? ' class="' + klasser.join(' ') + '"' : '';
        if (forsta.w === 12)
            html += attr ? '<span' + attr + '>' + text + '</span>' : text;
        else
            html += '<span' + attr + ' style="display:inline-block;transform:scaleX('
                + +(forsta.w / 12).toFixed(4) + ');transform-origin:0 0;margin-right:'
                + ch((j - i) * (forsta.w - 12)) + '">' + text + '</span>';
        x = rad[j - 1].x + forsta.w;
        i = j;
    }
    return html;
}

/* Papperet som text, sidorna med sidbyte emellan. */
function text_som_text(sidbyte) {
    const sidor = text_sidor.map(sida => sida.map(rad => text_rad_text(rad.tecken)));
    if (text_rad.length)
        sidor[sidor.length - 1].push(text_rad_text(text_rad));
    return sidor.map(sida => sida.join('\n')).join(sidbyte);
}

/* Papperet som HTML, sidorna med ett streck emellan. */
function text_som_html() {
    const sidor = text_sidor.map(sida => sida.map(rad => rad.html));
    if (text_rad.length)
        sidor[sidor.length - 1].push(text_html(text_rad));
    return sidor.map(sida => sida.join('\n') + '\n').join('<hr>');
}

/* Hämta tecknen som har kommit sedan sist och skriv om texten. Är
 * papperet fullt hämtas inget, så att kortet blir upptaget och 9704
 * väntar (BREAK avbryter). */
function text_rita() {
    const antal = e.centronics_antal();
    if (antal === 0)
        return;
    if (text_antal_rader() >= TEXT_RADER) {
        if (!text_fullt) {
            text_fullt = true;
            meddela('Papperet är fullt: riv av det.', 'The paper is full: tear it off.');
        }
        return;
    }
    const ny_utskrift = performance.now() - papper_senast > PAPPER_PAUS;
    papper_senast = performance.now();
    const tecknen = new Uint8Array(e.memory.buffer, e.centronics_tecken(), antal);
    for (const k of tecknen)
        text_tecken(k);
    e.centronics_hamtat();
    papper_ritat += antal;
    papper_rader = text_antal_rader();
    $('papper_text').innerHTML = text_som_html();
    papper_nytt(ny_utskrift);
}

/* Lappen visar P40:s papper (bild) eller Centronicsskrivarens (text). */
function papper_slag() {
    const text = $('centronics').checked;
    pappret.hidden = text;
    $('papper_text').hidden = !text;
}

let anvande_ljud = false;

/* Skärmen ritas som i sdl/fonster.c, som en vit fosforskärm: punkterna
 * (320 x 240, eller 640 x 240 med TKN80 i 80-teckensläget) läggs på en
 * egen canvas, punktbild, och ritas förstorade på skarm (640 x 480).
 * Det som tänds lyser direkt, och det som släcks klingar av med
 * tidskonstanten EFTERLYS: varje gång skärmen ritas läggs svart över med
 * täckningen 1 - exp(-dt / EFTERLYS), och sedan punkterna med
 * 'lighten', så att de svarta inte täcker det som klingar av. Skenet är
 * samma bild på canvasen sken, som täcker hela ramen, suddad med CSS
 * (filter: blur, med radien räknad om till skärmens storlek) och lagd
 * ovanpå. Med rutan skarp ritas punkterna utan sken och efterlysning. */
const EFTERLYS = 0.020;                      /* efterlysningens tidskonstant, s */
const SKEN = 2;                              /* skenets standardavvikelse, av 640 punkter */
const sken = $('sken');
const sken_ritning = sken.getContext('2d');
const punktbild = document.createElement('canvas');
const punktritning = punktbild.getContext('2d');
let bilden = null;
let fosfor_ritad = 0;                        /* performance.now() när skärmen senast ritades */

/* Den senaste bilden ur WebAssembly till punktbild. */
function rita() {
    const bredd = e.kolumner() * 8;
    if (!bilden || bredd !== bilden.width) {
        punktbild.width = bredd;
        punktbild.height = 240;
        bilden = punktritning.createImageData(bredd, 240);
    }
    bilden.data.set(new Uint8Array(e.memory.buffer, e.punkter(), bredd * 240 * 4));
    punktritning.putImageData(bilden, 0, 0);
}

/* Skärmen och skenet, en gång per varv. */
function fosfor() {
    if (!bilden)
        return;
    const nu = performance.now();
    const dt = fosfor_ritad ? (nu - fosfor_ritad) / 1000 : 1;
    fosfor_ritad = nu;
    const skarp = $('skarp').checked;
    ritning.globalCompositeOperation = 'source-over';
    ritning.globalAlpha = skarp ? 1 : 1 - Math.exp(-dt / EFTERLYS);
    ritning.fillStyle = '#000';
    ritning.fillRect(0, 0, skarm.width, skarm.height);
    ritning.globalAlpha = 1;
    ritning.globalCompositeOperation = 'lighten';
    ritning.imageSmoothingEnabled = false;
    ritning.drawImage(punktbild, 0, 0, skarm.width, skarm.height);
    ritning.globalCompositeOperation = 'source-over';
    if (!skarp) {
        sken_ritning.clearRect(0, 0, sken.width, sken.height);
        sken_ritning.drawImage(skarm, skarm.offsetLeft, skarm.offsetTop,
                               skarm.clientWidth, skarm.clientHeight);
    }
}

/* Skenets canvas har ramens storlek i CSS-punkter, och radien följer
 * skärmens storlek på sidan. */
function skenets_storlek() {
    sken.width = $('ram').clientWidth;
    sken.height = $('ram').clientHeight;
    sken.style.filter = 'blur(' + (SKEN * skarm.clientWidth / 640).toFixed(2) + 'px)';
}
new ResizeObserver(skenets_storlek).observe($('ram'));

$('skarp').addEventListener('change', () => {
    $('ram').classList.toggle('skarp', $('skarp').checked);
    visa_lagen();
    skarm.focus();
});

function varv() {
    if (v24_text !== null && e.tangenter_klara()) {
        skriv(v24_text, true, true);         /* som batchläget: 100 ms efter */
        e.v24_om(100);
        v24_text = null;
    }
    spola_vidare();
    papper_rita();
    if (play_vid_relaet && e.motor()) {
        play_vid_relaet = false;
        knappar(true);
    }
    if (performance.now() - cmos_sparat > 1000) {
        cmos_spara();
        cmos_sparat = performance.now();
    }
    const nu = klockan();
    const med_ljud = ljudet_gar();
    if (med_ljud !== anvande_ljud)
        visa_ljudet();
    if (nasta === null || med_ljud !== anvande_ljud || nu - nasta > 0.25 || nasta - nu > 0.5)
        nasta = nu + (med_ljud ? 0.05 : 0);
    anvande_ljud = med_ljud;
    /* Markören blinkar, utom på TKN80 med byglingen för fast markör. */
    const markor = $('tkn80').checked && $('fast').checked ? 1
        : Math.floor(performance.now() / 330) % 2 === 0 ? 1 : 0;

    if ($('snabbt').checked && e.motor()) {
        /* Kassetten: så många bilder som hinns, utan ljud. */
        const slut = performance.now() + SNABBT_MS;
        let antal = 0;
        while (performance.now() < slut && antal < 500 && e.motor()) {
            e.bild(0, markor);
            antal++;
        }
        e.bild(1, markor);
        rita();
        nasta = null;
    } else {
        let ritad = false;
        while (nasta < nu + (med_ljud ? LJUD_FORE : 0)) {
            const sista = nasta + BILD >= nu + (med_ljud ? LJUD_FORE : 0);
            const fran = bandljudet && e.bandet_gar() && !spolning ? e.bandlage() : null;
            const antal = e.bild(sista ? 1 : 0, markor);
            if (med_ljud && antal > 0)
                spela(antal, nasta, fran);
            nasta += BILD;
            ritad = ritad || sista;
        }
        if (ritad)
            rita();
    }
    fosfor();
    visa_bandet();
    visa_lagen();
    requestAnimationFrame(varv);
}

/* ---------------------------------------------------------------- */
/* Kassetten                                                         */

function sekunder(s) {
    const m = Math.floor(s / 60);
    return m + ':' + String(Math.floor(s % 60)).padStart(2, '0');
}

let spelar_in = false, har_band = false;

function visa_bandet() {
    let text;
    if (b2_valt())
        text = t('ingen kassett i BASIC II', 'no cassette in BASIC II');
    else if (!har_band && !spelar_in)
        text = t('inget band', 'no tape');
    else if (spelar_in)
        text = t('spelar in ', 'recording ') + sekunder(e.bandlage());
    else
        text = sekunder(e.bandlage()) + ' / ' + sekunder(e.bandlangd());
    if (e.motor())
        text += t(' · motorn går', ' · motor on');
    else if (e.bandet_gar())
        text += t(' · bandet går', ' · tape running');
    const status = $('bandstatus');
    if (status.textContent !== text)
        status.textContent = text;
    const steg = har_band ? Math.round(spolens_varv(e.bandlage()) - rakneverk_noll) : 0;
    const visas = String((steg % 1000 + 1000) % 1000).padStart(3, '0');
    if ($('rakneverk').textContent !== visas)
        $('rakneverk').textContent = visas;
}

/* Spolningen: -1 bakåt, 1 framåt, 0 står. En C60-sida (30 min) spolas
 * på en och en halv minut. */
const SPOLFART = 20;
let spolning = 0, spolat = 0, rakneverk_noll = 0;

/* Varv på upptagningsspolen efter s sekunder: navet 11 mm, bandet
 * 16 µm tjockt och 47,6 mm/s; räkneverket går 3 steg på 4 varv. En
 * C60-sida blir knappt 600. */
function spolens_varv(s) {
    const nav = 11, tjocklek = 0.016, fart = 47.6;
    return 0.75 * (Math.sqrt(nav * nav + fart * tjocklek * s / Math.PI) - nav) / tjocklek;
}

function spola_vidare() {
    if (!spolning)
        return;
    const nu = performance.now();
    const langd = e.bandlangd();
    const lage = Math.min(e.bandlage(), langd) + spolning * SPOLFART * (nu - spolat) / 1000;
    spolat = nu;
    e.spola(lage);                           /* stannar vid 0 och längden */
    if (spolning < 0 ? lage <= 0 : lage >= langd)
        stanna_spolningen();
}

function stanna_spolningen() {
    spolning = 0;
    $('bakat').classList.remove('pa');
    $('framat').classList.remove('pa');
}

function spola_at(riktning) {
    if (spelar_in || !har_band) {
        if (spelar_in)
            meddela('Under inspelning går det inte att spola.', 'The tape cannot be wound while recording.');
        else
            meddela('Inget band i kassetten.', 'No tape in the cassette.');
        return;
    }
    const igen = spolning === riktning;
    stanna_spolningen();
    if (!igen) {
        knappar(false);                      /* PLAY släpps upp */
        play_vid_relaet = false;
        spolning = riktning;
        spolat = performance.now();
        $(riktning < 0 ? 'bakat' : 'framat').classList.add('pa');
    }
    skarm.focus();
}

$('bakat').addEventListener('click', () => spola_at(-1));
$('framat').addEventListener('click', () => spola_at(1));
$('rakneverk').addEventListener('click', () => {
    rakneverk_noll = har_band ? spolens_varv(e.bandlage()) : 0;
    skarm.focus();
});

function lagg_i_band(data, namn) {
    const p = lagg_i_minnet(data);
    const flanker = e.band(p, data.length);
    e.slapp(p);
    if (flanker < 0) {
        meddela(namn + ': ingen WAV-fil med PCM, 8 eller 16 bitar',
                namn + ': not a WAV file with PCM, 8 or 16 bits');
        return;
    }
    oppna_panel('panel_band');
    har_band = true;
    spelar_in = false;
    stanna_spolningen();
    rakneverk_noll = 0;
    knappar(true);
    $('spelain').classList.remove('pa');
    meddela(namn + ' ligger i kassetten (' + flanker + ' flanker). Skriv LOAD CAS: eller RUN CAS:',
            namn + ' is in the cassette (' + flanker + ' edges). Type LOAD CAS: or RUN CAS:');
    bandljudet = avkoda_bandet(data);
}

/* Bandets ljud ur WAV-filen (PCM, 8 eller 16 bitar, första kanalen),
 * omräknat till provtakten; null om filen inte går att läsa. */
function avkoda_bandet(data) {
    const vy = new DataView(data.buffer, data.byteOffset, data.length);
    const text = (p) => String.fromCharCode(data[p], data[p + 1], data[p + 2], data[p + 3]);
    if (data.length < 12 || text(0) !== 'RIFF' || text(8) !== 'WAVE')
        return null;
    let kanaler = 0, takt = 0, bitar = 0, start = 0, langd = 0;
    for (let p = 12; p + 8 <= data.length; ) {
        const storlek = vy.getUint32(p + 4, true);
        if (text(p) === 'fmt ' && p + 24 <= data.length) {
            kanaler = vy.getUint16(p + 10, true);
            takt = vy.getUint32(p + 12, true);
            bitar = vy.getUint16(p + 22, true);
        } else if (text(p) === 'data') {
            start = p + 8;
            langd = Math.min(storlek, data.length - start);
            break;
        }
        p += 8 + storlek + (storlek & 1);
    }
    if (!start || !kanaler || !takt || (bitar !== 8 && bitar !== 16))
        return null;
    const steg = kanaler * bitar / 8;
    const antal = Math.floor(langd / steg);
    const prov = (k) => bitar === 8 ? (data[start + k * steg] - 128) / 128
                                    : vy.getInt16(start + k * steg, true) / 32768;
    const ut = new Float32Array(Math.floor(antal * provtakt / takt));
    for (let k = 0; k < ut.length; k++) {
        const x = k * takt / provtakt, i = Math.floor(x), f = x - i;
        ut[k] = i + 1 < antal ? prov(i) * (1 - f) + prov(i + 1) * f : prov(i);
    }
    return ut;
}

/* Bandspelarens knappar: PLAY (▶, nere eller uppe) och fjärrstyrd.
 * play_vid_relaet: PLAY trycks ned när motorreläet drar (Genesis-demot,
 * utan fjärrstyrning, så att bandet inte går före RUN CAS:). */
let play_nere = false, play_vid_relaet = false;

function knappar(play = play_nere) {
    play_nere = play;
    $('play').classList.toggle('pa', play_nere);
    e.knappar(play_nere ? 1 : 0, $('fjarr').checked ? 1 : 0);
}

$('play').addEventListener('click', () => {
    play_vid_relaet = false;
    if (!play_nere)
        stanna_spolningen();
    knappar(!play_nere);
    skarm.focus();
});
$('fjarr').addEventListener('change', () => { knappar(); skarm.focus(); });

$('spelain').addEventListener('click', () => {
    if (!e.spela_in()) {
        meddela('minnet räcker inte till inspelningen', 'not enough memory for the recording');
        return;
    }
    spelar_in = true;
    har_band = false;
    stanna_spolningen();
    knappar(true);
    bandljudet = null;
    $('spelain').classList.add('pa');
    meddela('Inspelningsknappen är nere. Skriv SAVE CAS:NAMN och tryck sedan Spara inspelning.',
            'The record button is down. Type SAVE CAS:NAME and then press Save recording.');
    skarm.focus();
});

$('spara').addEventListener('click', () => {
    if (!spelar_in || !e.inspelade_flanker()) {
        meddela('Inget har spelats in än: tryck Spela in och skriv SAVE CAS:NAMN.',
                'Nothing has been recorded yet: press Record and type SAVE CAS:NAME.');
        return;
    }
    const p = e.inspelning();
    if (!p) {
        meddela('minnet räcker inte till WAV-filen', 'not enough memory for the WAV file');
        return;
    }
    const data = new Uint8Array(e.memory.buffer, p, e.inspelning_storlek()).slice();
    const lank = document.createElement('a');
    lank.href = URL.createObjectURL(new Blob([data], { type: 'audio/wav' }));
    lank.download = 'inspelning.wav';
    lank.click();
    setTimeout(() => URL.revokeObjectURL(lank.href), 10000);
    skarm.focus();
});

/* ---------------------------------------------------------------- */
/* Disketten                                                         */

/* ABC80:s tecken i ett filnamn: Ä Ö Å Ü på [ \ ] ^ och ¤ på $. */
const ABC_TECKEN = { 0x5B: 'Ä', 0x5C: 'Ö', 0x5D: 'Å', 0x5E: 'Ü', 0x24: '¤' };

function filnamn(d, k) {
    let namn = '';
    for (let i = 0; i < 11; i++) {
        const c = d[k + i];
        if (i === 8)
            namn = namn.trimEnd() + '.';
        namn += ABC_TECKEN[c] || (c >= 0x20 && c < 0x7F ? String.fromCharCode(c) : '?');
    }
    return namn.trimEnd().replace(/\.$/, '');
}

/* Katalogen som text. En post är 16 byte: +0/+1 indexsektorns adress
 * (sektorn * 32, skyddsbitarna i bit 0-1), +4-+14 namnet, och $FF i +0
 * är en ledig post; den första posten i varje sektor är tom. Storleken
 * står ingenstans, så sektorerna räknas i indexsektorns sträckor (2 byte
 * var efter huvudet och $FF, längden - 1 i de låga 5 bitarna, slut med
 * $FF $FF). Lediga sektorer är nollor i bitkartan i sektor 6. */
function katalogen(d) {
    const sektorer = d.length >> 8;
    const rader = [];
    for (let s = 16; s < 24; s++)
        for (let k = s * 256 + 16; k < (s + 1) * 256; k += 16) {
            if (d[k] === 0xFF || (d[k] === 0 && d[k + 1] === 0))
                continue;
            const index = ((d[k] << 8 | d[k + 1]) >> 5) * 256;
            let antal = 0;
            if (index + 256 <= d.length && d[index + 3] === 0xFF)
                for (let i = index + 4; i + 1 < index + 256; i += 2) {
                    if (d[i] === 0xFF && d[i + 1] === 0xFF)
                        break;
                    antal += (d[i + 1] & 31) + 1;
                }
            rader.push(filnamn(d, k + 4).padEnd(14) +
                       String(antal).padStart(4) + (antal === 1 ? t(' sektor', ' sector')
                                                                 : t(' sektorer', ' sectors')));
        }
    let lediga = 0;
    for (let n = 0; n < sektorer; n++)
        if (!(d[6 * 256 + (n >> 3)] & 0x80 >> (n & 7)))
            lediga++;
    rader.push(filer_summa(rader.length) + lediga +
               t(' sektorer lediga (', ' sectors free (') + Math.floor(lediga / 4) + ' KB).');
    return rader.join('\n');
}

/* "Inga filer. 0 filer, " och så vidare, före det lediga. */
function filer_summa(n) {
    return (n ? '' : t('Inga filer. ', 'No files. ')) + n +
        (n === 1 ? t(' fil, ', ' file, ') : t(' filer, ', ' files, '));
}

/* Katalogen på en CP/M-skiva som vår BIOS ordnar den (cpm/bios.asm):
 * 64 poster om 32 bytes i början av spår 4. +0 är användaren ($E5 en
 * ledig post), +1-+11 namnet och typen (bit 7 är attribut), +16-+31
 * blocken om 1 KB, ett per byte. En fil med flera poster (över 16 KB)
 * räknas ihop; block 0 och 1 är katalogen, så 70 block blir över. */
/* FD2: katalogen i spår 4, 64 poster, block om 1 KB med numren i en
 * byte. Kort 44 (kort44): katalogen först, 128 poster, block om 2 KB
 * med numren i två bytes. */
function cpm_katalogen(d, kort44 = false) {
    const filer = new Map();
    let anvanda = 0;
    const forst = kort44 ? 0 : 4 * 16 * 128;
    const kb = kort44 ? 2 : 1;
    for (let k = forst; k < forst + (kort44 ? 128 : 64) * 32; k += 32) {
        if (d[k] > 15)
            continue;
        const del = (a, b) => String.fromCharCode(...[...d.subarray(k + a, k + b)]
                                                  .map(c => c & 0x7F)).trimEnd();
        const typ = del(9, 12);
        const namn = (d[k] ? d[k] + '/' : '') + del(1, 9) + (typ ? '.' + typ : '');
        let block = 0;
        for (let i = 16; i < 32; i += kb)
            if (d[k + i] || (kort44 && d[k + i + 1]))
                block += kb;
        filer.set(namn, (filer.get(namn) || 0) + block);
        anvanda += block;
    }
    const rader = [...filer].map(([namn, kb]) => namn.padEnd(14) + String(kb).padStart(3) + 'k');
    rader.push(filer_summa(rader.length) + ((kort44 ? 636 : 70) - anvanda) + t('k lediga.', 'k free.'));
    return rader.join('\n');
}

const INGEN_SKIVA = ['Ingen diskett i FD2: lägg i en med Lägg i diskett… eller Exempeldisketten.',
                     'No disk in the FD2: insert one with Insert disk… or Example disk.'];

/* Katalogen i panelen; false utan diskett. */
function visa_katalogen() {
    const d = skivan_nu();
    const ruta = $('katalog');
    if (!d) {
        ruta.hidden = true;
        return false;
    }
    const i = t(' i ', ' in ');
    if (cpm_valt())
        ruta.textContent = skivans_namn + i + 'A:\n' + cpm_katalogen(d) + '\n\n' +
                           skiva_b_namn + i + 'B:\n' + cpm_katalogen(skiva_b_nu()) + '\n\n' +
                           skiva_d_namn + i + 'D:\n' + cpm_katalogen(skiva_d_nu(), true);
    else
        ruta.textContent = skivans_namn + i + 'DR0:\n' + katalogen(d);
    ruta.hidden = false;
    return true;
}

$('katalog_knapp').addEventListener('click', () => {
    if (!visa_katalogen())
        meddela(...INGEN_SKIVA);
    skarm.focus();
});

function ladda_ner(d, namn) {
    const lank = document.createElement('a');
    lank.href = URL.createObjectURL(new Blob([d], { type: 'application/octet-stream' }));
    lank.download = namn;
    lank.click();
    setTimeout(() => URL.revokeObjectURL(lank.href), 10000);
}

/* Med CP/M laddas A: ner, B: också om något står på den och D: om den
 * har ändrats (ett sparat spel). */
$('spara_skiva').addEventListener('click', () => {
    const d = skivan_nu();
    if (!d) {
        meddela(...INGEN_SKIVA);
        return;
    }
    ladda_ner(d, skivans_namn);
    const b = skiva_b_nu();
    const dd = skiva_d_nu();
    const fler = [];
    if (b && b.some(x => x !== 0xE5))
        fler.push([b, skiva_b_namn, 'B:']);
    if (dd && (dd.length !== ROM.cpm_d.length || dd.some((x, k) => x !== ROM.cpm_d[k])))
        fler.push([dd, skiva_d_namn, 'D:']);
    fler.forEach(([data, namn], k) => setTimeout(() => ladda_ner(data, namn), 500 * (k + 1)));
    if (fler.length) {
        const lista = [[d, skivans_namn, 'A:'], ...fler].map(([, namn, enhet]) => namn + ' (' + enhet + ')');
        const och = (o) => lista.slice(0, -1).join(', ') + o + lista[lista.length - 1];
        meddela(och(' och ') + ' laddas ner med det som har sparats på dem.',
                och(' and ') + ' are downloaded with what has been saved on them.');
    } else
        meddela(skivans_namn + ' laddas ner med det som har sparats på den.',
                skivans_namn + ' is downloaded with what has been saved on it.');
    skarm.focus();
});

/* ---------------------------------------------------------------- */
/* Filerna                                                           */

let filval_for = null;

function valj_fil(vad, typer) {
    filval_for = vad;
    $('filval').accept = typer;
    $('filval').value = '';
    $('filval').click();
}

/* En fil: .wav till kassetten, .dsk till disketten, annat skrivs in. */
async function oppna(fil, vad) {
    const data = new Uint8Array(await fil.arrayBuffer());
    const namn = fil.name;
    if (!vad)
        vad = /\.wav$/i.test(namn) ? 'band' : /\.(dsk|img)$/i.test(namn) ? 'skiva' : 'program';
    if (vad === 'band' && b2_valt())
        meddela('BASIC II har ingen kassett (CAS: ger fel 21).',
                'BASIC II has no cassette (CAS: gives error 21).');
    else if (vad === 'band')
        lagg_i_band(data, namn);
    else if (vad === 'cmos') {
        cmos_byt(data);
        meddela(namn + ' ligger i CMOS-minnet' + ($('cmos').checked ? '.' : ' (slå på det).'),
                namn + ' is in the CMOS memory' + ($('cmos').checked ? '.' : ' (switch it on).'));
    }
    else if (vad === 'skiva' && data.length === KORT44_SKIVA && $('cpm')) {
        if (!cpm_valt()) {
            byt_lage(true);
            skiva_b_namn = 'b.dsk';
            starta(null, null, data);
        } else
            starta(skivan_nu(), skiva_b_nu(), data);
        skiva_d_namn = namn;
        meddela(namn + ' sitter i D: på kort 44, och CP/M startar om. DIR D: visar filerna.',
                namn + ' sits in D: on card 44, and CP/M restarts. DIR D: shows the files.');
    } else if (vad === 'skiva' && ar_cpm(data) && $('cpm')) {
        if (!cpm_valt()) {
            byt_lage(true);
            skiva_b_namn = 'b.dsk';
            starta(data, null, null);
        } else
            starta(data);
        skivans_namn = namn;
        meddela(namn + ' är en CP/M-systemskiva och sitter i A:. CP/M startar från den.',
                namn + ' is a CP/M system disk and sits in A:. CP/M starts from it.');
    } else if (vad === 'skiva' && cpm_valt()) {
        skiva_b_namn = namn;
        starta(skivan_nu(), data);
        meddela(namn + ' sitter i B:, och CP/M startar om. DIR B: visar filerna.',
                namn + ' sits in B:, and CP/M restarts. DIR B: shows the files.');
    } else if (vad === 'skiva') {
        skivans_namn = namn;
        starta(data);
        meddela(namn + ' sitter i FD2 med ABC-DOS. Katalogen visar filerna; skriv RUN DR0:NAMN.',
                namn + ' sits in the FD2 with ABC-DOS. Directory shows the files; type RUN DR0:NAME.');
    } else {
        skriv(new TextDecoder('utf-8').decode(data));
        meddela(namn + ' skrivs in.', namn + ' is being typed in.');
    }
    if (vad === 'skiva')
        oppna_panel('panel_disk');
    skarm.focus();
}

$('filval').addEventListener('change', () => {
    const fil = $('filval').files[0];
    if (fil)
        oppna(fil, filval_for);
});
$('program').addEventListener('click', () => valj_fil('program', '.bas,.txt,text/plain'));
$('skiva').addEventListener('click', () => valj_fil('skiva', '.dsk,.img'));
$('band').addEventListener('click', () => valj_fil('band', '.wav,audio/wav'));

document.addEventListener('dragover', (h) => {
    h.preventDefault();
    document.body.classList.add('slapp');
});
document.addEventListener('dragleave', () => document.body.classList.remove('slapp'));
document.addEventListener('drop', (h) => {
    h.preventDefault();
    document.body.classList.remove('slapp');
    const fil = h.dataTransfer.files[0];
    if (fil)
        oppna(fil);
});

/* ---------------------------------------------------------------- */
/* Knapparna                                                         */

$('aterstall').addEventListener('click', () => { e.aterstall(); skarm.focus(); });
$('nystart').addEventListener('click', () => { starta(null, null, null); meddela(); skarm.focus(); });
$('rom').addEventListener('change', () => {
    /* Sitter en exempeldiskett i byts den mot den som hör till tolken; den
     * som tas ur ligger undan med det som har sparats på den. */
    const b2 = b2_valt() && !!FILER.program_b2;
    const ur = b2 ? 'exempel.dsk' : 'exempel_b2.dsk';
    if (skivans_namn === ur && FILER.program_b2) {
        exempel_undan[ur] = skivan_nu();
        skivans_namn = b2 ? 'exempel_b2.dsk' : 'exempel.dsk';
        starta(exempel_undan[skivans_namn] || exempel_skivan(b2));
    } else
        starta();
    visa_lagen();
    if (b2_valt())
        meddela('BASIC II: FD2 är MO0:, till exempel SAVE "MO0:PROV". PR: skriver till Centronics.',
                'BASIC II: the FD2 is MO0:, for example SAVE "MO0:PROV". PR: prints to the Centronics.');
    else
        meddela();
    skarm.focus();
});
$('tkn80').addEventListener('change', () => {
    starta();
    if ($('tkn80').checked)
        meddela('TKN80: 80 tecken per rad. PRINT INP(3);CHR$(12) ger 40, PRINT INP(4);CHR$(12) 80.',
                'TKN80: 80 characters per line. PRINT INP(3);CHR$(12) gives 40, PRINT INP(4);CHR$(12) 80.');
    else
        meddela();
    skarm.focus();
});
$('fast').addEventListener('change', () => skarm.focus());
/* Skivorna i det läge som inte är valt (ABC80 med ABC-DOS eller CP/M)
 * ligger undan här: {a, b, d, namn, namn_b, namn_d}. */
const undan = { abc80: null, cpm: null };

/* Byt till CP/M (cpm) eller ABC80 och ge skivorna som låg undan för
 * det läget, eller null; starta() efteråt. */
function byt_lage(cpm) {
    undan[cpm ? 'abc80' : 'cpm'] = { a: skivan_nu(), b: skiva_b_nu(), d: skiva_d_nu(),
                                     namn: skivans_namn, namn_b: skiva_b_namn,
                                     namn_d: skiva_d_namn };
    const s = undan[cpm ? 'cpm' : 'abc80'];
    undan[cpm ? 'cpm' : 'abc80'] = null;
    $('cpm').checked = cpm;
    skivans_minne = skiva_b_minne = skiva_d_minne = 0;   /* starta() får skivorna som argument */
    skivan = skiva_b = skiva_d = null;
    return s;
}

/* Program för ABC80 (exempeldisketten, MÅLARE, demot) slår av CP/M. */
function abc80_lage() {
    if (cpm_valt())
        byt_lage(false);
}

const CPM_TEXT = ['CP/M 2.2 med UNI-80. BBC BASIC: skriv först B: och sedan BBCBASIC ' +
                  '(CP/M letar bara på skivan i prompten); *BYE går tillbaka. ' +
                  'A: systemskivan med DIR, STAT, PIP, ED och ASM, C: ABC80:s RAM (15k), ' +
                  'D: Zork I på kort 44 (ZTOLK D:ZORK1). ' +
                  '^C eller RESET startar om CP/M.',
                  'CP/M 2.2 with UNI-80. BBC BASIC: first type B: and then BBCBASIC ' +
                  '(CP/M only looks on the disk in the prompt); *BYE returns. ' +
                  'A: the system disk with DIR, STAT, PIP, ED and ASM, C: the ABC80\'s RAM (15k), ' +
                  'D: Zork I on card 44 (ZTOLK D:ZORK1). ' +
                  '^C or RESET restarts CP/M.'];

/* Byt till CP/M (cpm) eller ABC80 och starta med skivorna som låg undan. */
function lage_med_skivorna(cpm) {
    const s = byt_lage(cpm) || { a: null, b: null, d: null, namn: 'diskett.dsk',
                                 namn_b: 'b.dsk', namn_d: 'd.dsk' };
    skivans_namn = s.namn;
    skiva_b_namn = s.namn_b;
    skiva_d_namn = s.namn_d;
    starta(s.a, s.b, s.d);
}

$('cpm')?.addEventListener('change', () => {
    const cpm = $('cpm').checked;
    lage_med_skivorna(cpm);
    if (cpm)
        meddela(...CPM_TEXT);
    else
        meddela('ABC80 utan CP/M; CP/M-skivorna ligger undan till nästa gång.',
                'ABC80 without CP/M; the CP/M disks are put aside until next time.');
    skarm.focus();
});

/* Zork I: CP/M med skivorna (D: har spelet) och ZTOLK D:ZORK1. */
const ZORK_TEXT = ['Zork I (Infocom, MIT-licensen) med vår tolk ZTOLK från D: på kort 44. ' +
                   'save och restore använder D:ZORK1.SAV; quit går tillbaka till CP/M.',
                   'Zork I (Infocom, MIT licence) with our interpreter ZTOLK from D: on card 44. ' +
                   'save and restore use D:ZORK1.SAV; quit returns to CP/M.'];

function zork() {
    if (cpm_valt())
        starta();
    else
        lage_med_skivorna(true);
    e.paus(1500);
    skriv('ZTOLK D:ZORK1\n');
    meddela(...ZORK_TEXT);
}

$('zork')?.addEventListener('click', () => {
    zork();
    skarm.focus();
});

/* Exempeldisketten (exempel/program/), när den ligger i sidan. Med
 * BASIC II är det dess egen (program_b2.dsk), med programmen skrivna om
 * för ABC 800-tolken och ABC 800:s CMDINT.SYS, så att BYE går. */
const exempel_undan = {};

function exempel_skivan(b2) {
    return (b2 ? FILER.program_b2 : FILER.program).slice();
}

$('exempel')?.addEventListener('click', () => {
    abc80_lage();
    const b2 = b2_valt() && !!FILER.program_b2;
    skivans_namn = b2 ? 'exempel_b2.dsk' : 'exempel.dsk';
    exempel_undan[skivans_namn] = null;
    starta(exempel_skivan(b2));
    if (b2)
        meddela('Exempeldisketten för BASIC II: RUN "MO0:GISSA", KURVA, MUSIK, CHIP8 (Mini Lights Out) eller SNAKE3. ' +
                'BYE ger DOS:en (LIB), $BAS går tillbaka. Ctrl-C två gånger stoppar.',
                'The BASIC II example disk: RUN "MO0:GISSA", KURVA, MUSIK, CHIP8 (Mini Lights Out) or SNAKE3. ' +
                'BYE gives the DOS (LIB), $BAS returns. Ctrl-C twice stops.');
    else
        meddela('Exempeldisketten: RUN DR0:GISSA, RUN DR0:KURVA, RUN DR0:MUSIK eller RUN DR0:AIRFIGHT.',
                'The example disk: RUN DR0:GISSA, RUN DR0:KURVA, RUN DR0:MUSIK or RUN DR0:AIRFIGHT.');
    oppna_panel('panel_disk');
    skarm.focus();
});

/* Luxors systemskiva ver. 1.0 och demoskiva (skivor/hamta.py), när de
 * ligger i sidan. */
function systemskivan() {
    abc80_lage();
    skivans_namn = 'systemskiva.dsk';
    starta(FILER.systemskiva.slice());
    meddela('Systemskivan: RUN DR0:LIB visar katalogen, BYE ger kommandona (LIB, MAP, SPACE, COPY …).',
            'The system disk: RUN DR0:LIB shows the directory, BYE gives the commands (LIB, MAP, SPACE, COPY …).');
    oppna_panel('panel_disk');
}

function demoskivan() {
    abc80_lage();
    skivans_namn = 'demoskiva.dsk';
    starta(FILER.demoskiva.slice());
    meddela('Demoskivan: RUN DR0:START (svara N på frågan om klockan), eller RUN DR0:MENY.',
            'The demo disk: RUN DR0:START (answer N to the clock question), or RUN DR0:MENY.');
    oppna_panel('panel_disk');
}

$('systemskiva')?.addEventListener('click', () => {
    systemskivan();
    skarm.focus();
});

$('demoskiva')?.addEventListener('click', () => {
    demoskivan();
    skarm.focus();
});

/* Forth-kretsarna (exempel/krets/), Forthaid och Forthaid II (utanför
 * boken, utanfor/forthaid/ och utanfor/forthaid2/), när de ligger i
 * sidan: maskinen startas om med den valda kretsen eller utan. */
const KRETSAR = {
    '': ['', ''],
    forth: ['Forth-kretsen på $4000: POKE 65052,0,208, NEW och Z=CALL(16384,3).',
            'The Forth ROM at $4000: POKE 65052,0,208, NEW and Z=CALL(16384,3).'],
    cmos: ['Forth med CMOS: Z=CALL(16384,3); orden sparas i CMOS-minnet på $5000.',
           'Forth with CMOS: Z=CALL(16384,3); the words are saved in the CMOS memory at $5000.'],
    forthaid: ['Forthaid på $4000: Z=CALL(16384,2) kopplar in den, HELP visar kommandona, EX kopplar ur.',
               'Forthaid at $4000: Z=CALL(16384,2) hooks it in, HELP shows the commands, EX unhooks it.'],
    forthaid2: ['Forthaid II på $4000 och fem banker på $5000, också med TKN80: Z=CALL(16384,2) kopplar in den, HELP visar kommandona, EX kopplar ur. SAVEW och LOADW sparar och laddar de egna orden på en diskett.',
                'Forthaid II at $4000 and five banks at $5000, also with TKN80: Z=CALL(16384,2) hooks it in, HELP shows the commands, EX unhooks it. SAVEW and LOADW save and load your own words on a diskette.'],
};

/* Bokens exempel 1-6 (band 2, kapitlet Egna exempel; exempel/krets/),
 * när de ligger i sidan: filerna i FILER.exempel, på sina adresser som
 * -l och -b i bok/band2/exempel/eN.test. Med två filer är det banker. */
const EXEMPEL = {
    e1: { filer: ['e1'], adress: 0x4000,
          text: ['Exempel 1 på $4000: Z=CALL(16384) räknar tangenterna i hörnet, Z=CALL(16387) slutar.',
                 'Example 1 at $4000: Z=CALL(16384) counts the keys in the corner, Z=CALL(16387) stops.'] },
    e2: { filer: ['e2'], adress: 0x4000,
          text: ['Exempel 2 på $4000: Z=CALL(16384) gör gemener till versaler i direktläge, Z=CALL(16387) slutar.',
                 'Example 2 at $4000: Z=CALL(16384) turns lower case into capitals in direct mode, Z=CALL(16387) stops.'] },
    e3: { filer: ['e3'], adress: 0x4000,
          text: ['Exempel 3 på $4000: Z=CALL(16384), och CTRL-L i direktläge blir LIST.',
                 'Example 3 at $4000: Z=CALL(16384), and CTRL-L in direct mode becomes LIST.'] },
    e4: { filer: ['e4a', 'e4b'], adress: 0x4000,
          text: ['Exempel 4, två banker på $4000: Z=CALL(16384) anropar bank 1 från bank 0.',
                 'Example 4, two banks at $4000: Z=CALL(16384) calls bank 1 from bank 0.'] },
    e5: { filer: ['e5'], adress: 0x7000,
          text: ['Exempel 5 på $7000, IEC-kretsens plats: CMD "TEXT" skriver en statusrad.',
                 'Example 5 at $7000, the place of the IEC ROM: CMD "TEXT" writes a status line.'] },
    e6: { filer: ['e6'], adress: 0x4000,
          text: ['Exempel 6 på $4000: inkopplat (Z=CALL(16384)) ger tangenterna inget avbrott, så kör det i ett program som kopplar ur själv (Z=CALL(16387)); A=CALL(16390) ger tecknet från V24, -1 om inget. Fast: RESET.',
                 'Example 6 at $4000: hooked in (Z=CALL(16384)) the keys give no interrupt, so run it in a program that unhooks itself (Z=CALL(16387)); A=CALL(16390) gives the character from V24, -1 if none. Stuck: RESET.'] },
};
let v24_text = null;                         /* ?s=, sänds när tangenterna är skrivna */

$('krets')?.addEventListener('change', () => {
    starta();
    meddela(...(KRETSAR[$('krets').value] ?? EXEMPEL[$('krets').value].text));
    skarm.focus();
});

/* CMOS-minnet: på eller av startar om maskinen (som att sätta i kortet). */
$('cmos').addEventListener('change', () => {
    if (!$('cmos').checked && $('krets')?.value === 'cmos')
        $('krets').value = '';
    starta();
    if ($('cmos').checked)
        meddela('CMOS-minnet på $5000-$57FF (20480-22527), sparat i webbläsaren.',
                'The CMOS memory at $5000-$57FF (20480-22527), saved in the browser.');
    else
        meddela('CMOS-minnet är av; det som står där finns kvar till nästa gång.',
                'The CMOS memory is off; what is in it remains until next time.');
    skarm.focus();
});
$('cmos_tom').addEventListener('click', () => {
    cmos_byt(new Uint8Array(0));
    meddela('CMOS-minnet är tomt.', 'The CMOS memory is empty.');
    skarm.focus();
});
$('cmos_spara').addEventListener('click', () => {
    cmos_spara();
    const lank = document.createElement('a');
    lank.href = URL.createObjectURL(new Blob([cmos], { type: 'application/octet-stream' }));
    lank.download = 'cmos.bin';
    lank.click();
    setTimeout(() => URL.revokeObjectURL(lank.href), 10000);
    meddela('cmos.bin laddas ner (som -M cmos.bin i terminalen).',
            'cmos.bin is downloaded (like -M cmos.bin in the terminal).');
    skarm.focus();
});
/* Skrivaren kopplas in och ur medan maskinen går: programmet ligger
 * kvar, och BASIC hittar drivrutinen när PR: öppnas. */
for (const [namn, andra] of [['p40', 'centronics'], ['centronics', 'p40']])
    $(namn).addEventListener('change', () => {
        const byte = $(namn).checked && $(andra).checked;
        if (byte || $(namn).checked)
            papper_tomt();                   /* ett nytt papper för den nya */
        if (byte)
            $(andra).checked = false;        /* båda på kort 60 */
        papper_slag();
        skrivaren_koppla();
        if ($('p40').checked)
            meddela('P40 på kort 60: 10 OPEN "PR:" AS FILE 1, 20 PRINT #1,"HEJ", 30 CLOSE 1, eller LIST PR:.',
                    'P40 on card 60: 10 OPEN "PR:" AS FILE 1, 20 PRINT #1,"HELLO", 30 CLOSE 1, or LIST PR:.');
        else if ($('centronics').checked)
            meddela('Centronics på kort 60 med 9704: LIST PR:, eller OPEN "PR:CSA36C70" AS FILE 1.',
                    'Centronics on card 60 with 9704: LIST PR:, or OPEN "PR:CSA36C70" AS FILE 1.');
        else
            meddela('Skrivaren är urkopplad; papperet ligger kvar.',
                    'The printer is disconnected; the paper stays.');
        skarm.focus();
    });
$('papper_riv').addEventListener('click', () => {
    if ($('p40').checked)
        e.p40_riv();
    papper_tomt();
    meddela('Papperet är avrivet.', 'The paper is torn off.');
    skarm.focus();
});
if (!med_popover)                            /* annars popovertarget */
    $('papper_knapp').addEventListener('click', () => papper_visa(!papper_oppet()));
$('papper_stang').addEventListener('click', () => {
    papper_visa(false);
    skarm.focus();
});
/* Kom igång visas av sig själv första gången (sparas i webbläsaren). */
const HJALP_NYCKEL = 'abc80_kom_igang';
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

/* Papperet flyttas genom att man drar i listen (inte i knapparna). Var
 * det står sparas i webbläsaren; det hålls inne på sidan. */
const PAPPER_NYCKEL = 'abc80_papper';
let papper_drag = null;                      /* {dx, dy} medan det dras */

function papper_flytta(x, y) {
    const r = papperet.getBoundingClientRect();
    x = Math.max(0, Math.min(x, innerWidth - r.width));
    y = Math.max(0, Math.min(y, innerHeight - 40));
    papperet.style.left = x + 'px';
    papperet.style.top = y + 'px';
    papperet.style.right = 'auto';
    return [x, y];
}

function papper_lage() {
    try {
        const l = JSON.parse(localStorage.getItem(PAPPER_NYCKEL));
        if (l && isFinite(l.x) && isFinite(l.y))
            papper_flytta(l.x, l.y);
    } catch (fel) { /* utan localStorage: uppe till höger */ }
}

const listen = papperet.querySelector('header');
listen.addEventListener('pointerdown', h => {
    if (h.target.closest('button') || h.button !== 0)
        return;
    const r = papperet.getBoundingClientRect();
    papper_drag = { dx: h.clientX - r.left, dy: h.clientY - r.top };
    listen.setPointerCapture(h.pointerId);
    h.preventDefault();
});
listen.addEventListener('pointermove', h => {
    if (papper_drag)
        papper_flytta(h.clientX - papper_drag.dx, h.clientY - papper_drag.dy);
});
function papper_slapp() {
    if (!papper_drag)
        return;
    papper_drag = null;
    const r = papperet.getBoundingClientRect();
    try {
        localStorage.setItem(PAPPER_NYCKEL, JSON.stringify({ x: r.left, y: r.top }));
    } catch (fel) { /* sparas inte */ }
}
listen.addEventListener('pointerup', papper_slapp);
listen.addEventListener('pointercancel', papper_slapp);
addEventListener('resize', () => {
    if (papperet.style.left)
        papper_flytta(parseFloat(papperet.style.left), parseFloat(papperet.style.top));
});

papperet.addEventListener('toggle', () => {
    if (papper_oppet()) {
        papper_lage();
        $('papper_knapp').classList.remove('nytt');
        const ruta = $('papper_rulle');
        ruta.scrollTop = ruta.scrollHeight;
    }
});
document.addEventListener('keydown', h => {   /* Esc utan popover */
    if (h.key === 'Escape' && !med_popover)
        papper_visa(false);
});
$('papper_spara').addEventListener('click', () => {
    if (!papper_ritat) {
        meddela('Papperet är tomt.', 'The paper is empty.');
        return;
    }
    if ($('centronics').checked) {
        const lank = document.createElement('a');
        lank.href = URL.createObjectURL(new Blob([text_som_text('\f\n') + '\n'],
                                                 { type: 'text/plain;charset=utf-8' }));
        lank.download = 'papper.txt';
        lank.click();
        setTimeout(() => URL.revokeObjectURL(lank.href), 10000);
        meddela('papper.txt laddas ner (FF mellan sidorna).',
                'papper.txt is downloaded (FF between the pages).');
        skarm.focus();
        return;
    }
    pappret.toBlob(blob => {
        const lank = document.createElement('a');
        lank.href = URL.createObjectURL(blob);
        lank.download = 'papper.png';
        lank.click();
        setTimeout(() => URL.revokeObjectURL(lank.href), 10000);
    });
    meddela('papper.png laddas ner (som -CP papper.png i terminalen).',
            'papper.png is downloaded (like -CP papper.png in the terminal).');
    skarm.focus();
});
$('cmos_ladda').addEventListener('click', () => valj_fil('cmos', '.bin'));
window.addEventListener('pagehide', cmos_spara);
document.addEventListener('visibilitychange', () => {
    if (document.hidden)
        cmos_spara();
});

/* MÅLARE (exempel/malare/), när bandet ligger i sidan: fjärrstyrt, som
 * ett vanligt band, så att det stannar efter programmet. */
$('malare')?.addEventListener('click', () => {
    if (typeof FILER.malare_band === 'string')
        FILER.malare_band = base64(FILER.malare_band);
    abc80_lage();
    starta(null);
    lagg_i_band(FILER.malare_band, 'MÅLARE');
    $('fjarr').checked = true;
    knappar(true);
    e.paus(1000);
    skriv('RUN CAS:\n');
    meddela('MÅLARE av Mikael Bonnier 1982 (GPL-3.0). Svara med ett tal och RETURN; '
            + 'tangenterna runt F flyttar, Caps Lock på suddar, S tömmer skärmen.',
            'MÅLARE by Mikael Bonnier 1982 (GPL-3.0). Answer with a number and RETURN; '
            + 'the keys around F move, Caps Lock on erases, S clears the screen.');
    skarm.focus();
});

const GENESIS = ['ABCDemo av Genesis Project (2015): kod Shadow, grafik och musik Mermaid. ',
                 'ABCDemo by Genesis Project (2015): code Shadow, graphics and music Mermaid. '];

/* Från bandet: musiken ligger efter programmet och hörs utan
 * fjärrstyrning, så att bandet går vidare.
 * Knapparna finns bara när demot ligger i sidan. */
$('genesis')?.addEventListener('click', () => {
    if (typeof FILER.genesis_band === 'string')
        FILER.genesis_band = base64(FILER.genesis_band);
    abc80_lage();
    starta(null);
    lagg_i_band(FILER.genesis_band, 'Genesis-bandet');
    $('fjarr').checked = false;
    knappar(false);
    play_vid_relaet = true;
    e.paus(1000);
    skriv('RUN CAS:\n');
    meddela(GENESIS[0] + 'Från bandet; musiken spelas av bandspelaren, utan fjärrstyrning.',
            GENESIS[1] + 'From tape; the music is played by the tape recorder, without remote control.');
    skarm.focus();
});

$('genesis_skiva')?.addEventListener('click', () => {
    abc80_lage();
    skivans_namn = 'genesis.dsk';
    starta(FILER.genesis.slice());
    e.paus(1000);
    skriv('RUN DR0:A.B\n');
    meddela(GENESIS[0] + 'Från disketten, utan musik: den finns bara på bandet.',
            GENESIS[1] + 'From disk, without music: it is only on the tape.');
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
    byt_sprak(valt);
    paneler_las_in();

    const { instance } = await WebAssembly.instantiate(base64(FILER.wasm), {});
    e = instance.exports;
    ROM.ny = base64(FILER.rom_ny);
    ROM.gammal = base64(FILER.rom_gammal);
    if (FILER.rom_b2)
        ROM.b2 = base64(FILER.rom_b2);
    ROM.dos = base64(FILER.dos);
    ROM.p40 = base64(FILER.p40);
    ROM.pr9704 = base64(FILER.pr9704);
    ROM.tkn80 = base64(FILER.tkn80);
    if (FILER.cpm_start) {
        ROM.cpm_start = base64(FILER.cpm_start);
        ROM.cpm_skiva = base64(FILER.cpm_skiva);
        ROM.cpm_b = base64(FILER.cpm_b);
        ROM.cpm_d = new Uint8Array(KORT44_SKIVA).fill(0xE5);
        ROM.cpm_d.set(base64(FILER.cpm_d));
    }
    if (FILER.program)
        FILER.program = base64(FILER.program);
    if (FILER.program_b2)
        FILER.program_b2 = base64(FILER.program_b2);
    if (FILER.systemskiva) {
        FILER.systemskiva = base64(FILER.systemskiva);
        FILER.demoskiva = base64(FILER.demoskiva);
    }
    if (FILER.genesis)
        FILER.genesis = base64(FILER.genesis);
    if (FILER.krets) {
        FILER.krets = base64(FILER.krets);
        FILER.krets_cmos = base64(FILER.krets_cmos);
    }
    if (FILER.forthaid)
        FILER.forthaid = base64(FILER.forthaid);
    if (FILER.forthaid2) {
        FILER.forthaid2 = base64(FILER.forthaid2);
        FILER.forthaid2_banker = FILER.forthaid2_banker.map(base64);
    }
    cmos_las_in();

    if (fraga.get('rom') === 'gammal' || (fraga.get('rom') === 'b2' && ROM.b2))
        $('rom').value = fraga.get('rom');
    if (fraga.get('ljud') === 'av')
        ljud_valt = false;
    if (FILER.exempel)
        for (const namn in FILER.exempel)
            FILER.exempel[namn] = base64(FILER.exempel[namn]);
    else
        for (const namn in EXEMPEL)
            delete EXEMPEL[namn];
    if ($('krets').options.length < 2)
        $('krets').hidden = true;            /* varken Forth eller exemplen */
    const krets = fraga.get('krets');
    if ($('krets') && ((FILER.krets && ['forth', 'cmos'].includes(krets)) ||
                       (FILER.forthaid && krets === 'forthaid') ||
                       (FILER.forthaid2 && krets === 'forthaid2') || krets in EXEMPEL)) {
        $('krets').value = krets;
        oppna_panel('panel_program');
    }
    if (fraga.get('cmos') === 'pa') {
        $('cmos').checked = true;
        oppna_panel('panel_cmos');
    }
    if (fraga.get('p40') === 'pa') {
        $('p40').checked = true;
        oppna_panel('panel_skrivaren');
    } else if (fraga.get('centronics') === 'pa') {
        $('centronics').checked = true;
        oppna_panel('panel_skrivaren');
    }
    papper_slag();
    if (fraga.get('tkn80') === 'pa')
        $('tkn80').checked = true;
    if (fraga.get('fast') === 'pa')
        $('fast').checked = true;
    if (fraga.get('skarp') === 'pa') {
        $('skarp').checked = true;
        $('ram').classList.add('skarp');
    }
    const med_zork = fraga.get('zork') === 'pa' && $('cpm');
    if ((fraga.get('cpm') === 'pa' || med_zork) && $('cpm'))
        $('cpm').checked = true;
    visa_ljudet();
    starta(null, null, null);
    visa_lagen(true);
    if (cpm_valt())
        meddela(...CPM_TEXT);
    const med_system = ['system', 'demo'].includes(fraga.get('skiva')) && FILER.systemskiva && !cpm_valt();
    if (med_system)
        (fraga.get('skiva') === 'demo' ? demoskivan : systemskivan)();
    if (med_zork) {
        e.paus(1500);
        skriv('ZTOLK D:ZORK1\n');
        meddela(...ZORK_TEXT);
    } else if (fraga.get('k')) {
        e.paus(1000);
        skriv(fraga.get('k'), true);
    } else if (!fraga.get('s') && !med_system)
        kom_igang();
    v24_text = fraga.get('s');
    skarm.focus();
    requestAnimationFrame(varv);
})();
