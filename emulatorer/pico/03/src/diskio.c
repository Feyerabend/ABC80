// FatFS disk I/O — bit-bang SPI SD card on Pimoroni VGA Demo Base.
//
// Pin assignment (fixed by the board):
//   GPIO  5  CLK   — shared with VGA Green[0] (PIO0); claimed/released per op
//   GPIO 18  MOSI  — SD CMD
//   GPIO 19  MISO  — SD DAT0
//   GPIO 22  CS    — SD DAT3 (active-low)
//
// SD cards 1-3 (DAT1=GPIO20, DAT2=GPIO21) are unused in SPI mode.

#include "ff.h"
#include "diskio.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#define SD_CLK   5
#define SD_MOSI 18
#define SD_MISO 19
#define SD_CS   22

// ---- GPIO helpers ----------------------------------------------------------

static inline void clk_claim(void) {
    gpio_set_function(SD_CLK, GPIO_FUNC_SIO);
    gpio_set_dir(SD_CLK, GPIO_OUT);
    gpio_put(SD_CLK, 0);
}
static inline void clk_release(void) {
    gpio_set_function(SD_CLK, GPIO_FUNC_PIO0);
}
static inline void cs_lo(void) { gpio_put(SD_CS, 0); }
static inline void cs_hi(void) { gpio_put(SD_CS, 1); }

// ---- SPI bit-bang ----------------------------------------------------------

static uint8_t spi_byte(uint8_t d) {
    uint8_t r = 0;
    for (int i = 7; i >= 0; i--) {
        gpio_put(SD_MOSI, (d >> i) & 1);
        gpio_put(SD_CLK, 1);
        r = (uint8_t)((r << 1) | gpio_get(SD_MISO));
        gpio_put(SD_CLK, 0);
    }
    return r;
}

// Clock out 'n' 0xFF bytes (dummy / idle).
static inline void spi_ff(unsigned n) {
    while (n--) spi_byte(0xFF);
}

// ---- SD command / response -------------------------------------------------

// Send a 6-byte SD command (no CS manipulation).  Returns R1 response byte.
static uint8_t sd_cmd_raw(uint8_t cmd, uint32_t arg) {
    spi_byte((uint8_t)(0x40 | cmd));
    spi_byte((uint8_t)(arg >> 24));
    spi_byte((uint8_t)(arg >> 16));
    spi_byte((uint8_t)(arg >>  8));
    spi_byte((uint8_t)(arg      ));
    // CRC — only required for CMD0 and CMD8 in SPI mode
    uint8_t crc = 0xFF;
    if (cmd ==  0) crc = 0x95;
    if (cmd ==  8) crc = 0x87;
    spi_byte(crc);
    // NCR: wait up to 8 bytes for non-0xFF response
    uint8_t r = 0xFF;
    for (int i = 0; i < 8 && r == 0xFF; i++)
        r = spi_byte(0xFF);
    return r;
}

// Send command with CS assertion.  Releases CS after response.
static uint8_t sd_cmd(uint8_t cmd, uint32_t arg) {
    cs_lo();
    uint8_t r = sd_cmd_raw(cmd, arg);
    cs_hi();
    spi_ff(1);
    return r;
}

// ACMD = CMD55 + CMDn.
static uint8_t sd_acmd(uint8_t cmd, uint32_t arg) {
    sd_cmd(55, 0);
    return sd_cmd(cmd, arg);
}

// Wait for data token 0xFE (or error token 0x0x) with timeout.
// CS must be asserted by caller.  Returns 0 on success.
static int wait_token(void) {
    for (int i = 0; i < 100000; i++) {
        uint8_t t = spi_byte(0xFF);
        if (t == 0xFE) return 0;
        if (t != 0xFF) return -1;   // error token
    }
    return -1;
}

// Wait for SD card to de-assert busy (MISO high) after a write.
static int wait_ready(void) {
    for (int i = 0; i < 500000; i++)
        if (spi_byte(0xFF) == 0xFF) return 0;
    return -1;
}

// ---- Driver state ----------------------------------------------------------

static DSTATUS s_sta    = STA_NOINIT;
static bool    s_sdhc   = false;    // true = SDHC/SDXC (block addr), false = SDSC (byte addr)

// ---- Initialisation --------------------------------------------------------

static void sd_gpio_init(void) {
    // MOSI, CS: outputs; MISO: input with pull-up
    gpio_init(SD_MOSI); gpio_set_dir(SD_MOSI, GPIO_OUT); gpio_put(SD_MOSI, 1);
    gpio_init(SD_MISO); gpio_set_dir(SD_MISO, GPIO_IN);  gpio_pull_up(SD_MISO);
    gpio_init(SD_CS);   gpio_set_dir(SD_CS,   GPIO_OUT); gpio_put(SD_CS,   1);
    // CLK managed separately (shared with VGA PIO0)
}

DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;

    sd_gpio_init();
    clk_claim();

    // Power-up: ≥74 clocks with CS=MOSI=1
    cs_hi();
    gpio_put(SD_MOSI, 1);
    spi_ff(10);

    // CMD0 — software reset, enter SPI mode
    int attempts = 0;
    uint8_t r;
    do {
        r = sd_cmd(0, 0);
    } while (r != 0x01 && ++attempts < 20);
    if (r != 0x01) goto fail;

    // CMD8 — send interface condition (also tells card we support SDHC)
    cs_lo();
    r = sd_cmd_raw(8, 0x1AA);
    if (r == 0x01) {
        // SDHC path: consume the 4-byte R7 tail
        uint8_t r7[4];
        for (int i = 0; i < 4; i++) r7[i] = spi_byte(0xFF);
        cs_hi();
        spi_ff(1);
        // ACMD41 with HCS bit until card is ready (up to 2 s)
        for (int i = 0; i < 200; i++) {
            r = sd_acmd(41, 0x40000000UL);
            if (r == 0) break;
            sleep_ms(10);
        }
        if (r != 0) goto fail;
        // CMD58 — read OCR to check CCS bit (SDHC vs SDSC)
        cs_lo();
        r = sd_cmd_raw(58, 0);
        uint8_t ocr[4];
        for (int i = 0; i < 4; i++) ocr[i] = spi_byte(0xFF);
        cs_hi();
        spi_ff(1);
        if (r != 0) goto fail;
        s_sdhc = (ocr[0] & 0x40) != 0;
    } else {
        // SDSC / MMC path
        cs_hi();
        spi_ff(1);
        // Try ACMD41 (SD) then CMD1 (MMC)
        for (int i = 0; i < 200; i++) {
            r = sd_acmd(41, 0);
            if (r == 0) break;
            sleep_ms(10);
        }
        if (r != 0) goto fail;
        s_sdhc = false;
        // CMD16 — set block length to 512
        r = sd_cmd(16, 512);
        if (r != 0) goto fail;
    }

    clk_release();
    s_sta = 0;
    return 0;

fail:
    clk_release();
    s_sta = STA_NOINIT;
    return STA_NOINIT;
}

// ---- Status ----------------------------------------------------------------

DSTATUS disk_status(BYTE pdrv) {
    return (pdrv != 0) ? STA_NOINIT : s_sta;
}

// ---- Read ------------------------------------------------------------------

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || (s_sta & STA_NOINIT)) return RES_NOTRDY;
    if (!s_sdhc) sector *= 512;
    clk_claim();

    if (count == 1) {
        // CMD17 — single block read
        cs_lo();
        uint8_t r = sd_cmd_raw(17, sector);
        if (r != 0 || wait_token() != 0) { cs_hi(); goto fail; }
        for (int i = 0; i < 512; i++) buff[i] = spi_byte(0xFF);
        spi_ff(2);  // discard CRC
        cs_hi();
        spi_ff(1);
    } else {
        // CMD18 — multiple block read
        cs_lo();
        uint8_t r = sd_cmd_raw(18, sector);
        if (r != 0) { cs_hi(); goto fail; }
        while (count--) {
            if (wait_token() != 0) { cs_hi(); goto fail; }
            for (int i = 0; i < 512; i++) *buff++ = spi_byte(0xFF);
            spi_ff(2);
        }
        cs_hi();
        spi_ff(1);
        sd_cmd(12, 0);  // CMD12 — stop transmission
    }

    clk_release();
    return RES_OK;
fail:
    clk_release();
    return RES_ERROR;
}

// ---- Write -----------------------------------------------------------------

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || (s_sta & STA_NOINIT)) return RES_NOTRDY;
    if (s_sta & STA_PROTECT)               return RES_WRPRT;
    if (!s_sdhc) sector *= 512;
    clk_claim();

    while (count--) {
        // CMD24 — single block write
        cs_lo();
        uint8_t r = sd_cmd_raw(24, sector);
        if (r != 0) { cs_hi(); goto fail; }
        spi_ff(1);                  // one idle byte before token
        spi_byte(0xFE);             // data token
        for (int i = 0; i < 512; i++) spi_byte(buff[i]);
        spi_ff(2);                  // dummy CRC
        r = spi_byte(0xFF) & 0x1F;
        cs_hi();
        if (r != 0x05) goto fail;   // 0b00101 = data accepted
        // Wait for card to finish programming
        cs_lo();
        if (wait_ready() != 0) { cs_hi(); goto fail; }
        cs_hi();
        spi_ff(1);
        buff   += 512;
        if (!s_sdhc) sector += 512;
        else         sector++;
    }

    clk_release();
    return RES_OK;
fail:
    clk_release();
    return RES_ERROR;
}

// ---- ioctl -----------------------------------------------------------------

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv != 0 || (s_sta & STA_NOINIT)) return RES_NOTRDY;
    switch (cmd) {
        case CTRL_SYNC:       return RES_OK;
        case GET_SECTOR_SIZE: *(WORD  *)buff = 512; return RES_OK;
        case GET_BLOCK_SIZE:  *(DWORD *)buff = 1;   return RES_OK;
        default:              return RES_PARERR;
    }
}
