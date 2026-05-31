/*
 * dvi_out.c  double-buffered DVI output, core 1
 *
 * How the two cores interact
 * --------------------------
 * Core 1 runs dvi_scanbuf_main_16bpp() for the entire lifetime of the program.
 * That function loops:
 *   1. Pop a (uint16_t *) scanline pointer from q_colour_valid.
 *   2. TMDS-encode the 320 RGB565 pixels it points at.
 *      On RP2350, the SIO hardware TMDS encoder is used automatically
 *      (DVI_USE_SIO_TMDS_ENCODER = 1 when PICO_RP2040 == 0).
 *   3. Push the encoded buffer to q_tmds_valid.
 *   4. Push the original scanline pointer back to q_colour_free.
 *   The DMA IRQ (also on core 1) simultaneously moves TMDS buffers from
 *   q_tmds_valid to the PIO FIFOs, repeating each scanline DVI_VERTICAL_REPEAT
 *   (= 2) times before releasing the buffer back to q_tmds_free.
 *
 * Core 0 runs dvi_out_wait_vsync(), which pushes 240 scanline pointers from
 * the current front framebuffer into q_colour_valid.  queue_add_blocking_u32()
 * stalls whenever all DVI_N_TMDS_BUFFERS (= 3) buffers are in flight, which
 * naturally throttles the loop to the DVI output rate.  The 240-scanline loop
 * therefore takes exactly one frame period (≈ 16.7 ms) and acts as the vsync.
 *
 * Double-buffer swap
 * ------------------
 * Core 0 renders CHIP-8 into back_buf, calls dvi_out_swap_buffers() to
 * atomically promote it to front, then calls dvi_out_wait_vsync() which feeds
 * the new front.  The swap is a single 32-bit-aligned pointer write, which
 * the RP2350 guarantees to be seen atomically by the other core.
 *
 * Memory layout
 * -------------
 * Two 320 x 240 x 2-byte framebuffers = 307 200 bytes ≈ 300 KB.
 * The RP2350 has 520 KB SRAM; together with code, stack and libdvi's TMDS
 * buffers the total stays well within budget.
 */

#include "dvi_out.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/sync.h"   /* next_striped_spin_lock_num(), __wfe()  */
#include "dvi.h"             /* dvi_inst, dvi_init, dvi_start, ...     */
#include "dvi_timing.h"      /* dvi_timing_640x480p_60hz               */
#include "common_dvi_pin_configs.h"   /* pico_sock_cfg                 */
#include <string.h>

/* 640 x 480 @ 60 Hz: TMDS bit clock = 252 MHz --> sys_clock = 252 MHz */
#define DVI_TIMING dvi_timing_640x480p_60hz

static struct dvi_inst dvi0;

/* Two full-resolution RGB565 framebuffers (~150 KB each, ≈ 300 KB total). */
static gfx_color_t fb_a[GFX_W * GFX_H];
static gfx_color_t fb_b[GFX_W * GFX_H];

/* front = fed to DVI by core 0; back = rendered into by core 0. */
static volatile gfx_color_t * volatile dvi_front = fb_a;
static volatile gfx_color_t * volatile dvi_back  = fb_b;

static volatile uint32_t dvi_frame_ctr = 0;

/*
 * Public API
 */

void dvi_out_init(void)
{
    /*
     * Raise DVDD to 1.2 V before overclocking.  On RP2350 the default core
     * voltage (1.1 V) supports up to ~200 MHz; 1.2 V unlocks up to ~300 MHz.
     * The short sleep lets the SMPS settle before the PLL is reconfigured.
     */
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);

    /* TMDS bit rate for 640 x 480 @ 60 Hz is exactly 252 Mbit/s per lane.
     * PIO serialisers run at sys_clock / 1, so sys_clock must equal
     * bit_clk_khz (252 000 kHz) for the TMDS timing to be correct. */
    set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);

    dvi0.timing  = &DVI_TIMING;
    dvi0.ser_cfg = pico_sock_cfg;
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

    memset(fb_a, 0, sizeof(fb_a));
    memset(fb_b, 0, sizeof(fb_b));
}

gfx_color_t *dvi_out_back_buffer(void)
{
    /* Cast away volatile: core 0 is the sole writer of the back buffer. */
    return (gfx_color_t *)dvi_back;
}

void dvi_out_swap_buffers(void)
{
    /* Single 32-bit aligned pointer write — atomic on RP2350. */
    gfx_color_t *old_front = (gfx_color_t *)dvi_front;
    dvi_front = dvi_back;
    dvi_back  = old_front;
}

void dvi_out_wait_vsync(void)
{
    /*
     * Feed one full frame to libdvi.
     *
     * We push a pointer to each of the GFX_H = 240 scanlines in the front
     * framebuffer.  libdvi outputs each scanline DVI_VERTICAL_REPEAT = 2
     * times, producing 480 active DVI rows total (640 x 480 mode).
     *
     * queue_add_blocking_u32 blocks when all DVI_N_TMDS_BUFFERS = 3 TMDS
     * buffers are in flight.  Each buffer is held for ≈ 2 x 31.7 µs before
     * being released, so core 0 stalls briefly after every few pushes.  Over
     * 240 iterations this amounts to exactly one frame period (≈ 16.7 ms),
     * giving us a software vsync with zero extra busy-wait.
     *
     * We drain q_colour_free after each push to prevent it from filling up
     * and stalling the encoder on the other side.
     */
    for (int y = 0; y < GFX_H; y++) {
        const gfx_color_t *row = (const gfx_color_t *)dvi_front + y * GFX_W;
        queue_add_blocking_u32(&dvi0.q_colour_valid, &row);

        gfx_color_t *freed;
        while (queue_try_remove_u32(&dvi0.q_colour_free, &freed))
            ;
    }

    ++dvi_frame_ctr;
}

uint32_t dvi_out_frame_count(void)
{
    return dvi_frame_ctr;
}

void dvi_out_push_row(int y)
{
    const gfx_color_t *row = (const gfx_color_t *)dvi_front + (uint32_t)y * GFX_W;
    queue_add_blocking_u32(&dvi0.q_colour_valid, &row);
    gfx_color_t *freed;
    while (queue_try_remove_u32(&dvi0.q_colour_free, &freed))
        ;
    if (y == GFX_H - 1)
        ++dvi_frame_ctr;
}

/* 
 * Core 1 entry point
 */

void dvi_out_core1_main(void)
{
    /*
     * Route DMA completion interrupts to this core.  All DVI DMA channels
     * share DMA_IRQ_0; dvi_register_irqs_this_core installs an exclusive
     * handler and will assert if another handler is already present.
     */
    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);

    /* Wait until core 0 has pushed at least one scanline so we don't start
     * the PIO serialisers with an empty TMDS queue. */
    while (queue_is_empty(&dvi0.q_colour_valid))
        __wfe();

    dvi_start(&dvi0);

    /* dvi_scanbuf_main_16bpp never returns.
     * It pops from q_colour_valid, TMDS-encodes each scanline using the
     * RP2350 SIO hardware encoder, and hands the result to q_tmds_valid.
     * The DMA IRQ runs inside this function to keep the PIO FIFOs fed. */
    dvi_scanbuf_main_16bpp(&dvi0);
}
