/*
 * hr80.c -- ett grafikkort på V24 i HR-80:s anda, till ABC80 inifrån
 *
 * Utanför boken; se hr80.h. Koderna och deras namn är annonsens (Liber
 * 1984, emulator/färg-abc80.pdf):
 *
 *     0 null                  16 semigrafik, sammanhängande (teledata)
 *     1 välj färg             17 semigrafik, åtskild
 *     2 sätt punkt (x, y)     18 markören (y+32, x+32)
 *     4 linje (x, y)          19 absolut adressering (y, x)
 *     5 fyll (x, y)           20 förgrundsfärg   21 bakgrundsfärg
 *     8 vänster  9 höger      22 standard        24 rensa raden
 *    10 ned     11 upp        25 markören relativt
 *    12 rensa skärmen         27 ESC
 *    13 vagnretur             28 fast punkt (x, y)
 *    14 markören på           29 punkt relativt (x, y)
 *    15 markören av           30 linje relativt  31 fyll relativt
 *    32-127 tecken            128-253 kör ett makro
 *
 *     ESC 61 markören (rad, kolumn)    ESC 48-51 färgekvivalens
 *     ESC 73 initiera om               ESC 52 40 tecken (färg)
 *     ESC 82 reset                     ESC 56 80 tecken (svartvitt)
 *     ESC 68 kod 66 (bitmapp) ... 254, ESC 68 kod 82 (instruktioner) ... 254
 *
 * Koderna 3, 6, 7, 23, 26 och 255 gör ingenting. Annonsen nämner också
 * understrykning, dubbel höjd och omvänd video, men inte med vilka
 * koder; de finns inte här.
 *
 * Gissat, eftersom manualen inte är funnen:
 * - Argumenten är hela byte: x och y 0-255, de relativa -128...127
 *   (tvåkomplement); bara kod 18 har +32 i annonsen, och ESC 61 får det
 *   också. y = 0 är nederst, som i ett diagram. I det svartvita läget är
 *   grafikens x dubblat (0-255 över 512 punkter); texten använder alla.
 * - Färgerna är ABC 800 C:s. Bildskärmen ABC 810 får signalerna R, G och
 *   B av och på (ABC 800, bruksanvisningen 2.3), så de åtta är 0 svart,
 *   1 röd, 2 grön, 3 gul, 4 blå, 5 magenta, 6 cyan, 7 vit, med R som bit
 *   0, G bit 1 och B bit 2; det är också text-tv:ns och ABC 800:s
 *   färgkoder för text (BASIC II-manualen 11.1). Grafiken där visar fyra
 *   av dem, valda med FGCTL ur en färgvalstabell (manualen 12.4,
 *   s. 86-88). ESC 48-51 gissas sätta färg 0-3 var för sig, så att varje
 *   rad i tabellen går att få, också de med två lika färger som
 *   animationen använder (12.3). Från början 0 svart, 1 röd, 2 grön,
 *   3 vit, som FGCTL 6 (134). Ritfärg och förgrund 3, bakgrund 0.
 * - Texten: rutor om 6 x 10 punkter med ABC80:s teckensnitt, 40 eller 80
 *   per rad och 24 rader, mitt på bilden. 19 sätter rutans övre vänstra
 *   hörn i punkten (x, y), så att text kan stå var som helst. 25 flyttar
 *   (rader, kolumner). Längst ned rullar hela bilden upp en rad.
 * - 16 och 17 ritar 32-63 och 96-127 som text-tv:ns mosaik (2 x 3 block)
 *   i förgrundsfärgen, tills 22 ("standard"), som också återställer
 *   färgerna.
 * - Fyllningen byter den sammanhängande ytan med startpunktens färg mot
 *   ritfärgen.
 * - ESC 73 går till 40 tecken, standardfärgerna och en ren skärm; makrona
 *   är kvar. ESC R tar bort dem också.
 * - Makron: 254 avslutar alltid en definition. En bitmapp börjar med
 *   bredden, sedan raderna uppifrån med 7 punkter per byte (bit 6 till
 *   vänster, så att 254 aldrig förekommer); den ritas i ritfärgen med
 *   övre vänstra hörnet i den fasta punkten. Ett makro med instruktioner
 *   tolkas som om tecknen kom på V24. Makrominnet frigörs vid ESC R.
 *
 * Fönstren är påhittade (utanfor/fonster/PLAN.md, steg 2), i koder som
 * annonsen inte använder och med samma bokstäver som FONSTER.BAS:
 *
 *     ESC 79 (O) n r k h b titel 13   öppnar fönster n (0-7) på textrad
 *                                     r, kolumn k, h rader högt och b
 *                                     tecken brett med ramen, överst och
 *                                     valt; talen + 32
 *     ESC 86 (V) n    väljer fönster n och lyfter det överst
 *     ESC 84 (T) n    väljer fönster n utan att lyfta det; 8 skrivbordet
 *     ESC 83 (S) n    stänger fönster n
 *
 * Ramen är ett streck runt fönstret, med en titelrad överst och en
 * stängningsruta till höger i den; det översta fönstrets titelrad är
 * fylld. Ramen tar en textruta på varje sida, som i FONSTER.BAS, så att
 * de b tecknen ger b - 1 kolumner text och h rader ger h - 2. Efter V
 * eller T gäller allt i det valda fönstret: grafikens (0, 0) är dess
 * nedre vänstra hörn innanför ramen, 18 och ESC 61 räknar raderna och
 * kolumnerna innanför den, FF och 24 rensar bara där, texten bryts och
 * rullar där, och allt kapas vid ramen och vid fönstren ovanför. Så kan
 * ett program ritas i ett fönster utan ändringar. Skrivbordet är som
 * kortet utan fönster, men kapas vid fönstren.
 *
 * Inget sparas under fönstren: minnet räcker inte (16 KB bild, 2 KB
 * för resten). Ett fönster som stängs lämnar skrivbordets bakgrund och
 * fönstren under ritade med tom insida; ett fönster som lyfts får tom
 * insida där det var täckt. omrita får en bit för varje fönster som
 * tappade innehåll (8 för skrivbordet), så att ABC80 kan rita om det.
 *
 * Musen (påhittad, steg 3) sitter på kortet och ger rörelser och en
 * knapp, som Ataris styrspak. Pekaren ritas ovanpå bilden när den visas,
 * inte i bildminnet; på ett riktigt kort en överlagring i
 * bildutmatningen. Ett tryck på knappen
 *   - i stängningsrutan stänger fönstret, som ESC S;
 *   - i titelraden lyfter fönstret, och det dras medan knappen hålls,
 *     med en streckad ram ovanpå bilden; där knappen släpps flyttas det,
 *     i hela textrutor. Det översta fönstret syns helt, så innehållet
 *     flyttas med i bildminnet och behöver inte ritas om;
 *   - i ett fönster lyfter det och ger ett klick, på skrivbordet bara
 *     klicket.
 * Det valda fönstret byts inte: ABC80 märker inget förrän det läser
 * kön. Händelserna är fyra byte, slaget, fönstret och två tal:
 *     'K' n x y   klick i fönster n (8 skrivbordet), grafikens x och y i det
 *     'L' n 0 0   fönster n lyftes
 *     'F' n r k   fönster n flyttades till textrad r, kolumn k
 *     'S' n 0 0   fönster n stängdes
 * Kön rymmer 16; blir den full tappas de nya.
 *
 * Fönstertabellen och kön räknas till 384 byte av de 2 KB: nio poster
 * om 32 (fem byte läge och storlek i rutor, fyra markören och den fasta
 * punkten, färgerna, 16 titeln), ordningen, kön om 64 och tillståndet.
 * Makrona får resten, 1 664 byte. ESC I, ESC R, ESC 4 och ESC 8 stänger
 * alla.
 */

#include <string.h>

#include "hr80.h"
#include "teckensnitt.h"

static const uint8_t standardpalett[4] = { 0, 1, 2, 7 };   /* FGCTL 6 */

static int bredd(const Hr80 *h) { return h->svartvit ? 512 : 256; }
static int kolumner(const Hr80 *h) { return h->svartvit ? 80 : 40; }
static int text_x0(const Hr80 *h) { return (bredd(h) - kolumner(h) * 6) / 2; }
#define TEXT_Y0 8

/* Grafikens koordinater (y uppåt) i den valda ytan till bildminnets
   (y nedåt). */
static int gx(const Hr80 *h, int x) { return h->gx0 + (h->svartvit ? 2 * x : x); }
static int gy(const Hr80 *h, int y) { return h->gy1 - 1 - y; }

static int las_punkt(const Hr80 *h, int x, int y)
{
    if (h->svartvit)
        return h->minne[y * 64 + x / 8] >> (7 - x % 8) & 1;
    return h->minne[y * 64 + x / 4] >> (6 - 2 * (x % 4)) & 3;
}

static void satt_punkt(Hr80 *h, int x, int y, int farg)
{
    if (x < 0 || y < 0 || x >= bredd(h) || y >= 256)
        return;
    if (h->svartvit) {
        uint8_t *p = &h->minne[y * 64 + x / 8], bit = (uint8_t)(0x80 >> x % 8);
        *p = farg & 1 ? *p | bit : *p & ~bit;
    } else {
        uint8_t *p = &h->minne[y * 64 + x / 4];
        int skift = 6 - 2 * (x % 4);
        *p = (uint8_t)((*p & ~(3 << skift)) | (farg & 3) << skift);
    }
}

static int i_fonster(const Hr80Fonster *f, int x, int y)
{
    return x >= f->x && x < f->x + f->b && y >= f->y && y < f->y + f->hojd;
}

/* Om punkten hör till ytan som ritas i: inom klippet och inte under
 * ett fönster ovanför. */
static int synlig(const Hr80 *h, int x, int y)
{
    if (x < h->kx0 || y < h->ky0 || x >= h->kx1 || y >= h->ky1)
        return 0;
    for (int i = h->kniva + 1; i < h->antal_oppna; i++)
        if (i_fonster(&h->fonster[h->ordning[i]], x, y))
            return 0;
    return 1;
}

static void rita_punkt(Hr80 *h, int x, int y, int farg)
{
    if (synlig(h, x, y))
        satt_punkt(h, x, y, farg);
}

static void fyll_ruta(Hr80 *h, int x0, int y0, int x1, int y1, int farg)
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            rita_punkt(h, x, y, farg);
}

/* Fönster n:s plats i ordningen, -1 om det är stängt. */
static int plats(const Hr80 *h, int n)
{
    for (int i = 0; i < h->antal_oppna; i++)
        if (h->ordning[i] == n)
            return i;
    return -1;
}

/* Klippet till rektangeln, med fönstren ovanför plats kniva. */
static void klipp(Hr80 *h, int x0, int y0, int x1, int y1, int kniva)
{
    h->kx0 = x0 < 0 ? 0 : x0;
    h->ky0 = y0 < 0 ? 0 : y0;
    h->kx1 = x1 > bredd(h) ? bredd(h) : x1;
    h->ky1 = y1 > 256 ? 256 : y1;
    h->kniva = kniva;
}

/* Ytan efter det valda fönstret. */
static void yta(Hr80 *h)
{
    if (h->vald == HR80_SKRIVBORDET) {
        klipp(h, 0, 0, bredd(h), 256, -1);
        h->gx0 = 0; h->gy1 = 256;
        h->tx0 = text_x0(h); h->ty0 = TEXT_Y0;
        h->tkol = kolumner(h);
        h->tymax = 256 - 10;
        return;
    }
    const Hr80Fonster *f = &h->fonster[h->vald];
    klipp(h, f->x + 1, f->y + 11, f->x + f->b - 1, f->y + f->hojd - 1,
          plats(h, h->vald));
    h->gx0 = f->x + 1; h->gy1 = f->y + f->hojd - 1;
    h->tx0 = f->x + 2; h->ty0 = f->y + 12;
    h->tkol = f->b / 6 - 1;
    h->tymax = f->y + f->hojd - 11;
}

/* Byter det valda; markören och den fasta punkten följer fönstret. */
static void valj(Hr80 *h, int n)
{
    Hr80Fonster *f = &h->fonster[h->vald];
    f->tx = (int16_t)h->tx; f->ty = (int16_t)h->ty;
    f->px = (int16_t)h->px; f->py = (int16_t)h->py;
    h->vald = n;
    f = &h->fonster[n];
    h->tx = f->tx; h->ty = f->ty;
    h->px = f->px; h->py = f->py;
    yta(h);
}

/* En byte där alla punkterna har färgen. */
static uint8_t fylld_byte(const Hr80 *h, int farg)
{
    if (h->svartvit)
        return farg & 1 ? 0xFF : 0x00;
    return (uint8_t)((farg & 3) * 0x55);
}

/* FF: den valda ytan i bakgrundsfärgen och markören i dess hörn. */
static void rensa(Hr80 *h)
{
    if (h->antal_oppna == 0)
        memset(h->minne, fylld_byte(h, h->bakgrund), HR80_MINNE);
    else
        fyll_ruta(h, h->kx0, h->ky0, h->kx1, h->ky1, h->bakgrund);
    if (h->vald == HR80_SKRIVBORDET)
        h->skrivbord = (uint8_t)h->bakgrund;
    else
        h->fonster[h->vald].bak = (uint8_t)h->bakgrund;
    h->tx = h->tx0;
    h->ty = h->ty0;
}

static void linje(Hr80 *h, int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int fel = dx + dy;
    for (;;) {
        rita_punkt(h, x0, y0, h->farg);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * fel;
        if (e2 >= dy) { fel += dy; x0 += sx; }
        if (e2 <= dx) { fel += dx; y0 += sy; }
    }
}

/* Fyllningen rad för rad med en egen stack. Blir den full tappas
 * resten av ytan; 4 096 sträckor räcker för vanliga figurer. Kanten på
 * ytan som ritas i stoppar den som en annan färg. */
#define FYLLSTACK 4096
static struct { int16_t x, y; } fyllstack[FYLLSTACK];

static int fyllbar(const Hr80 *h, int x, int y, int gammal)
{
    return synlig(h, x, y) && las_punkt(h, x, y) == gammal;
}

static void fyll(Hr80 *h, int x, int y)
{
    if (!synlig(h, x, y))
        return;
    int gammal = las_punkt(h, x, y), ny = h->svartvit ? h->farg & 1 : h->farg & 3;
    if (gammal == ny)
        return;
    int n = 0;
    fyllstack[n].x = (int16_t)x; fyllstack[n].y = (int16_t)y; n++;
    while (n > 0) {
        n--;
        x = fyllstack[n].x; y = fyllstack[n].y;
        if (las_punkt(h, x, y) != gammal)
            continue;
        int v = x, hg = x;
        while (fyllbar(h, v - 1, y, gammal))
            v--;
        while (fyllbar(h, hg + 1, y, gammal))
            hg++;
        for (int i = v; i <= hg; i++)
            satt_punkt(h, i, y, ny);
        for (int r = y - 1; r <= y + 1; r += 2) {
            int i = v;
            while (i <= hg) {
                if (fyllbar(h, i, r, gammal) && n < FYLLSTACK) {
                    fyllstack[n].x = (int16_t)i; fyllstack[n].y = (int16_t)r; n++;
                    while (i <= hg && fyllbar(h, i, r, gammal))
                        i++;
                } else
                    i++;
            }
        }
    }
}

/* Ytan upp en textrad; den nya raden längst ned i bakgrunden. Utan
 * fönster flyttas hela bilden. Det som kommer fram under ett fönster
 * ovanför finns inte, och blir bakgrund. */
static void rulla(Hr80 *h)
{
    h->ty -= 10;
    if (h->antal_oppna == 0) {
        memmove(h->minne, h->minne + 10 * 64, (256 - 10) * 64);
        memset(h->minne + (256 - 10) * 64, fylld_byte(h, h->bakgrund), 10 * 64);
        return;
    }
    for (int y = h->ky0; y < h->ky1; y++)
        for (int x = h->kx0; x < h->kx1; x++) {
            if (!synlig(h, x, y))
                continue;
            int farg = h->bakgrund;
            if (y + 10 < h->ky1) {
                if (synlig(h, x, y + 10))
                    farg = las_punkt(h, x, y + 10);
                else
                    h->omrita |= (uint16_t)(1 << h->vald);
            }
            satt_punkt(h, x, y, farg);
        }
}

static void ny_rad(Hr80 *h)
{
    h->tx = h->tx0;
    h->ty += 10;
    while (h->ty > h->tymax)
        rulla(h);
}

/* Tecknets tio rader, sex punkter i de höga bitarna. */
static void teckenruta(const Hr80 *h, uint8_t kod, uint8_t ruta[10])
{
    memset(ruta, 0, 10);
    if (h->semigrafik && (kod < 0x40 || kod >= 0x60)) {
        static const int rad_start[4] = { 0, 3, 7, 10 };
        int block = (kod & 0x1F) | (kod & 0x40) >> 1;
        for (int r = 0; r < 3; r++)
            for (int y = rad_start[r]; y < rad_start[r + 1]; y++) {
                uint8_t v = block >> 2 * r & 1 ? 0xE0 : 0, hg = block >> 2 * r & 2 ? 0x1C : 0;
                if (h->semigrafik == 2) {          /* åtskild: luft till höger och under */
                    v &= 0xC0; hg &= 0x18;
                    if (y == rad_start[r + 1] - 1)
                        v = hg = 0;
                }
                ruta[y] = v | hg;
            }
    } else if (kod >= 0x20 && kod < 0x7F) {
        for (int y = 0; y < 7; y++)
            ruta[y] = teckensnitt[kod - 0x20][y];
        ruta[7] = teckensnitt_under[kod][0];
        ruta[8] = teckensnitt_under[kod][1];
    }
}

static void rita_ruta(Hr80 *h, int x0, int y0, const uint8_t *ruta, int rader,
                      int forgrund, int bakgrund)
{
    for (int y = 0; y < rader; y++)
        for (int x = 0; x < 6; x++)
            rita_punkt(h, x0 + x, y0 + y, ruta[y] & 0x80 >> x ? forgrund : bakgrund);
}

/* Ett tecken (32-127) i rutan vid markören, sedan ett steg åt höger. */
static void rita_tecken(Hr80 *h, uint8_t kod)
{
    uint8_t ruta[10];
    teckenruta(h, kod, ruta);
    rita_ruta(h, h->tx, h->ty, ruta, 10, h->forgrund, h->bakgrund);
    h->tx += 6;
    if (h->tx > h->tx0 + (h->tkol - 1) * 6)
        ny_rad(h);
}

/* En bitmapp ur ett makro: bredden, sedan 7 punkter per byte. */
static void rita_bitmapp(Hr80 *h, const uint8_t *data, int langd)
{
    if (langd < 1 || data[0] == 0)
        return;
    int b = data[0], per_rad = (b + 6) / 7;
    int x0 = gx(h, h->px), y0 = gy(h, h->py);
    for (int i = 1, rad = 0; i + per_rad <= langd; i += per_rad, rad++)
        for (int x = 0; x < b; x++)
            if (data[i + x / 7] & 0x40 >> x % 7) {
                rita_punkt(h, x0 + (h->svartvit ? 2 * x : x), y0 + rad, h->farg);
                if (h->svartvit)
                    rita_punkt(h, x0 + 2 * x + 1, y0 + rad, h->farg);
            }
}

/* Fönster n:s ram inom rektangeln, kapad vid fönstren ovanför; med
 * inre också insidan i bakgrundsfärgen. Ytan måste sättas efteråt. */
static void rita_fonster(Hr80 *h, int n, int x0, int y0, int x1, int y1, int inre)
{
    const Hr80Fonster *f = &h->fonster[n];
    int x = f->x, y = f->y, b = f->b, hj = f->hojd;
    int aktiv = h->antal_oppna > 0 && h->ordning[h->antal_oppna - 1] == n;
    int text = aktiv ? f->bak : f->ram, titel = aktiv ? f->ram : f->bak;
    klipp(h, x0 > x ? x0 : x, y0 > y ? y0 : y,
          x1 < x + b ? x1 : x + b, y1 < y + hj ? y1 : y + hj, plats(h, n));

    fyll_ruta(h, x, y, x + b, y + 1, f->ram);                 /* ramen */
    fyll_ruta(h, x, y + hj - 1, x + b, y + hj, f->ram);
    fyll_ruta(h, x, y, x + 1, y + hj, f->ram);
    fyll_ruta(h, x + b - 1, y, x + b, y + hj, f->ram);
    fyll_ruta(h, x + 1, y + 10, x + b - 1, y + 11, f->ram);   /* under titeln */
    fyll_ruta(h, x + 1, y + 1, x + b - 1, y + 10, titel);
    for (int i = 0; f->titel[i] && i < (b - 14) / 6; i++) {
        uint8_t ruta[10];
        teckenruta(h, (uint8_t)f->titel[i], ruta);
        rita_ruta(h, x + 3 + 6 * i, y + 1, ruta, 9, text, titel);
    }
    int sx = x + b - 10, sy = y + 2;                          /* stängningsrutan */
    for (int i = 0; i < 7; i++) {
        rita_punkt(h, sx + i, sy, text);
        rita_punkt(h, sx + i, sy + 6, text);
        rita_punkt(h, sx, sy + i, text);
        rita_punkt(h, sx + 6, sy + i, text);
        if (i >= 2 && i <= 4) {
            rita_punkt(h, sx + i, sy + i, text);
            rita_punkt(h, sx + i, sy + 6 - i, text);
        }
    }
    if (inre)
        fyll_ruta(h, x + 1, y + 11, x + b - 1, y + hj - 1, f->bak);
}

static void hela_fonstret(Hr80 *h, int n, int inre)
{
    const Hr80Fonster *f = &h->fonster[n];
    rita_fonster(h, n, f->x, f->y, f->x + f->b, f->y + f->hojd, inre);
}

static int snittar(const Hr80Fonster *a, const Hr80Fonster *b)
{
    return a->x < b->x + b->b && b->x < a->x + a->b &&
           a->y < b->y + b->hojd && b->y < a->y + a->hojd;
}

static void stang(Hr80 *h, int n)
{
    int p = plats(h, n);
    if (p < 0)
        return;
    Hr80Fonster *f = &h->fonster[n];
    int var_overst = p == h->antal_oppna - 1;
    memmove(h->ordning + p, h->ordning + p + 1, (size_t)(h->antal_oppna - p - 1));
    h->antal_oppna--;
    f->oppen = 0;
    if (h->drar == n)
        h->drar = -1;

    /* Det under: skrivbordet och fönstren nerifrån och upp, med tom
       insida inom rutan. */
    klipp(h, f->x, f->y, f->x + f->b, f->y + f->hojd, -1);
    fyll_ruta(h, f->x, f->y, f->x + f->b, f->y + f->hojd, h->skrivbord);
    h->omrita |= 1 << HR80_SKRIVBORDET;
    for (int i = 0; i < h->antal_oppna; i++) {
        int m = h->ordning[i];
        if (!snittar(&h->fonster[m], f))
            continue;
        rita_fonster(h, m, f->x, f->y, f->x + f->b, f->y + f->hojd, 1);
        h->omrita |= (uint16_t)(1 << m);
    }
    if (var_overst && h->antal_oppna > 0)
        hela_fonstret(h, h->ordning[h->antal_oppna - 1], 0);
    if (h->vald == n)
        valj(h, h->antal_oppna > 0 ? h->ordning[h->antal_oppna - 1] : HR80_SKRIVBORDET);
    else
        yta(h);
}

static void lyft(Hr80 *h, int n)
{
    int p = plats(h, n);
    if (p < 0 || p == h->antal_oppna - 1)
        return;
    int forra = h->ordning[h->antal_oppna - 1];
    uint8_t ovanfor[HR80_FONSTER];
    int antal = h->antal_oppna - p - 1;
    memcpy(ovanfor, h->ordning + p + 1, (size_t)antal);
    memmove(h->ordning + p, h->ordning + p + 1, (size_t)antal);
    h->ordning[h->antal_oppna - 1] = (uint8_t)n;

    /* Överst nu: det som låg under de andra blir tomt. */
    Hr80Fonster *f = &h->fonster[n];
    for (int i = 0; i < antal; i++) {
        const Hr80Fonster *o = &h->fonster[ovanfor[i]];
        if (!snittar(o, f))
            continue;
        rita_fonster(h, n, o->x, o->y, o->x + o->b, o->y + o->hojd, 1);
        h->omrita |= (uint16_t)(1 << n);
    }
    hela_fonstret(h, forra, 0);                 /* titelraden blir ofylld */
    hela_fonstret(h, n, 0);
    yta(h);
}

/* ESC O: läge och storlek; titeln kommer efter, och ramen ritas när
 * den har kommit. */
static void oppna(Hr80 *h, int n, int r, int k, int hj, int b)
{
    if (h->fonster[n].oppen)
        stang(h, n);
    int kol = kolumner(h);
    if (hj < 3) hj = 3;
    if (hj > 24) hj = 24;
    if (b < 3) b = 3;
    if (b > kol) b = kol;
    if (r < 0) r = 0;
    if (r + hj > 24) r = 24 - hj;
    if (k < 0) k = 0;
    if (k + b > kol) k = kol - b;
    Hr80Fonster *f = &h->fonster[n];
    memset(f, 0, sizeof *f);
    f->oppen = 1;
    f->x = (int16_t)(text_x0(h) + k * 6); f->y = (int16_t)(TEXT_Y0 + r * 10);
    f->b = (int16_t)(b * 6); f->hojd = (int16_t)(hj * 10);
    f->ram = (uint8_t)h->forgrund; f->bak = (uint8_t)h->bakgrund;
    f->tx = (int16_t)(f->x + 2); f->ty = (int16_t)(f->y + 12);
    h->titel_till = n;
}

static void oppna_klart(Hr80 *h, int n)
{
    int forra = h->antal_oppna > 0 ? h->ordning[h->antal_oppna - 1] : -1;
    h->ordning[h->antal_oppna++] = (uint8_t)n;
    hela_fonstret(h, n, 1);
    if (forra >= 0)
        hela_fonstret(h, forra, 0);
    valj(h, n);
}

static void ko_lagg(Hr80 *h, int slag, int n, int a, int b)
{
    if (h->ko_antal == HR80_KO)
        return;
    uint8_t *e = h->ko[(h->ko_forsta + h->ko_antal++) % HR80_KO];
    e[0] = (uint8_t)slag; e[1] = (uint8_t)n; e[2] = (uint8_t)a; e[3] = (uint8_t)b;
}

int hr80_handelse(Hr80 *h, uint8_t ut[4])
{
    if (h->ko_antal == 0)
        return 0;
    memcpy(ut, h->ko[h->ko_forsta], 4);
    h->ko_forsta = (h->ko_forsta + 1) % HR80_KO;
    h->ko_antal--;
    return 1;
}

/* Det översta fönstret n till ytterkanten (x, y). Det syns helt, så
 * punkterna flyttas i bildminnet, i en ordning som inte skriver över det
 * som ska flyttas; det som blir synligt där det var ritas som vid
 * stängning. */
static void flytta(Hr80 *h, int n, int x, int y)
{
    Hr80Fonster *f = &h->fonster[n], g = *f;
    int dx = x - f->x, dy = y - f->y;
    if (dx == 0 && dy == 0)
        return;
    for (int j = 0; j < f->hojd; j++) {
        int r = dy > 0 ? f->hojd - 1 - j : j;
        for (int i = 0; i < f->b; i++) {
            int k = dx > 0 ? f->b - 1 - i : i;
            satt_punkt(h, x + k, y + r, las_punkt(h, f->x + k, f->y + r));
        }
    }
    f->x = (int16_t)x; f->y = (int16_t)y;
    f->tx = (int16_t)(f->tx + dx); f->ty = (int16_t)(f->ty + dy);
    if (h->vald == n) {
        h->tx += dx; h->ty += dy;
    }
    klipp(h, g.x, g.y, g.x + g.b, g.y + g.hojd, -1);
    fyll_ruta(h, g.x, g.y, g.x + g.b, g.y + g.hojd, h->skrivbord);
    h->omrita |= 1 << HR80_SKRIVBORDET;
    for (int i = 0; i < h->antal_oppna - 1; i++) {
        int m = h->ordning[i];
        if (!snittar(&h->fonster[m], &g))
            continue;
        rita_fonster(h, m, g.x, g.y, g.x + g.b, g.y + g.hojd, 1);
        h->omrita |= (uint16_t)(1 << m);
    }
    yta(h);
}

/* Det översta fönstret under punkten (bildminnets), -1 för skrivbordet. */
static int fonster_vid(const Hr80 *h, int x, int y)
{
    for (int i = h->antal_oppna - 1; i >= 0; i--)
        if (i_fonster(&h->fonster[h->ordning[i]], x, y))
            return h->ordning[i];
    return -1;
}

/* Hörnet där fönstret som dras hamnar: hela rutor, på bilden. */
static void drag_hamnar(Hr80 *h)
{
    const Hr80Fonster *f = &h->fonster[h->drar];
    int x0 = text_x0(h), mx = h->svartvit ? h->mx : h->mx / 2;
    int k = (mx - h->drag_dx - x0 + 3) / 6, r = (h->my - h->drag_dy - TEXT_Y0 + 5) / 10;
    if (mx - h->drag_dx - x0 + 3 < 0) k = 0;
    if (h->my - h->drag_dy - TEXT_Y0 + 5 < 0) r = 0;
    if (k + f->b / 6 > kolumner(h)) k = kolumner(h) - f->b / 6;
    if (r + f->hojd / 10 > 24) r = 24 - f->hojd / 10;
    h->drag_x = x0 + k * 6;
    h->drag_y = TEXT_Y0 + r * 10;
}

static void tryck(Hr80 *h)
{
    int x = h->svartvit ? h->mx : h->mx / 2, y = h->my;
    int n = fonster_vid(h, x, y);
    if (n < 0) {
        ko_lagg(h, 'K', HR80_SKRIVBORDET, h->svartvit ? x / 2 : x, 255 - y);
        return;
    }
    const Hr80Fonster *f = &h->fonster[n];
    if (x >= f->x + f->b - 10 && x < f->x + f->b - 3 && y >= f->y + 2 && y < f->y + 9) {
        stang(h, n);
        ko_lagg(h, 'S', n, 0, 0);
        return;
    }
    if (plats(h, n) != h->antal_oppna - 1) {
        lyft(h, n);
        ko_lagg(h, 'L', n, 0, 0);
    }
    if (y < f->y + 11) {
        h->drar = n;
        h->drag_dx = x - f->x;
        h->drag_dy = y - f->y;
        drag_hamnar(h);
        return;
    }
    int gx = (x - (f->x + 1)) / (h->svartvit ? 2 : 1), gyv = f->y + f->hojd - 2 - y;
    ko_lagg(h, 'K', n, gx < 0 ? 0 : gx, gyv < 0 ? 0 : gyv);
}

void hr80_mus(Hr80 *h, int dx, int dy, int knapp)
{
    h->mus_pa = 1;
    h->mx += dx; h->my += dy;
    if (h->mx < 0) h->mx = 0;
    if (h->mx >= HR80_BREDD) h->mx = HR80_BREDD - 1;
    if (h->my < 0) h->my = 0;
    if (h->my >= HR80_HOJD) h->my = HR80_HOJD - 1;
    knapp = knapp != 0;
    if (h->drar >= 0)
        drag_hamnar(h);
    if (knapp && !h->knapp)
        tryck(h);
    else if (!knapp && h->knapp && h->drar >= 0) {
        int n = h->drar;
        h->drar = -1;
        const Hr80Fonster *f = &h->fonster[n];
        if (f->oppen && (h->drag_x != f->x || h->drag_y != f->y)) {
            flytta(h, n, h->drag_x, h->drag_y);
            ko_lagg(h, 'F', n, (f->y - TEXT_Y0) / 10, (f->x - text_x0(h)) / 6);
        }
    }
    h->knapp = knapp;
}

static void stang_alla(Hr80 *h)
{
    for (int i = 0; i <= HR80_FONSTER; i++)
        h->fonster[i].oppen = 0;
    h->antal_oppna = 0;
    h->vald = HR80_SKRIVBORDET;
    h->titel_till = -1;
    h->omrita = 0;
    h->drar = -1;
    h->ko_antal = 0;
    yta(h);
}

static void standard(Hr80 *h)
{
    h->semigrafik = 0;
    h->farg = h->forgrund = 3;
    h->bakgrund = 0;
}

static void initiera(Hr80 *h)
{
    h->svartvit = 0;
    memcpy(h->palett, standardpalett, 4);
    standard(h);
    h->markor = 1;
    h->px = h->py = 0;
    h->kommando = 0;
    h->esc = 0;
    h->behover = h->antal_argument = 0;
    stang_alla(h);
    rensa(h);
}

void hr80_starta(Hr80 *h)
{
    memset(h, 0, sizeof *h);
    h->definierar = -1;
    initiera(h);
}

static int s8(uint8_t v) { return v < 128 ? v : v - 256; }

/* Kommandot med alla sina argument. */
static void utfor(Hr80 *h)
{
    const uint8_t *a = h->argument;
    switch (h->kommando) {
    case 1:  h->farg = a[0]; break;
    case 2:  h->px = a[0]; h->py = a[1];
             rita_punkt(h, gx(h, h->px), gy(h, h->py), h->farg); break;
    case 4:  linje(h, gx(h, h->px), gy(h, h->py), gx(h, a[0]), gy(h, a[1]));
             h->px = a[0]; h->py = a[1]; break;
    case 5:  fyll(h, gx(h, a[0]), gy(h, a[1])); break;
    case 18:
    case 256 + 61:
             h->ty = h->ty0 + (a[0] - 32) * 10;
             h->tx = h->tx0 + (a[1] - 32) * 6; break;
    case 19: h->ty = gy(h, a[0]); h->tx = gx(h, a[1]); break;
    case 20: h->forgrund = a[0]; break;
    case 21: h->bakgrund = a[0]; break;
    case 25: h->ty += s8(a[0]) * 10; h->tx += s8(a[1]) * 6; break;
    case 28: h->px = a[0]; h->py = a[1]; break;
    case 29: h->px += s8(a[0]); h->py += s8(a[1]);
             rita_punkt(h, gx(h, h->px), gy(h, h->py), h->farg); break;
    case 30: {
             int x = h->px + s8(a[0]), y = h->py + s8(a[1]);
             linje(h, gx(h, h->px), gy(h, h->py), gx(h, x), gy(h, y));
             h->px = x; h->py = y; break;
             }
    case 31: fyll(h, gx(h, h->px + s8(a[0])), gy(h, h->py + s8(a[1]))); break;
    case 256 + 48: case 256 + 49: case 256 + 50: case 256 + 51:
             h->palett[h->kommando - 256 - 48] = a[0] & 7; break;
    case 256 + 68: {
             /* Början på en definition; resten tas emot till 254. */
             int nr = a[0] - 128;
             h->definierar = nr >= 0 && nr < 126 ? nr : 126;   /* 126: kastas */
             h->makro_typ = a[1];
             if (h->definierar < 126) {
                 h->makro_start[nr] = h->makro_slut;
                 h->makro_langd[nr] = 0;
                 h->makro_slag[nr] = a[1];
             }
             break;
             }
    case 256 + 79: {
             int n = a[0] - 32;
             if (n >= 0 && n < HR80_FONSTER)
                 oppna(h, n, a[1] - 32, a[2] - 32, a[3] - 32, a[4] - 32);
             else
                 h->titel_till = HR80_FONSTER;     /* titeln kastas */
             break;
             }
    case 256 + 86: case 256 + 84: {
             int n = a[0] - 32;
             if (n == HR80_SKRIVBORDET)
                 valj(h, n);
             else if (n >= 0 && n < HR80_FONSTER && h->fonster[n].oppen) {
                 if (h->kommando == 256 + 86)
                     lyft(h, n);
                 valj(h, n);
             }
             break;
             }
    case 256 + 83: {
             int n = a[0] - 32;
             if (n >= 0 && n < HR80_FONSTER)
                 stang(h, n);
             break;
             }
    }
    h->kommando = 0;
}

/* Hur många argument kommandot tar; 0 för dem som utförs direkt. */
static int argument_till(int kommando)
{
    switch (kommando) {
    case 1: case 20: case 21:
    case 256 + 48: case 256 + 49: case 256 + 50: case 256 + 51:
    case 256 + 83: case 256 + 84: case 256 + 86:
        return 1;
    case 2: case 4: case 5: case 18: case 19: case 25:
    case 28: case 29: case 30: case 31:
    case 256 + 61: case 256 + 68:
        return 2;
    case 256 + 79:
        return 5;
    }
    return 0;
}

static void kor_makro(Hr80 *h, int nr)
{
    if (h->makro_langd[nr] == 0 || h->djup >= 8)
        return;
    const uint8_t *data = h->makrominne + h->makro_start[nr];
    int langd = h->makro_langd[nr];
    if (h->makro_slag[nr] == 66) {
        rita_bitmapp(h, data, langd);
        return;
    }
    h->djup++;
    for (int i = 0; i < langd; i++)
        hr80_tecken(h, data[i]);
    h->djup--;
}

/* ESC och koden efter. */
static void esc(Hr80 *h, uint8_t kod)
{
    switch (kod) {
    case 73: initiera(h); return;
    case 82: hr80_starta(h); return;
    case 52: h->svartvit = 0; stang_alla(h); rensa(h); return;
    case 56: h->svartvit = 1; stang_alla(h); rensa(h); return;
    }
    if (argument_till(256 + kod)) {
        h->kommando = 256 + kod;
        h->behover = argument_till(h->kommando);
        h->antal_argument = 0;
    }
}

void hr80_tecken(Hr80 *h, uint8_t tecken)
{
    /* En definition tar emot allt till 254. */
    if (h->definierar >= 0) {
        if (tecken == 254)
            h->definierar = -1;
        else if (h->definierar < 126) {
            if (h->makro_slut < HR80_MAKROMINNE) {
                h->makrominne[h->makro_slut++] = tecken;
                h->makro_langd[h->definierar]++;
            } else {
                h->makro_langd[h->definierar] = 0;     /* fullt: kastas */
                h->definierar = 126;
            }
        }
        return;
    }
    /* ESC O tar emot titeln till 13. */
    if (h->titel_till >= 0) {
        int n = h->titel_till;
        if (tecken == 13) {
            h->titel_till = -1;
            if (n < HR80_FONSTER)
                oppna_klart(h, n);
        } else if (n < HR80_FONSTER && tecken >= 32 && tecken < 127) {
            char *t = h->fonster[n].titel;
            size_t l = strlen(t);
            if (l < HR80_TITEL - 1)
                t[l] = (char)tecken;
        }
        return;
    }
    if (h->behover > 0) {
        h->argument[h->antal_argument++] = tecken;
        if (h->antal_argument == h->behover) {
            h->behover = 0;
            utfor(h);
        }
        return;
    }
    if (h->esc) {
        h->esc = 0;
        esc(h, tecken);
        return;
    }
    if (tecken >= 128) {
        if (tecken <= 253)
            kor_makro(h, tecken - 128);
        return;
    }
    if (tecken >= 32) {
        if (tecken < 127)
            rita_tecken(h, tecken);
        return;
    }
    if (argument_till(tecken)) {
        h->kommando = tecken;
        h->behover = argument_till(tecken);
        h->antal_argument = 0;
        return;
    }
    switch (tecken) {
    case 8:
        if (h->tx - 6 >= h->kx0)
            h->tx -= 6;
        break;
    case 9:
        h->tx += 6;
        if (h->tx > h->tx0 + (h->tkol - 1) * 6)
            ny_rad(h);
        break;
    case 10:
        h->ty += 10;
        while (h->ty > h->tymax)
            rulla(h);
        break;
    case 11:
        if (h->ty - 10 >= h->ky0)
            h->ty -= 10;
        break;
    case 12: rensa(h); break;
    case 13: h->tx = h->tx0; break;
    case 14: h->markor = 1; break;
    case 15: h->markor = 0; break;
    case 16: h->semigrafik = 1; break;
    case 17: h->semigrafik = 2; break;
    case 22: standard(h); break;
    case 24:
        fyll_ruta(h, h->kx0, h->ty, h->kx1, h->ty + 10, h->bakgrund);
        break;
    case 27: h->esc = 1; break;
    }
}

void hr80_rita(const Hr80 *h, uint32_t *punkter, const uint32_t farger[8],
               int markor_synlig)
{
    for (int y = 0; y < HR80_HOJD; y++)
        for (int x = 0; x < HR80_BREDD; x++)
            punkter[y * HR80_BREDD + x] = h->svartvit
                ? farger[las_punkt(h, x, y) ? 7 : 0]
                : farger[h->palett[las_punkt(h, x / 2, y)]];
    int skala = h->svartvit ? 1 : 2;
    if (h->markor && markor_synlig) {
        int y = h->ty + 9;
        uint32_t farg = h->svartvit ? farger[h->forgrund & 1 ? 7 : 0]
                                    : farger[h->palett[h->forgrund & 3]];
        for (int x = h->tx * skala; x < (h->tx + 6) * skala; x++)
            if (y >= 0 && y < HR80_HOJD && x >= 0 && x < HR80_BREDD)
                punkter[y * HR80_BREDD + x] = farg;
    }

    /* Fönstret som dras: en streckad ram, svart och vit. */
    if (h->drar >= 0) {
        const Hr80Fonster *f = &h->fonster[h->drar];
        int x0 = h->drag_x * skala, y0 = h->drag_y;
        int x1 = (h->drag_x + f->b) * skala - 1, y1 = h->drag_y + f->hojd - 1;
        for (int x = x0; x <= x1; x++) {
            punkter[y0 * HR80_BREDD + x] = farger[x / 4 % 2 ? 0 : 7];
            punkter[y1 * HR80_BREDD + x] = farger[x / 4 % 2 ? 0 : 7];
        }
        for (int y = y0; y <= y1; y++) {
            punkter[y * HR80_BREDD + x0] = farger[y / 2 % 2 ? 0 : 7];
            punkter[y * HR80_BREDD + x1] = farger[y / 2 % 2 ? 0 : 7];
        }
    }

    /* Pekaren, en pil med spetsen i (mx, my): vit med svart kant, två
       punkter bred per tecken nedan. */
    static const char *pil[11] = {
        "X", "XX", "X.X", "X..X", "X...X", "X....X",
        "X.....X", "X..XXXX", "X.X", "XX", "X"
    };
    if (!h->mus_pa)
        return;
    for (int r = 0; r < 11; r++)
        for (int k = 0; pil[r][k]; k++) {
            for (int x = h->mx + 2 * k, y = h->my + r; x < h->mx + 2 * k + 2; x++)
                if (x < HR80_BREDD && y < HR80_HOJD)
                    punkter[y * HR80_BREDD + x] = farger[pil[r][k] == 'X' ? 0 : 7];
        }
}
