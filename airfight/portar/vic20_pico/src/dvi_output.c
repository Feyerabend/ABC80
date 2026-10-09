#include "dvi_output.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/sync.h"
#include "dvi.h"
#include "dvi_timing.h"
#include "common_dvi_pin_configs.h"
#include <string.h>

#define DVI_TIMING dvi_timing_640x480p_60hz

static struct dvi_inst dvi0;

static uint16_t fb_a[DVI_BUF_PIXELS * DVI_BUF_LINES];
static uint16_t fb_b[DVI_BUF_PIXELS * DVI_BUF_LINES];

static volatile uint16_t * volatile dvi_front = fb_a;
static volatile uint16_t * volatile dvi_back  = fb_b;

void dvi_output_init(void) {
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);

    dvi0.timing  = &DVI_TIMING;
    dvi0.ser_cfg = pico_sock_cfg;
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

    memset(fb_a, 0, sizeof(fb_a));
    memset(fb_b, 0, sizeof(fb_b));
}

uint16_t *dvi_output_back_buf(void) {
    return (uint16_t *)dvi_back;
}

void dvi_output_swap(void) {
    uint16_t *old_front = (uint16_t *)dvi_front;
    dvi_front = dvi_back;
    dvi_back  = old_front;
}

void dvi_output_push_row(int y) {
    const uint16_t *row = (const uint16_t *)dvi_front + (uint32_t)y * DVI_BUF_PIXELS;
    queue_add_blocking_u32(&dvi0.q_colour_valid, &row);
    uint16_t *freed;
    while (queue_try_remove_u32(&dvi0.q_colour_free, &freed))
        ;
}

void dvi_output_core1_main(void) {
    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
    while (queue_is_empty(&dvi0.q_colour_valid))
        __wfe();
    dvi_start(&dvi0);
    dvi_scanbuf_main_16bpp(&dvi0);
    __builtin_unreachable();
}
