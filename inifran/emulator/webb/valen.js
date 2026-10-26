/*
 * valen.js -- prov av valen på webbsidan (VAL i abc80.js) med Playwright
 *
 *   NODE_PATH=.../node_modules node webb/valen.js [chromium|webkit] [-p]
 *
 * Kräver playwright-core och dess webbläsare (npx playwright install).
 * Först de enskilda reglerna: vad som dimmas när ett val är på, och
 * att skälet står i title. Sedan ?-parametrar som krockar. Med -p
 * dessutom alla par av val som ?-parametrar: att sidan startar (ABC80,
 * ABC800 eller CP/M:s A> på skärmen), att inget som är på och gäller
 * krockar med något annat som är på och gäller, och att inget som är
 * på och gäller är dimmat. Skriver ok eller FEL per prov.
 */

const pw = require('playwright-core');
const path = require('path');

const motor = process.argv[2] && !process.argv[2].startsWith('-') ? process.argv[2] : 'chromium';
const par = process.argv.includes('-p');
const SIDAN = 'file://' + path.resolve(__dirname, 'abc80.html');

let fel = 0;
function prov(namn, ok, mer = '') {
    console.log((ok ? 'ok   ' : 'FEL  ') + namn + (ok ? '' : '  ' + mer));
    if (!ok)
        fel++;
}

async function sida(webblasare, fraga) {
    const p = await webblasare.newPage();
    p.fel = [];
    p.on('pageerror', e => p.fel.push(e.message));
    await p.addInitScript(() => {
        for (const n of ['instantiate', 'instantiateStreaming']) {
            const o = WebAssembly[n];
            WebAssembly[n] = async (...a) => {
                const r = await o.apply(WebAssembly, a);
                window.__ex = (r.instance || r).exports;
                return r;
            };
        }
        localStorage.setItem('abc80_sprak', 'sv');
    });
    await p.goto(SIDAN + '?ljud=av&' + fraga);
    await p.waitForFunction(() => window.__ex);
    await p.keyboard.press('Escape');
    return p;
}

/* Skärmen som text, 40 eller 80 tecken per rad. */
function skarmen(p) {
    return p.evaluate(() => {
        const e = window.__ex, a = e.bildminne(), m = new Uint8Array(e.memory.buffer);
        const k = e.kolumner(), r = [];
        for (let i = 0; i < 24; i++) {
            const o = k === 80 ? ((i & 7) << 8) + (i >> 3) * 80 : ((i & 7) << 7) + (i >> 3) * 40;
            r.push(Array.from(m.slice(a + o, a + o + k), c => String.fromCharCode(c & 0x7F)).join(''));
        }
        return r.join('\n');
    });
}

/* Dimmat och skälet: {dimmat, titel} för ett id i VAL. */
function lage(p, id) {
    return p.evaluate(id => {
        const el = valets_element(id);
        if (!el)
            return null;
        const t = el.tagName === 'OPTION' ? el.textContent : (el.closest('label') ?? el).title;
        return { dimmat: el.disabled, titel: t };
    }, id);
}

async function dimmat(p, namn, id, ska, skal) {
    const l = await lage(p, id);
    if (!l)
        return;                              /* finns inte i den här sidan */
    prov(namn + ': ' + id + (ska ? ' dimmat' : ' inte dimmat'),
         l.dimmat === ska && (!ska || !skal || l.titel.includes(skal)), JSON.stringify(l));
}

async function klicka(p, id, varde) {
    await p.evaluate(([id, varde]) => {
        const el = document.getElementById(id);
        if (varde === undefined)
            el.checked = !el.checked;
        else
            el.value = varde;
        el.dispatchEvent(new Event('change'));
        visa_lagen(true);
    }, [id, varde]);
}

async function reglerna(w) {
    let p = await sida(w, '');
    await dimmat(p, 'utan TKN80', 'fast', true, 'bara med TKN80');
    for (const id of ['tkn80', 'cpm', 'cmos', 'krets:forthaid', 'krets:forthaid2', 'genesis', 'band'])
        await dimmat(p, 'grundläget', id, false);

    await klicka(p, 'rom', 'b2');
    for (const id of ['tkn80', 'fast', 'cpm', 'krets', 'cmos', 'p40', 'band', 'spelain', 'play',
                      'malare', 'systemskiva', 'demoskiva', 'genesis', 'genesis_skiva'])
        await dimmat(p, 'BASIC II', id, true, 'inte med BASIC II');
    for (const id of ['centronics', 'exempel'])
        await dimmat(p, 'BASIC II', id, false);
    await klicka(p, 'rom', 'ny');
    await dimmat(p, 'tillbaka från BASIC II', 'krets', false);

    await klicka(p, 'cpm');
    for (const id of ['krets', 'cmos', 'p40', 'centronics', 'band', 'spelain'])
        await dimmat(p, 'CP/M', id, true, 'inte med CP/M');
    for (const id of ['tkn80', 'malare', 'systemskiva', 'genesis'])
        await dimmat(p, 'CP/M', id, false);
    await klicka(p, 'cpm');

    await klicka(p, 'tkn80');
    await dimmat(p, 'TKN80', 'krets:forthaid', true, 'inte med TKN80');
    await dimmat(p, 'TKN80', 'genesis', true, 'skärmen');
    await dimmat(p, 'TKN80', 'genesis_skiva', true, 'skärmen');
    for (const id of ['krets:forthaid2', 'fast', 'cmos', 'malare', 'demoskiva'])
        await dimmat(p, 'TKN80', id, false);
    await klicka(p, 'tkn80');

    await klicka(p, 'cmos');
    await dimmat(p, 'CMOS', 'krets:forthaid', true, '$5000-$57FF');
    await dimmat(p, 'CMOS', 'krets:forthaid2', true, 'inte med CMOS-minnet');
    await dimmat(p, 'CMOS', 'krets:forth', false);
    await klicka(p, 'cmos');

    if (await lage(p, 'krets:forthaid')) {
        await klicka(p, 'krets', 'forthaid');
        await dimmat(p, 'Forthaid', 'tkn80', true, 'inte med Forthaid');
        await dimmat(p, 'Forthaid', 'cmos', true, 'inte med Forthaid');
        await dimmat(p, 'Forthaid', 'krets:forthaid2', false);
    }
    if (await lage(p, 'krets:forthaid2')) {
        await klicka(p, 'krets', 'forthaid2');
        await dimmat(p, 'Forthaid II', 'cmos', true, 'inte med Forthaid II');
        await dimmat(p, 'Forthaid II', 'tkn80', false);
    }
    await klicka(p, 'krets', 'cmos');
    prov('Forth med CMOS slår på CMOS-minnet', await p.evaluate(() => $('cmos').checked));
    prov('inga fel i sidan', !p.fel.length, p.fel.join('; '));
    await p.close();
}

/* ?-parametrar som krockar: [fråga, rutor som ska vara av, kretsen, texten]. */
const KROCKAR = [
    ['krets=forthaid&tkn80=pa', ['tkn80'], 'forthaid', 'TKN80 är av'],
    ['krets=forthaid&cmos=pa', ['cmos'], 'forthaid', 'CMOS-minnet är av'],
    ['krets=forthaid2&cmos=pa', ['cmos'], 'forthaid2', 'CMOS-minnet är av'],
    ['fast=pa', ['fast'], '', 'Fast markör är av'],
    ['rom=b2&tkn80=pa', [], '', 'TKN80 gäller inte'],
    ['cpm=pa&krets=forth', [], 'forth', ''],
];

async function krockarna(w) {
    for (const [fraga, av, krets, text] of KROCKAR) {
        const p = await sida(w, fraga);
        await p.waitForTimeout(1500);
        const s = await p.evaluate(() => ({
            pa: ['tkn80', 'fast', 'cmos', 'cpm'].filter(id => $(id).checked),
            krets: $('krets').value,
            text: $('meddelande').textContent,
            kolumner: window.__ex.kolumner(),
        }));
        prov('?' + fraga, av.every(id => !s.pa.includes(id)) && s.krets === krets &&
             (!text || s.text.includes(text)) && !p.fel.length,
             JSON.stringify(s) + p.fel.join('; '));
        if (fraga.startsWith('rom=b2'))
            prov('?' + fraga + ': 40 tecken', s.kolumner === 40);
        if (fraga.startsWith('cpm=pa'))
            prov('?' + fraga + ': CP/M startar', (await skarmen(p)).includes('A>'));
        await p.close();
    }
}

/* Alla par: varje val på för sig som en ?-parameter. */
const ENSKILDA = ['rom=gammal', 'rom=b2', 'tkn80=pa', 'fast=pa', 'cpm=pa', 'cmos=pa', 'p40=pa',
                  'centronics=pa', 'krets=forth', 'krets=cmos', 'krets=forthaid', 'krets=forthaid2',
                  'krets=e1', 'krets=e4', 'krets=e5', 'skiva=system'];

async function paren(w) {
    const alla = [];
    for (let i = 0; i < ENSKILDA.length; i++)
        for (let j = i + 1; j < ENSKILDA.length; j++)
            if (ENSKILDA[i].split('=')[0] !== ENSKILDA[j].split('=')[0])
                alla.push(ENSKILDA[i] + '&' + ENSKILDA[j]);
    const ett = async fraga => {
        const p = await sida(w, fraga);
        await p.waitForTimeout(2500);
        const s = await skarmen(p);
        const krock = await p.evaluate(() => {
            const pa = VAL.filter(v => i_bruk(v.id));
            for (const a of pa) {
                if (valets_element(a.id)?.disabled)
                    return a.id + ' på men dimmat';
                for (const b of pa)
                    if (a !== b && (a.tar ?? []).some(r => (b.tar ?? []).includes(r)))
                        return a.id + ' och ' + b.id;
            }
            return '';
        });
        prov('?' + fraga, /ABC80|A>/.test(s) && !krock && !p.fel.length,
             krock + ' ' + p.fel.join('; ') + '\n' + s.replace(/\s+\n/g, '\n').trim());
        await p.close();
    };
    for (let k = 0; k < alla.length; k += 4)
        await Promise.all(alla.slice(k, k + 4).map(ett));
}

(async () => {
    const w = await pw[motor].launch();
    await reglerna(w);
    await krockarna(w);
    if (par)
        await paren(w);
    await w.close();
    console.log(fel ? fel + ' FEL' : 'alla ok');
    process.exit(fel ? 1 : 0);
})();
