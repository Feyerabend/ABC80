/*
 * abc80.js -- emulatorn i webbläsaren, till boken ABC80 inifrån
 *
 * Kärnan är WebAssembly (webb.c och karna/, make webb); här finns allt
 * runt den: skärmen på en canvas, tangentbordet, ljudet med Web Audio,
 * filerna och knapparna. bygg.sh lägger in WebAssembly-koden, ROM:arna,
 * exempeldisketten och Genesis-demot (de två om de finns) som base64 i FILER, så
 * att sidan inte behöver hämta något.
 *
 * Tiden: en bild är 20 ms. Med ljudet på följer emulatorn ljudets
 * klocka, och varje bilds prov läggs i en AudioBuffer som spelas precis
 * efter den förra; utan ljud följer den performance.now(). Bilderna
 * körs lite före (LJUD_FORE), så att ljudet inte hackar. Medan
 * kassettens motor går körs maskinen fortare om "snabbt" är ikryssat.
 *
 * Bandets eget ljud: WAV-filen läses här (avkoda_bandet) och räknas om
 * till provtakten, och medan bandet går (och inte snabbt) läggs det som
 * passerar huvudet till bildens prov. Med "PLAY nere" stannar bandet inte när ROM:en slår av motorn;
 * så hörs musiken efter programmet i Genesis-demots band.
 *
 * Disketten: kortet skriver i avbilden i WebAssemblys minne, så det som
 * sparas med SAVE DR0: finns där tills maskinen startas om med en annan
 * diskett. "Spara disketten" laddar ner avbilden som den är nu, och
 * "Katalogen" visar filerna, läst ur katalogen i sektor 16-23 som
 * ABC-DOS ordnar den (band 2, kapitlet om FD2).
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
 * med det som har sparats på den. */
function starta(diskett = skivan_nu()) {
    const rom = ROM[$('rom').value];
    const p = lagg_i_minnet(rom);
    e.starta(16, p, rom.length, provtakt);
    e.slapp(p);
    e.spela_vidare($('play').checked ? 1 : 0, 0);
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

let anvande_ljud = false;

const bilden = ritning.createImageData(320, 240);

function rita() {
    bilden.data.set(new Uint8Array(e.memory.buffer, e.punkter(), 320 * 240 * 4));
    ritning.putImageData(bilden, 0, 0);
}

function varv() {
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
            const fran = bandljudet && e.bandet_gar() ? e.bandlage() : null;
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
}

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

$('play').addEventListener('change', () => {
    e.spela_vidare($('play').checked ? 1 : 0, har_band ? 1 : 0);
    skarm.focus();
});

$('spelain').addEventListener('click', () => {
    if (!e.spela_in()) {
        meddela('minnet räcker inte till inspelningen');
        return;
    }
    spelar_in = true;
    har_band = false;
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

const GENESIS = 'ABCDemo av Genesis Project (2015): kod Shadow, grafik och musik Mermaid. ';

/* Från bandet: musiken ligger efter programmet och hörs med PLAY nere.
 * Knapparna finns bara när demot ligger i sidan. */
$('genesis')?.addEventListener('click', () => {
    if (typeof FILER.genesis_band === 'string')
        FILER.genesis_band = base64(FILER.genesis_band);
    starta(null);
    lagg_i_band(FILER.genesis_band, 'Genesis-bandet');
    $('play').checked = true;
    e.spela_vidare(1, 0);
    e.paus(1000);
    skriv('RUN CAS:\n');
    meddela(GENESIS + 'Från bandet; musiken spelas av bandspelaren, med PLAY nere.');
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
    if (FILER.program)
        FILER.program = base64(FILER.program);
    if (FILER.genesis)
        FILER.genesis = base64(FILER.genesis);

    const fraga = new URLSearchParams(location.search);
    if (fraga.get('rom') === 'gammal')
        $('rom').value = 'gammal';
    if (fraga.get('ljud') === 'av')
        ljud_valt = false;
    visa_ljudet();
    starta(null);
    if (fraga.get('k')) {
        e.paus(1000);
        skriv(fraga.get('k'), true);
    }
    skarm.focus();
    requestAnimationFrame(varv);
})();
