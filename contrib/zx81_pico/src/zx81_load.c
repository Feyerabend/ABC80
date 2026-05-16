/*
 * zx81_load.c  ZX81 .p file loader via USB-CDC serial
 *
 * State machine (runs on Core 0, called once per frame from main.c):
 *
 *   IDLE  ──Ctrl+L──►  RECV_LO  ──byte──►  RECV_HI  ──byte──►  RECV_DATA
 *                                                                    │
 *                                                           all bytes received
 *                                                                    │
 *                                                                    ▼
 *                                                              write → m[0x4009]
 *                                                              zx81_p_load_launch()
 *                                                              print "OK"
 *                                                              back to IDLE
 *
 * A 5-second receive timeout returns to IDLE with an error message so the
 * Pico does not lock up if the sender crashes mid-transfer.
 */

#include "zx81_load.h"
#include "z80_api.h"
#include "zx81.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

/* ZX81 BASIC program starts at 0x4009 (first byte after system variables). */
#define LOAD_ADDR   0x4009u

/* Maximum .p file size that fits in our RAM window (0x4009 – 0xBFFF). */
#define MAX_LOAD    (0xBFFF - LOAD_ADDR + 1)   /* 32759 bytes */

/* Timeout: if no byte arrives for this many frames, abort. */
#define TIMEOUT_FRAMES  300   /* ~5 s at 60 Hz */

typedef enum {
    LD_IDLE,
    LD_RECV_LO,    /* waiting for length low byte  */
    LD_RECV_HI,    /* waiting for length high byte */
    LD_RECV_DATA,  /* receiving payload             */
} ld_state_t;

static ld_state_t ld_state  = LD_IDLE;
static uint16_t   ld_len    = 0;     /* expected payload length  */
static uint16_t   ld_got    = 0;     /* bytes received so far    */
static int        ld_timer  = 0;     /* timeout countdown        */

/* ---- helpers --------------------------------------------------------- */

static void ld_abort(const char *msg)
{
    printf("LOAD: %s\r\n", msg);
    ld_state = LD_IDLE;
}

static void ld_finish(void)
{
    printf("OK  (%u bytes loaded at 0x%04X)\r\n",
           (unsigned)ld_len, (unsigned)LOAD_ADDR);
    zx81_p_load_launch();
}

/* ---- public API ------------------------------------------------------ */

void zx81_load_start(void)
{
    ld_state = LD_RECV_LO;
    ld_timer = TIMEOUT_FRAMES;
    printf("LOAD: waiting... (send 2-byte length + raw .p data)\r\n");
}

bool zx81_load_tick(void)
{
    if (ld_state == LD_IDLE)
        return false;

    /* Timeout guard */
    if (--ld_timer <= 0) {
        ld_abort("timeout");
        return false;
    }

    /* Drain all available bytes this frame */
    int ch;
    while ((ch = getchar_timeout_us(0)) >= 0) {
        ld_timer = TIMEOUT_FRAMES;   /* reset timeout on each byte */

        switch (ld_state) {

        case LD_RECV_LO:
            ld_len  = (uint16_t)(ch & 0xFF);
            ld_got  = 0;
            ld_state = LD_RECV_HI;
            break;

        case LD_RECV_HI:
            ld_len |= (uint16_t)((ch & 0xFF) << 8);
            if (ld_len == 0 || ld_len > MAX_LOAD) {
                ld_abort("bad length");
                return false;
            }
            ld_state = LD_RECV_DATA;
            break;

        case LD_RECV_DATA:
            m[LOAD_ADDR + ld_got] = (uint8_t)ch;
            ld_got++;
            if (ld_got >= ld_len) {
                ld_state = LD_IDLE;
                ld_finish();
                return true;   /* signal to caller: CPU was reset */
            }
            break;

        default:
            break;
        }
    }

    return false;
}
