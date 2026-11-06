/*
 * stordator.js -- en IBM-stordator att ringa till från CH B, till
 * ABC 802 inifrån
 *
 * En REKONSTRUKTION: Datacentralen har inte funnits, och användarna,
 * filerna och jobben är påhittade här. Det som ska synas är hur en
 * persondator kom åt en stordator i början av 1980-talet: inte som en
 * 3270-skärm (den talade EBCDIC och blockläge över en egen ledning) utan
 * som en enkel radterminal, en "TTY", genom en frontdator som gjorde om
 * ASCII till EBCDIC och tillbaka. Stordatorn kör MVS med TSO i radläge:
 * man skriver en hel rad, och svaret kommer rad för rad. Ett jobb lämnas
 * in med SUBMIT, väntar i kön, körs och hamnar på utskriftskön, där
 * OUTPUT hämtar det: batch och interaktivt i samma samtal.
 *
 * Meddelandena (IKJ..., IEF..., $HASP...) är gjorda efter hur TSO och
 * JES2 skrev dem, ur minnet och inte ur manualerna (?): numren och
 * ordalydelsen kan skilja sig från de riktiga. Året är 1984.
 *
 * ABC 802 är terminalen: LOAD V24:TSA30B24.22A (typ T, 300 bit/s), som
 * för BBS:en. Frontdatorn skickar tillbaka det man skriver men inte
 * lösenordet. BREAK går i terminalen ut som $03, och det är TSO:s
 * attention: en lång utskrift avbryts och man får READY.
 *
 * skapa_stordator(sand, ko_langd) ger { ring, lagg_pa, tecken,
 * uppkopplad }, som skapa_bbs i bbs.js.
 */

const SD_SVENSKA = { 'Å': 0x5D, 'Ä': 0x5B, 'Ö': 0x5C, 'É': 0x40, 'Ü': 0x5E,
                     'å': 0x7D, 'ä': 0x7B, 'ö': 0x7C, 'é': 0x60, 'ü': 0x7E };
const SD_TILLBAKA = {};
for (const [c, b] of Object.entries(SD_SVENSKA))
    SD_TILLBAKA[b] = c;

function sd_bytes(text) {
    const ut = [];
    for (const c of text) {
        if (c === '\n')
            ut.push(0x0D, 0x0A);
        else if (c in SD_SVENSKA)
            ut.push(SD_SVENSKA[c]);
        else {
            const k = c.charCodeAt(0);
            ut.push(k < 0x80 ? k : 0x3F);
        }
    }
    return ut;
}

const SD_MANADER = ['JANUARY', 'FEBRUARY', 'MARCH', 'APRIL', 'MAY', 'JUNE', 'JULY',
                    'AUGUST', 'SEPTEMBER', 'OCTOBER', 'NOVEMBER', 'DECEMBER'];
const SD_AR = 1984;

function sd_tva(n) {
    return String(n).padStart(2, '0');
}

/* 10:31:47, med punkter (10.31.47) för JES2. */
function sd_klocka(d, tecken = ':') {
    return sd_tva(d.getHours()) + tecken + sd_tva(d.getMinutes()) + tecken + sd_tva(d.getSeconds());
}

function sd_datum(d) {
    return SD_MANADER[d.getMonth()] + ' ' + d.getDate() + ', ' + SD_AR;
}

/* Det julianska datumet, 84311, som i IEF373I. */
function sd_juliansk(d) {
    const forsta = new Date(SD_AR, 0, 1);
    const dag = new Date(SD_AR, d.getMonth(), d.getDate());
    return String(SD_AR % 100) + String(Math.round((dag - forsta) / 86400000) + 1).padStart(3, '0');
}

/* Filerna, med användarens namn först. Radnumren som i en ISPF/TSO-fil. */
function sd_filer(anv) {
    return {
        [anv + '.LASMIG.TEXT']: [
            'DET HÄR ÄR EN REKONSTRUKTION. DATACENTRALEN HAR INTE FUNNITS,',
            'OCH ALLT HÄR ÄR PÅHITTAT FÖR ATT VISA HUR ETT SAMTAL GICK TILL.',
            '',
            'ABC 802 ÄR HÄR EN RADTERMINAL: FRONTDATORN GÖR OM ASCII TILL',
            'EBCDIC. I DEN SVENSKA EBCDIC-TABELLEN STÅR Å Ä Ö DÄR DE',
            'AMERIKANSKA HAR $ # OCH @ (?).',
            '',
            'PRÖVA:  LISTCAT, LIST KVADRAT.FORT, SUBMIT KVADRAT,',
            '        STATUS, OUTPUT ' + anv + 'A, TIME, HELP OCH LOGOFF.',
            'BREAK AVBRYTER EN LÅNG UTSKRIFT.'],
        [anv + '.KVADRAT.CNTL']: [
            '//' + (anv + 'A').padEnd(8) + ' JOB (4711),\'ABC 802\',CLASS=A,MSGCLASS=X,',
            '//             NOTIFY=' + anv,
            '//STEP1    EXEC PGM=KVADRAT',
            '//STEPLIB  DD DSN=' + anv + '.LOAD,DISP=SHR',
            '//SYSPRINT DD SYSOUT=*'],
        [anv + '.KVADRAT.FORT']: [
            'C     KVADRATER OCH KUBER, FÖR ABC 802 (REKONSTRUKTION)',
            '      WRITE(6,10)',
            '   10 FORMAT(1H1,\'  N    N**2    N**3\')',
            '      DO 20 N=1,12',
            '      WRITE(6,30) N,N*N,N*N*N',
            '   20 CONTINUE',
            '   30 FORMAT(1H ,I3,I8,I8)',
            '      STOP',
            '      END'],
        [anv + '.LOAD']: null,               /* ett bibliotek, inte text */
    };
}

const SD_HJALP = [
    'FUNCTIONS -',
    '  LISTCAT   LIST THE NAMES OF YOUR DATA SETS',
    '  LIST      LIST A DATA SET, E.G. LIST KVADRAT.FORT',
    '  SUBMIT    SUBMIT A BATCH JOB, E.G. SUBMIT KVADRAT',
    '  STATUS    SHOW THE STATUS OF YOUR BATCH JOBS',
    '  OUTPUT    LIST THE OUTPUT OF A JOB THAT HAS ENDED',
    '  TIME      SHOW TIME, CPU TIME AND SESSION TIME',
    '  LOGOFF    END THE SESSION',
    'ATTENTION (BREAK) STOPS A LISTING.',
];

function skapa_stordator(sand, ko_langd) {
    let uppkopplad = false;
    let anv = '';
    let start = 0;
    let rad = '';
    let dold = false;
    let nar_rad = null;
    let pump = null;                         /* en utskrift på väg ut */
    let vantande = [];                       /* meddelanden till nästa READY */
    let jobb = [];
    let jobbnr = 1233;
    let klockor = [];
    let filer = {};

    function skriv(text) {
        sand(sd_bytes(text));
    }

    function las(gor, hemlig = false) {
        rad = '';
        dold = hemlig;
        nar_rad = gor;
    }

    function ready() {
        for (const m of vantande)
            skriv(m + '\n');
        vantande = [];
        skriv('READY\n');
        las(kommando);
    }

    /* En lång utskrift: en rad i taget, så att BREAK kan avbryta den. */
    function skriv_rader(rader) {
        let i = 0;
        nar_rad = null;
        pump = setInterval(() => {
            while (i < rader.length && ko_langd() < 160)
                skriv(rader[i++] + '\n');
            if (i >= rader.length) {
                stoppa_pumpen();
                ready();
            }
        }, 100);
    }

    function stoppa_pumpen() {
        if (pump !== null) {
            clearInterval(pump);
            pump = null;
        }
    }

    /* Ett namn utan citattecken får användarens namn först. */
    function fullt_namn(namn) {
        namn = namn.trim();
        if (/^'.*'$/.test(namn))
            return namn.slice(1, -1);
        return anv + '.' + namn;
    }

    function hitta_fil(namn) {
        const n = fullt_namn(namn);
        if (n in filer)
            return n;
        for (const f of Object.keys(filer))      /* KVADRAT räcker för .CNTL */
            if (f.startsWith(n + '.'))
                return f;
        return null;
    }

    function jobbets_lage(j) {
        const t = Date.now() - j.inlamnat;
        return t < 8000 ? 'vantar' : t < 16000 ? 'kor' : 'klar';
    }

    function jobbnamn(j) {
        return j.namn + '(JOB' + String(j.nr).padStart(5, '0') + ')';
    }

    function utskrift(j) {
        const d = new Date(j.inlamnat + 8000);
        const s = new Date(j.inlamnat + 16000);
        const k1 = sd_klocka(d, '.'), k2 = sd_klocka(s, '.');
        const nr = 'JOB' + String(j.nr).padStart(5, '0');
        const jul = sd_juliansk(d);
        const ut = [
            '                J E S 2  J O B  L O G  --  S Y S T E M  M V S 1',
            '',
            k1 + ' ' + nr + '  $HASP373 ' + j.namn.padEnd(8) + ' STARTED - INIT  1 - CLASS A - SYS MVS1',
            k2 + ' ' + nr + '  IEF142I ' + j.namn + ' STEP1 - STEP WAS EXECUTED - COND CODE 0000',
            k2 + ' ' + nr + '  $HASP395 ' + j.namn.padEnd(8) + ' ENDED',
            '',
            ...filer[anv + '.KVADRAT.CNTL'].map((r, i) => String(i + 1).padStart(9) + ' ' + r),
            '',
            'IEF236I ALLOC. FOR ' + j.namn + ' STEP1',
            'IEF237I 130  ALLOCATED TO STEPLIB',
            'IEF237I JES2 ALLOCATED TO SYSPRINT',
            'IEF142I ' + j.namn + ' STEP1 - STEP WAS EXECUTED - COND CODE 0000',
            'IEF285I   ' + (anv + '.LOAD').padEnd(40) + ' KEPT',
            'IEF373I STEP /STEP1   / START ' + jul + '.' + k1.slice(0, 5).replace('.', ''),
            'IEF374I STEP /STEP1   / STOP  ' + jul + '.' + k2.slice(0, 5).replace('.', '') +
                ' CPU    0MIN 00.04SEC',
            '',
            '  N    N**2    N**3',
        ];
        for (let n = 1; n <= 12; n++)
            ut.push(String(n).padStart(3) + String(n * n).padStart(8) + String(n * n * n).padStart(8));
        return ut;
    }

    function kommando(text) {
        const ord = text.trim().toUpperCase().split(/\s+/);
        const k = ord[0];
        const rest = text.trim().slice(k.length).trim().toUpperCase();
        if (!k)
            return las(kommando);
        if (k === 'HELP' || k === 'H')
            return skriv_rader(SD_HJALP);
        if (k === 'TIME') {
            const nu = new Date();
            const sek = Math.floor((Date.now() - start) / 1000);
            const h = nu.getHours() % 12 || 12;
            skriv('IKJ56650I TIME-' + sd_tva(h) + sd_klocka(nu).slice(2) +
                  (nu.getHours() < 12 ? ' AM' : ' PM') + '. CPU-00:00:0' + Math.min(9, 1 + (sek >> 6)) +
                  ' SERVICE-' + (4000 + sek * 37) + ' SESSION-' +
                  sd_tva(Math.floor(sek / 3600)) + ':' + sd_tva(Math.floor(sek / 60) % 60) + ':' +
                  sd_tva(sek % 60) + '\n          ' + SD_MANADER[nu.getMonth()] + ' ' + nu.getDate() + ',' + SD_AR + '\n');
            return ready();
        }
        if (k === 'LISTCAT' || k === 'LISTC')
            return skriv_rader(['IN CATALOG:SYS1.UCAT.TSO', ...Object.keys(filer).sort()]);
        if (k === 'LIST' || k === 'L') {
            if (!rest) {
                skriv('IKJ56700A ENTER DATA SET NAME -\n');
                return las((n) => kommando('LIST ' + n));
            }
            const f = hitta_fil(rest);
            if (!f) {
                skriv('IKJ58503I DATA SET ' + fullt_namn(rest) + ' NOT IN CATALOG\n');
                return ready();
            }
            if (!filer[f]) {
                skriv('IKJ58509I DATA SET ' + f + ' IS A LOAD LIBRARY, NOT TEXT\n');
                return ready();
            }
            return skriv_rader([f, ...filer[f].map((r, i) =>
                String((i + 1) * 10).padStart(5, '0') + ' ' + r)]);
        }
        if (k === 'SUBMIT' || k === 'SUB') {
            const f = hitta_fil(rest || 'KVADRAT.CNTL');
            if (!f || !f.endsWith('.CNTL')) {
                skriv('IKJ56251I NO JOB CARD IN ' + (f || fullt_namn(rest)) + '\n');
                return ready();
            }
            const j = { namn: anv + 'A', nr: ++jobbnr, inlamnat: Date.now(), anmalt: false };
            jobb.push(j);
            skriv('IKJ56250I JOB ' + jobbnamn(j) + ' SUBMITTED\n');
            klockor.push(setTimeout(() => {
                const m = sd_klocka(new Date(), '.') + ' JOB' + String(j.nr).padStart(5, '0') +
                          ' $HASP165 ' + j.namn.padEnd(8) + ' ENDED AT MVS1  MAXCC=0000';
                /* NOTIFY: direkt om terminalen väntar på en tom rad. */
                if (uppkopplad && nar_rad === kommando && rad === '' && pump === null)
                    skriv(m + '\n');
                else
                    vantande.push(m);
            }, 16000));
            return ready();
        }
        if (k === 'STATUS' || k === 'ST') {
            if (!jobb.length)
                skriv('IKJ56216I NO JOBS FOUND\n');
            for (const j of jobb) {
                const l = jobbets_lage(j);
                skriv((l === 'vantar' ? 'IKJ56197I JOB ' + jobbnamn(j) + ' WAITING FOR EXECUTION'
                      : l === 'kor' ? 'IKJ56211I JOB ' + jobbnamn(j) + ' EXECUTING'
                      : 'IKJ56192I JOB ' + jobbnamn(j) + ' ON OUTPUT QUEUE') + '\n');
            }
            return ready();
        }
        if (k === 'OUTPUT' || k === 'OUT') {
            const namn = (rest.split(/[\s(]/)[0]) || anv + 'A';
            const j = [...jobb].reverse().find(j => j.namn === namn);
            if (!j) {
                skriv('IKJ56328I JOB ' + namn + ' REJECTED - JOBNAME NOT FOUND\n');
                return ready();
            }
            if (jobbets_lage(j) !== 'klar') {
                skriv('IKJ56191I JOB ' + jobbnamn(j) + ' NOT ON OUTPUT QUEUE\n');
                return ready();
            }
            return skriv_rader(utskrift(j));
        }
        if (k === 'LOGOFF') {
            skriv('IKJ56470I ' + anv + ' LOGGED OFF TSO AT ' + sd_klocka(new Date()) +
                  ' ON ' + sd_datum(new Date()) + '\n\n(DATACENTRALEN LÄGGER PÅ.)\n');
            return lagg_pa();
        }
        skriv('IKJ56500I COMMAND ' + k + ' NOT FOUND\n');
        ready();
    }

    function ring() {
        lagg_pa();
        uppkopplad = true;
        start = Date.now();
        jobb = [];
        vantande = [];
        sand([0x1A]);
        skriv('DATACENTRALEN   MVS/TSO   LINJE 12   300 BIT/S\n' +
              'EN REKONSTRUKTION: ANLÄGGNINGEN HAR INTE FUNNITS, OCH ALLT HÄR\n' +
              'ÄR PÅHITTAT. VILKEN ANVÄNDARE OCH VILKET LÖSENORD SOM HELST GÅR BRA.\n\n' +
              'IKJ56700A ENTER USERID -\n');
        las(function nar_anv(n) {
            n = n.trim().toUpperCase().replace(/[^A-Z0-9]/g, '').slice(0, 7);
            if (!n || !/^[A-Z]/.test(n)) {
                skriv('IKJ56701I INVALID USERID\nIKJ56700A ENTER USERID -\n');
                return las(nar_anv);
            }
            anv = n;
            filer = sd_filer(anv);
            skriv('IKJ56714A ENTER CURRENT PASSWORD FOR ' + anv + '-\n');
            las(() => {
                const nu = new Date();
                skriv('IKJ56455I ' + anv + ' LOGON IN PROGRESS AT ' + sd_klocka(nu) +
                      ' ON ' + sd_datum(nu) + '\n' +
                      'NO BROADCAST MESSAGES\n' +
                      'VÄLKOMMEN. LIST LASMIG.TEXT BERÄTTAR VAD SOM FINNS.\n');
                ready();
            }, true);
        });
    }

    function lagg_pa() {
        stoppa_pumpen();
        for (const k of klockor)
            clearTimeout(k);
        klockor = [];
        uppkopplad = false;
        nar_rad = null;
    }

    function tecken(b) {
        b &= 0x7F;
        if (!uppkopplad || b === 0x11 || b === 0x13)
            return;
        if (b === 0x03) {                    /* BREAK: attention */
            if (pump !== null) {
                stoppa_pumpen();
                skriv('\n');
                return ready();
            }
            if (nar_rad === kommando) {
                skriv('\n');
                return ready();
            }
            return;
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
                if (!dold)
                    sand([0x08, 0x20, 0x08]);
            }
            return;
        }
        if (b < 0x20 || rad.length >= 72)
            return;
        rad += SD_TILLBAKA[b] || String.fromCharCode(b);
        if (!dold)                           /* lösenordet syns inte */
            sand([b]);
    }

    return { ring, lagg_pa, tecken, uppkopplad: () => uppkopplad };
}

if (typeof module !== 'undefined')
    module.exports = { skapa_stordator };
