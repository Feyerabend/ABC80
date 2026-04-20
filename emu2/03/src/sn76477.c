// Full SN76477 emulation for ABC80 on Pico 2.
//
// Audio output: PWM-DAC on GPIO 28 (Pimoroni Demo Board PWM audio left).
// GPIO 28 -> logic buffer U3 -> RC low-pass filter -> 3.5mm PWM audio jack.
//
// PWM carrier: ~586 kHz (150 MHz / 256).  Duty cycle updated at 22050 Hz
// by a repeating timer ISR.  The RC filter recovers the audio envelope.
//
// ABC80 component values (MAME abc80.cpp / mame-rr abc80.c):
//   Noise clock:  R=47kΩ   (R26)     Noise filter: R=330kΩ (R24), C=390pF (C52)
//   Decay:        R=47kΩ   (R23)     Attack/decay: C=10µF (C50), R_att=2.2kΩ (R21)
//   Amplitude:    R=33kΩ   (R19)     Feedback:     R=10kΩ (R18)
//   VCO:          C=10nF   (C48), R=100kΩ (R20), pitch_voltage=0 (N/C)
//   SLF:          R=220kΩ  (R22), C=1µF (C51)
//   One-shot:     C=0.1µF  (C53), R=330kΩ (R25)
//
// Port 6 bit mapping (MAME csg_w):
//   bit 0 : enable inverted — 1=chip on, 0=chip off (silence)
//   bit 1 : vco_voltage     — 0=0V->640 Hz, 1=2.5V->1280 Hz
//   bit 2 : vco_w           — 0=SLF drives VCO (wow-wow), 1=external voltage
//   bit 3 : mixer_b     ─┐
//   bit 4 : mixer_a     ─┼─ mixer_mode = (bit5<<2)|(bit3<<1)|bit4
//   bit 5 : mixer_c     ─┘
//   bit 6 : envelope_2  ─┐ envelope_mode = (bit6<<1)|bit7
//   bit 7 : envelope_1  ─┘
//
// Mixer modes:  0=VCO  1=SLF  2=Noise  3=VCO+Noise
//               4=SLF+Noise  5=VCO+SLF+Noise  6=VCO+SLF  7=Inhibit
// Envelope:     0=VCO-gated  1=one-shot  2=always-on  3=alternating

#include "sn76477.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#define AUDIO_GPIO   28
#define SAMPLE_RATE  22050

//  Voltage thresholds (empirical, from MAME / nocoolnicksleft source)
#define ONE_SHOT_V_MIN   0.0f
#define ONE_SHOT_V_MAX   2.5f
#define ONE_SHOT_V_RANGE 2.5f
#define SLF_V_MIN        0.33f
#define SLF_V_MAX        2.37f
#define SLF_V_RANGE      2.04f
#define VCO_SLF_V_DIFF   0.35f
#define VCO_V_MIN        0.33f
#define VCO_V_MAX        2.72f
#define VCO_V_RANGE      2.39f
#define NOISE_V_MIN      0.0f
#define NOISE_V_MAX      5.0f
#define NOISE_V_RANGE    5.0f
#define NOISE_HI_THOLD   3.35f
#define NOISE_LO_THOLD   0.74f
#define AD_V_MIN         0.0f
#define AD_V_MAX         4.44f
#define AD_V_RANGE       4.44f
#define OUT_CENTER       2.57f
#define OUT_HI_CLIP      3.51f
#define OUT_LO_CLIP      0.715f

// ABC80 fixed component values
#define R_NOISE_CLK    47000.0f
#define R_NOISE_FILT   330000.0f
#define C_NOISE_FILT   390e-12f
#define R_DECAY        47000.0f
#define C_AD           10e-6f
#define R_ATTACK       2200.0f
#define R_AMP          33000.0f
#define R_FB           10000.0f
#define C_VCO          10e-9f
#define R_VCO          100000.0f
#define R_SLF          220000.0f
#define C_SLF          1e-6f
#define C_ONESHOT      0.1e-6f
#define R_ONESHOT      330000.0f

// Envelope gain tables (empirical measurements from real chip)
static const float out_pos_gain[45] = {
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.01f,
    0.03f, 0.11f, 0.15f, 0.19f, 0.21f, 0.23f, 0.26f, 0.29f, 0.31f, 0.33f,
    0.36f, 0.38f, 0.41f, 0.43f, 0.46f, 0.49f, 0.52f, 0.54f, 0.57f, 0.60f,
    0.62f, 0.65f, 0.68f, 0.70f, 0.73f, 0.76f, 0.80f, 0.82f, 0.84f, 0.87f,
    0.90f, 0.93f, 0.96f, 0.98f, 1.00f
};
static const float out_neg_gain[45] = {
     0.00f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f, -0.01f,
    -0.02f, -0.09f, -0.13f, -0.15f, -0.17f, -0.19f, -0.22f, -0.24f, -0.26f, -0.28f,
    -0.30f, -0.32f, -0.34f, -0.37f, -0.39f, -0.41f, -0.44f, -0.46f, -0.48f, -0.51f,
    -0.53f, -0.56f, -0.58f, -0.60f, -0.62f, -0.65f, -0.67f, -0.69f, -0.72f, -0.74f,
    -0.76f, -0.78f, -0.81f, -0.84f, -0.85f
};

// Chip state
typedef struct {
    // Set by sn76477_write() — all 32-bit, atomic on RP2350
    uint32_t enable;          // 0=sound on
    float    vco_voltage;     // 0.0 or 2.5
    uint32_t vco_mode;        // 0=SLF drives VCO, 1=external voltage
    uint32_t mixer_mode;      // 0–7
    uint32_t envelope_mode;   // 0–3

    // Capacitor voltages
    float one_shot_v;
    float slf_v;
    float vco_v;
    float noise_filt_v;
    float ad_v;

    // Flip-flops
    uint32_t one_shot_ff;
    uint32_t slf_ff;
    uint32_t vco_ff;
    uint32_t vco_alt_ff;
    uint32_t noise_filt_ff;
    uint32_t noise_raw_ff;

    // Noise LFSR + phase accumulator
    uint32_t rng;
    uint32_t noise_acc;

    // Precomputed per-sample steps (constant for ABC80)
    float one_shot_charge;
    float one_shot_discharge;
    float slf_charge;
    float slf_discharge;
    float vco_step;
    float noise_filt_charge;
    float noise_filt_discharge;
    float attack_step;
    float decay_step;
    float peak_v;
    uint32_t noise_freq;
} sn_t;

static sn_t s_sn;
static uint s_slice;
static uint s_chan;
static repeating_timer_t s_timer;

static bool audio_cb(repeating_timer_t *rt) {
    (void)rt;
    sn_t *s = &s_sn;

    // One-shot
    if (s->one_shot_ff) {
        s->one_shot_v += s->one_shot_charge;
        if (s->one_shot_v >= ONE_SHOT_V_MAX) {
            s->one_shot_v  = ONE_SHOT_V_MAX;
            s->one_shot_ff = 0;
        }
    } else {
        s->one_shot_v -= s->one_shot_discharge;
        if (s->one_shot_v < ONE_SHOT_V_MIN) s->one_shot_v = ONE_SHOT_V_MIN;
    }

    // SLF triangle oscillator
    if (!s->slf_ff) {
        s->slf_v += s->slf_charge;
        if (s->slf_v >= SLF_V_MAX) { s->slf_v = SLF_V_MAX; s->slf_ff = 1; }
    } else {
        s->slf_v -= s->slf_discharge;
        if (s->slf_v <= SLF_V_MIN) { s->slf_v = SLF_V_MIN; s->slf_ff = 0; }
    }

    // VCO
    // External: higher pin-16 voltage -> lower frequency (SN76477 datasheet).
    //   0V (bit1=0) -> ~6400 Hz max;  ~2V (bit1=1) -> 640 Hz minimum.
    // SLF: triangle wave sweeps vco_top -> wow-wow.
    float vco_top;
    if (s->vco_mode) {
        vco_top = VCO_V_MIN + VCO_V_RANGE * (s->vco_voltage / 2.0f);
        if (vco_top > VCO_V_MAX)         vco_top = VCO_V_MAX;         // clamp to 640 Hz
        if (vco_top < VCO_V_MIN + 0.24f) vco_top = VCO_V_MIN + 0.24f; // clamp to ~6400 Hz
    } else {
        vco_top = s->slf_v + VCO_SLF_V_DIFF;
        if (vco_top < VCO_V_MIN + 0.01f) vco_top = VCO_V_MIN + 0.01f;
    }

    if (!s->vco_ff) {
        s->vco_v += s->vco_step;
        if (s->vco_v >= vco_top) {
            s->vco_v = vco_top;
            s->vco_ff = 1;
            s->vco_alt_ff ^= 1;
        }
    } else {
        s->vco_v -= s->vco_step;
        if (s->vco_v <= VCO_V_MIN) { s->vco_v = VCO_V_MIN; s->vco_ff = 0; }
    }

    // Noise LFSR
    s->noise_acc += s->noise_freq;
    while (s->noise_acc >= (uint32_t)SAMPLE_RATE) {
        s->noise_acc -= (uint32_t)SAMPLE_RATE;
        uint32_t b = ((s->rng >> 28) & 1) ^ (s->rng & 1);
        if ((s->rng & 0x1000001fu) == 0) b = 1;
        s->rng = (s->rng >> 1) | (b << 30);
        s->noise_raw_ff = b;
    }
    if (s->noise_raw_ff) {
        s->noise_filt_v += s->noise_filt_charge;
        if (s->noise_filt_v > NOISE_V_MAX) s->noise_filt_v = NOISE_V_MAX;
    } else {
        s->noise_filt_v -= s->noise_filt_discharge;
        if (s->noise_filt_v < NOISE_V_MIN) s->noise_filt_v = NOISE_V_MIN;
    }
    if      (s->noise_filt_v >= NOISE_HI_THOLD) s->noise_filt_ff = 0;
    else if (s->noise_filt_v <= NOISE_LO_THOLD) s->noise_filt_ff = 1;

    // Envelope attack / decay
    int charging;
    switch (s->envelope_mode) {
        case 0:  charging = (int)s->vco_ff; break;
        case 1:  charging = (int)s->one_shot_ff; break;
        default:
        case 2:  charging = 1; break;
        case 3:  charging = (int)(s->vco_ff & s->vco_alt_ff); break;
    }
    if (charging) {
        s->ad_v += s->attack_step;
        if (s->ad_v > AD_V_MAX) s->ad_v = AD_V_MAX;
    } else {
        s->ad_v -= s->decay_step;
        if (s->ad_v < AD_V_MIN) s->ad_v = AD_V_MIN;
    }

    // Mixer -> output voltage
    float voltage_out;
    if (!s->enable) {
        uint32_t out;
        switch (s->mixer_mode) {
            case 0: out = s->vco_ff; break;
            case 1: out = s->slf_ff; break;
            case 2: out = s->noise_filt_ff; break;
            case 3: out = s->vco_ff  & s->noise_filt_ff; break;
            case 4: out = s->slf_ff  & s->noise_filt_ff; break;
            case 5: out = s->vco_ff  & s->slf_ff & s->noise_filt_ff; break;
            case 6: out = s->vco_ff  & s->slf_ff; break;
            default: out = 0; break;
        }
        int idx = (int)(s->ad_v * 10.0f);
        if (idx < 0)  idx = 0;
        if (idx > 44) idx = 44;
        if (out) {
            voltage_out = OUT_CENTER + s->peak_v * out_pos_gain[idx];
            if (voltage_out > OUT_HI_CLIP) voltage_out = OUT_HI_CLIP;
        } else {
            voltage_out = OUT_CENTER + s->peak_v * out_neg_gain[idx];
            if (voltage_out < OUT_LO_CLIP) voltage_out = OUT_LO_CLIP;
        }
    } else {
        voltage_out = OUT_CENTER;   // chip disabled -> DC -> filtered away
    }

    //  PWM-DAC: map voltage to 8-bit duty cycle
    float norm = (voltage_out - OUT_LO_CLIP) / (OUT_HI_CLIP - OUT_LO_CLIP);
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    pwm_set_chan_level(s_slice, s_chan, (uint16_t)(norm * 255.0f));
    return true;
}

void sn76477_init(void) {
    // PWM-DAC: carrier = 150 MHz / 256 ≈ 586 kHz; duty updated at SAMPLE_RATE Hz
    gpio_set_function(AUDIO_GPIO, GPIO_FUNC_PWM);
    s_slice = pwm_gpio_to_slice_num(AUDIO_GPIO);
    s_chan  = pwm_gpio_to_channel(AUDIO_GPIO);
    pwm_set_clkdiv_int_frac(s_slice, 1, 0);
    pwm_set_wrap(s_slice, 255);
    pwm_set_chan_level(s_slice, s_chan, 128);
    pwm_set_enabled(s_slice, true);

    sn_t *s = &s_sn;
    *s = (sn_t){0};
    s->enable = 1;         // start silent
    s->rng    = 0xACE1u;
    s->ad_v   = AD_V_MAX;  // envelope_mode=2 starts at full amplitude

    // Precompute per-sample charging steps
    s->one_shot_charge    = ONE_SHOT_V_RANGE / (0.8024f * R_ONESHOT * C_ONESHOT + 0.002079f)        / SAMPLE_RATE;
    s->one_shot_discharge = ONE_SHOT_V_RANGE / (854.7f  * C_ONESHOT + 0.00001795f)                  / SAMPLE_RATE;
    s->slf_charge         = SLF_V_RANGE / (0.5885f * R_SLF * C_SLF + 0.001300f)                     / SAMPLE_RATE;
    s->slf_discharge      = SLF_V_RANGE / (0.5413f * R_SLF * C_SLF + 0.001343f)                     / SAMPLE_RATE;
    s->vco_step           = 0.64f * 2.0f * VCO_V_RANGE / (R_VCO * C_VCO)                            / SAMPLE_RATE;
    s->noise_filt_charge  = NOISE_V_RANGE / (0.1571f * R_NOISE_FILT * C_NOISE_FILT + 0.00001430f)   / SAMPLE_RATE;
    s->noise_filt_discharge = NOISE_V_RANGE / (0.1331f * R_NOISE_FILT * C_NOISE_FILT + 0.00001734f) / SAMPLE_RATE;
    s->attack_step        = AD_V_RANGE / (R_ATTACK * C_AD)                                          / SAMPLE_RATE;
    s->decay_step         = AD_V_RANGE / (R_DECAY  * C_AD)                                          / SAMPLE_RATE;
    s->peak_v             = 3.818f * (R_FB / R_AMP) + 0.03f;
    s->noise_freq         = (uint32_t)(339100000.0f * powf(R_NOISE_CLK, -0.8849f));

    add_repeating_timer_us(1000000 / SAMPLE_RATE, audio_cb, NULL, &s_timer);
}

void sn76477_write(uint8_t data) {
    sn_t *s = &s_sn;
    uint32_t new_enable = (data & 0x01) ? 0u : 1u;
    // One-shot fires on chip-enable edge: disabled->enabled (bit0: 0->1).
    // Per SN76477 datasheet: pin 9 high-to-low transition triggers one-shot.
    if (s->enable && !new_enable) {
        s->one_shot_ff = 1;
        s->one_shot_v  = ONE_SHOT_V_MIN;
    }
    s->enable        = new_enable;
    s->vco_voltage   = (data & 0x02) ? 2.0f : 0.0f;  // pin16 via 1.5k/1k divider -> 0V or ~2V
    s->vco_mode      = (data & 0x04) ? 0u : 1u;  // bit2=0->external(1), bit2=1->SLF(0)
    s->mixer_mode    = (((data >> 5) & 1u) << 2)
                     | (((data >> 3) & 1u) << 1)
                     |  ((data >> 4) & 1u);
    s->envelope_mode = (((data >> 6) & 1u) << 1)
                     |  ((data >> 7) & 1u);
}
