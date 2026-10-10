/*
 * abc80.js -- emulatorn i webbläsaren, till boken ABC80 inifrån
 *
 * Kärnan är WebAssembly (webb.c och karna/, make webb); här finns allt
 * runt den: skärmen på en canvas, tangentbordet, ljudet med Web Audio,
 * filerna och knapparna. bygg.sh lägger in WebAssembly-koden, ROM:arna,
 * exempeldisketten, Forth-kretsarna, MÅLARE-bandet och Genesis-demot (de
 * fyra om de finns)
 * som base64 i FILER, så att sidan inte behöver hämta något.
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
 * CMOS-minnet: 2 KB RAM på $5000, som Super Smartaids med batteri. Det
 * läggs in efter varje start och sparas i webbläsaren (localStorage),
 * en gång i sekunden när det har ändrats och när sidan stängs. Forth
 * med CMOS är tolken från exempel 7 med de egna orden där.
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
let provtakt = 48000;                        /* ljudets prov/s: webbläsarens egen takt */
let nasta = null;                            /* klockans tid för nästa bild */
let skivan = null;                           /* disketten (bytes), eller null */
let skivans_minne = 0;                       /* avbilden i WebAssemblys minne */
let skivans_namn = 'diskett.dsk';            /* namnet när den sparas */
let bandljudet = null;                       /* bandets ljud i provtakten, eller null */

function base64(text) {
    const s = atob(text);
    const b = new Uint8Array(s.length);
    for (let k = 0; k < s.length; k++)
        b[k] = s.charCodeAt(k);
    return b;
}

function meddela(text) {
    $('meddelande').textContent = text;
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

/* Starta maskinen; med en diskett sitter ABC-DOS på $6000 och
 * disketten i FD2 (kort 45). Utan argument sitter samma diskett kvar,
 * med det som har sparats på den. En krets sitter på $4000, som
 * Smartaid (-l forth.bin@4000); Forth med CMOS slår på CMOS-minnet. */
function starta(diskett = skivan_nu()) {
    cmos_spara();                            /* innan minnet töms */
    const rom = ROM[$('rom').value];
    const p = lagg_i_minnet(rom);
    e.starta(16, p, rom.length, provtakt);
    e.slapp(p);
    knappar();
    if (skivans_minne)                       /* kortet är tomt nu */
        e.slapp(skivans_minne);
    skivans_minne = 0;
    skivan = diskett;
    $('katalog').hidden = true;
    if (skivan) {
        const dos = lagg_i_minnet(ROM.dos);
        e.krets(0x6000, dos, ROM.dos.length);
        e.slapp(dos);
        /* Kortet skriver i avbilden, så den ligger kvar (256 bytes över). */
        skivans_minne = lagg_i_minnet(skivan, 256);
        e.skiva(45, 0, skivans_minne, skivan.length, 0);
    }
    const val = $('krets')?.value;
    if (val) {
        const data = val === 'cmos' ? FILER.krets_cmos : FILER.krets;
        const krets = lagg_i_minnet(data);
        e.krets(0x4000, krets, data.length);
        e.slapp(krets);
        if (val === 'cmos')
            $('cmos').checked = true;
    }
    cmos_pekare = 0;
    if ($('cmos').checked) {
        const p = lagg_i_minnet(cmos);
        cmos_pekare = e.cmos(CMOS_ADRESS, p, CMOS_STORLEK);
        e.slapp(p);
    }
    papper_tomt();
    p40_koppla();
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
 * flaggor gäller -k: \r, \w ms, \xHH, \\. */
function skriv(text, flaggor = false) {
    for (let k = 0; k < text.length; k++) {
        const c = text[k];
        if (flaggor && c === '\\' && k + 1 < text.length) {
            const n = text[++k];
            if (n === 'r')
                e.tangent(0x0D);
            else if (n === 'w') {
                const m = /^\d+/.exec(text.slice(k + 1));
                e.paus(m ? +m[0] : 0);
                k += m ? m[0].length : 0;
            } else if (n === 'x') {
                e.tangent(parseInt(text.substr(k + 1, 2), 16) & 0xFF);
                k += 2;
            } else if (n !== '.')
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
        const kod = tangent_kod(c);
        e.tangent(kod || 0x3F);
    }
}

skarm.addEventListener('keydown', (h) => {
    if (h.metaKey)
        return;
    let kod = 0;
    if (h.ctrlKey && h.key.length === 1) {
        if (h.key === 'v' || h.key === 'V')
            return;                          /* klistra in */
        kod = h.key.toUpperCase().charCodeAt(0) & 0x1F;
    } else if (h.key.length === 1)
        kod = tangent_kod(h.key);
    else
        kod = { Enter: 0x0D, Backspace: 0x08, ArrowLeft: 0x08,
                ArrowRight: 0x09, Escape: 0x1B, Tab: 0x09 }[h.key] || 0;
    if (!kod)
        return;
    h.preventDefault();
    vack_ljudet();
    e.tangent(kod);
});

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
 * förrän man har klickat eller skrivit på sidan; då väcks det. Det
 * skapas i webbläsarens egen provtakt, som kärnans ljud får. */
let ljud_valt = true;

try {
    ljud = new AudioContext();
    provtakt = ljud.sampleRate;
    ljud.addEventListener('statechange', visa_ljudet);
} catch (fel) {
    ljud = null;
}

function vack_ljudet() {
    if (ljud_valt && ljud && ljud.state !== 'running')
        ljud.resume().catch(() => {});
}

function visa_ljudet() {
    const knapp = $('ljud');
    let text = 'Ljud: av';
    if (!ljud)
        text = 'Inget ljud';
    else if (ljud_valt)
        text = ljud.state === 'running' ? 'Ljud: på' : 'Ljud: klicka på skärmen';
    if (knapp.textContent !== text)
        knapp.textContent = text;
    knapp.classList.toggle('pa', ljud_valt && !!ljud);
}

document.addEventListener('pointerdown', vack_ljudet, true);
document.addEventListener('keydown', vack_ljudet, true);

$('ljud').addEventListener('click', () => {
    ljud_valt = !ljud_valt;
    if (ljud)
        ljud_valt ? vack_ljudet() : ljud.suspend();
    nasta = null;
    visa_ljudet();
    skarm.focus();
});

function ljudet_gar() {
    return ljud_valt && ljud && ljud.state === 'running';
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
        $('p40_visa').classList.remove('nytt');
}

/* Skrivaren och drivrutinen in eller ur, som rutan säger. */
function p40_koppla() {
    if ($('p40').checked) {
        const p = lagg_i_minnet(ROM.p40);
        e.krets(0x7800, p, ROM.p40.length);
        e.slapp(p);
    }
    e.p40($('p40').checked ? 1 : 0);
}

function papper_tomt() {
    papper_ritat = 0;
    papper_rader = 0;
    papper_senast = 0;
    papper_visa(false);
    $('p40_visa').disabled = true;
    $('p40_visa').classList.remove('nytt');
    $('papper_rubrik').textContent = 'Papperet';
}

function papper_anslag(fran, till) {
    return new DataView(e.memory.buffer, e.p40_anslag() + fran * P40_ANSLAG,
                        (till - fran) * P40_ANSLAG);
}

/* Rita de anslag som har kommit sedan sist. Behövs fler rader görs
 * bilden om och allt ritas igen. */
function papper_rita() {
    if (!$('p40').checked)
        return;
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
    $('p40_visa').disabled = false;
    $('papper_rubrik').textContent = 'Papperet, ' + papper_rader +
        (papper_rader === 1 ? ' rad' : ' rader');
    /* En ny utskrift tar fram lappen; stängs den under utskriften
     * lyser bara knappen. */
    if (ny_utskrift)
        papper_visa(true);
    else if (!papper_oppet())
        $('p40_visa').classList.add('nytt');
    const ruta = $('papper_rulle');
    if (ny_utskrift || ruta.scrollTop + ruta.clientHeight >= ruta.scrollHeight - 80)
        ruta.scrollTop = ruta.scrollHeight;
    if (e.p40_fullt())
        meddela('Papperet är fullt: riv av det.');
}

let anvande_ljud = false;

const bilden = ritning.createImageData(320, 240);

function rita() {
    bilden.data.set(new Uint8Array(e.memory.buffer, e.punkter(), 320 * 240 * 4));
    ritning.putImageData(bilden, 0, 0);
}

function varv() {
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
    if (nasta === null || med_ljud !== anvande_ljud || nu - nasta > 0.25 || nasta - nu > 0.5)
        nasta = nu + (med_ljud ? 0.05 : 0);
    anvande_ljud = med_ljud;
    const markor = Math.floor(performance.now() / 330) % 2 === 0 ? 1 : 0;

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
    visa_bandet();
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
    if (!har_band && !spelar_in)
        text = 'inget band';
    else if (spelar_in)
        text = 'spelar in ' + sekunder(e.bandlage());
    else
        text = sekunder(e.bandlage()) + ' / ' + sekunder(e.bandlangd());
    if (e.motor())
        text += ' · motorn går';
    else if (e.bandet_gar())
        text += ' · bandet går';
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
        meddela(spelar_in ? 'Under inspelning går det inte att spola.' : 'Inget band i kassetten.');
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
        meddela(namn + ': ingen WAV-fil med PCM, 8 eller 16 bitar');
        return;
    }
    har_band = true;
    spelar_in = false;
    stanna_spolningen();
    rakneverk_noll = 0;
    knappar(true);
    $('spelain').classList.remove('pa');
    meddela(namn + ' ligger i kassetten (' + flanker + ' flanker). Skriv LOAD CAS: eller RUN CAS:');
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
        meddela('minnet räcker inte till inspelningen');
        return;
    }
    spelar_in = true;
    har_band = false;
    stanna_spolningen();
    knappar(true);
    bandljudet = null;
    $('spelain').classList.add('pa');
    meddela('Inspelningsknappen är nere. Skriv SAVE CAS:NAMN och tryck sedan Spara inspelning.');
    skarm.focus();
});

$('spara').addEventListener('click', () => {
    if (!spelar_in || !e.inspelade_flanker()) {
        meddela('Inget har spelats in än: tryck Spela in och skriv SAVE CAS:NAMN.');
        return;
    }
    const p = e.inspelning();
    if (!p) {
        meddela('minnet räcker inte till WAV-filen');
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
                       String(antal).padStart(4) + (antal === 1 ? ' sektor' : ' sektorer'));
        }
    let lediga = 0;
    for (let n = 0; n < sektorer; n++)
        if (!(d[6 * 256 + (n >> 3)] & 0x80 >> (n & 7)))
            lediga++;
    rader.push((rader.length ? '' : 'Inga filer. ') + rader.length +
               (rader.length === 1 ? ' fil, ' : ' filer, ') + lediga +
               ' sektorer lediga (' + Math.floor(lediga / 4) + ' KB).');
    return rader.join('\n');
}

$('katalog_knapp').addEventListener('click', () => {
    const d = skivan_nu();
    const ruta = $('katalog');
    if (!d) {
        ruta.hidden = true;
        meddela('Ingen diskett i FD2: lägg i en med Lägg i diskett… eller Exempeldisketten.');
        return;
    }
    ruta.textContent = skivans_namn + ' i DR0:\n' + katalogen(d);
    ruta.hidden = false;
    skarm.focus();
});

$('spara_skiva').addEventListener('click', () => {
    const d = skivan_nu();
    if (!d) {
        meddela('Ingen diskett i FD2: lägg i en med Lägg i diskett… eller Exempeldisketten.');
        return;
    }
    const lank = document.createElement('a');
    lank.href = URL.createObjectURL(new Blob([d], { type: 'application/octet-stream' }));
    lank.download = skivans_namn;
    lank.click();
    setTimeout(() => URL.revokeObjectURL(lank.href), 10000);
    meddela(skivans_namn + ' laddas ner med det som har sparats på den.');
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
    if (vad === 'band')
        lagg_i_band(data, namn);
    else if (vad === 'cmos') {
        cmos_byt(data);
        meddela(namn + ' ligger i CMOS-minnet' + ($('cmos').checked ? '.' : ' (slå på det).'));
    }
    else if (vad === 'skiva') {
        skivans_namn = namn;
        starta(data);
        meddela(namn + ' sitter i FD2 med ABC-DOS. Katalogen visar filerna; skriv RUN DR0:NAMN.');
    } else {
        skriv(new TextDecoder('utf-8').decode(data));
        meddela(namn + ' skrivs in.');
    }
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
$('nystart').addEventListener('click', () => { starta(null); meddela(''); skarm.focus(); });
$('rom').addEventListener('change', () => { starta(); skarm.focus(); });
/* Exempeldisketten (exempel/program/), när den ligger i sidan. */
$('exempel')?.addEventListener('click', () => {
    skivans_namn = 'exempel.dsk';
    starta(FILER.program.slice());
    meddela('Exempeldisketten: RUN DR0:GISSA, RUN DR0:KURVA, RUN DR0:MUSIK eller RUN DR0:AIRFIGHT.');
    skarm.focus();
});

/* Forth-kretsarna (exempel/krets/), när de ligger i sidan: maskinen
 * startas om med den valda kretsen eller utan. */
const KRETSAR = {
    '': '',
    forth: 'Forth-kretsen på $4000: POKE 65052,0,208, NEW och Z=CALL(16384,3).',
    cmos: 'Forth med CMOS: Z=CALL(16384,3); orden sparas i CMOS-minnet på $5000.',
};
$('krets')?.addEventListener('change', () => {
    starta();
    meddela(KRETSAR[$('krets').value]);
    skarm.focus();
});

/* CMOS-minnet: på eller av startar om maskinen (som att sätta i kortet). */
$('cmos').addEventListener('change', () => {
    if (!$('cmos').checked && $('krets')?.value === 'cmos')
        $('krets').value = '';
    starta();
    meddela($('cmos').checked
        ? 'CMOS-minnet på $5000-$57FF (20480-22527), sparat i webbläsaren.'
        : 'CMOS-minnet är av; det som står där finns kvar till nästa gång.');
    skarm.focus();
});
$('cmos_tom').addEventListener('click', () => {
    cmos_byt(new Uint8Array(0));
    meddela('CMOS-minnet är tomt.');
    skarm.focus();
});
$('cmos_spara').addEventListener('click', () => {
    cmos_spara();
    const lank = document.createElement('a');
    lank.href = URL.createObjectURL(new Blob([cmos], { type: 'application/octet-stream' }));
    lank.download = 'cmos.bin';
    lank.click();
    setTimeout(() => URL.revokeObjectURL(lank.href), 10000);
    meddela('cmos.bin laddas ner (som -M cmos.bin i terminalen).');
    skarm.focus();
});
/* Skrivaren kopplas in och ur medan maskinen går: programmet ligger
 * kvar, och BASIC hittar drivrutinen när PR: öppnas. */
$('p40').addEventListener('change', () => {
    p40_koppla();
    meddela($('p40').checked
        ? 'P40 på kort 60: 10 OPEN "PR:" AS FILE 1, 20 PRINT #1,"HEJ", 30 CLOSE 1, eller LIST PR:.'
        : 'Skrivaren är urkopplad; papperet ligger kvar.');
    skarm.focus();
});
$('p40_riv').addEventListener('click', () => {
    if ($('p40').checked)
        e.p40_riv();
    papper_tomt();
    meddela('Papperet är avrivet.');
    skarm.focus();
});
if (!med_popover)                            /* annars popovertarget */
    $('p40_visa').addEventListener('click', () => papper_visa(!papper_oppet()));
$('p40_stang').addEventListener('click', () => {
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
if (typeof $('hjalp').showPopover !== 'function')
    $('hjalp_visa').hidden = true;           /* utan popover: bara texten nedan */
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
        $('p40_visa').classList.remove('nytt');
        const ruta = $('papper_rulle');
        ruta.scrollTop = ruta.scrollHeight;
    }
});
document.addEventListener('keydown', h => {   /* Esc utan popover */
    if (h.key === 'Escape' && !med_popover)
        papper_visa(false);
});
$('p40_spara').addEventListener('click', () => {
    if (!papper_ritat) {
        meddela('Papperet är tomt.');
        return;
    }
    pappret.toBlob(blob => {
        const lank = document.createElement('a');
        lank.href = URL.createObjectURL(blob);
        lank.download = 'papper.png';
        lank.click();
        setTimeout(() => URL.revokeObjectURL(lank.href), 10000);
    });
    meddela('papper.png laddas ner (som -CP papper.png i terminalen).');
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
    starta(null);
    lagg_i_band(FILER.malare_band, 'MÅLARE');
    $('fjarr').checked = true;
    knappar(true);
    e.paus(1000);
    skriv('RUN CAS:\n');
    meddela('MÅLARE av Mikael Bonnier 1982 (GPL-3.0). Svara med ett tal och RETURN; '
            + 'tangenterna runt F flyttar, Caps Lock på suddar, S tömmer skärmen.');
    skarm.focus();
});

const GENESIS = 'ABCDemo av Genesis Project (2015): kod Shadow, grafik och musik Mermaid. ';

/* Från bandet: musiken ligger efter programmet och hörs utan
 * fjärrstyrning, så att bandet går vidare.
 * Knapparna finns bara när demot ligger i sidan. */
$('genesis')?.addEventListener('click', () => {
    if (typeof FILER.genesis_band === 'string')
        FILER.genesis_band = base64(FILER.genesis_band);
    starta(null);
    lagg_i_band(FILER.genesis_band, 'Genesis-bandet');
    $('fjarr').checked = false;
    knappar(false);
    play_vid_relaet = true;
    e.paus(1000);
    skriv('RUN CAS:\n');
    meddela(GENESIS + 'Från bandet; musiken spelas av bandspelaren, utan fjärrstyrning.');
    skarm.focus();
});

$('genesis_skiva')?.addEventListener('click', () => {
    skivans_namn = 'genesis.dsk';
    starta(FILER.genesis.slice());
    e.paus(1000);
    skriv('RUN DR0:A.B\n');
    meddela(GENESIS + 'Från disketten, utan musik: den finns bara på bandet.');
    skarm.focus();
});

/* ---------------------------------------------------------------- */
/* Starten                                                           */

(async function () {
    const { instance } = await WebAssembly.instantiate(base64(FILER.wasm), {});
    e = instance.exports;
    ROM.ny = base64(FILER.rom_ny);
    ROM.gammal = base64(FILER.rom_gammal);
    ROM.dos = base64(FILER.dos);
    ROM.p40 = base64(FILER.p40);
    if (FILER.program)
        FILER.program = base64(FILER.program);
    if (FILER.genesis)
        FILER.genesis = base64(FILER.genesis);
    if (FILER.krets) {
        FILER.krets = base64(FILER.krets);
        FILER.krets_cmos = base64(FILER.krets_cmos);
    }
    cmos_las_in();

    const fraga = new URLSearchParams(location.search);
    if (fraga.get('rom') === 'gammal')
        $('rom').value = 'gammal';
    if (fraga.get('ljud') === 'av')
        ljud_valt = false;
    if (['forth', 'cmos'].includes(fraga.get('krets')) && $('krets'))
        $('krets').value = fraga.get('krets');
    if (fraga.get('cmos') === 'pa')
        $('cmos').checked = true;
    if (fraga.get('p40') === 'pa')
        $('p40').checked = true;
    visa_ljudet();
    starta(null);
    if (fraga.get('k')) {
        e.paus(1000);
        skriv(fraga.get('k'), true);
    } else
        kom_igang();
    skarm.focus();
    requestAnimationFrame(varv);
})();
