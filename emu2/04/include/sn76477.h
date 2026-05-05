#pragma once
#include <stdint.h>

// Audio output for ABC80 — ABC80 drives sound via Z80 OUT 6.
// Call sn76477_init() once at startup, sn76477_write() from port_out(6, val).
//
// Hardware: PWM audio left channel on GPIO 28 (Pimoroni Demo Board PWM jack).
//
// Port 6 bit 0 : 1=sound on, 0=silence
// Port 6 bit 1 : (external mode) 0=~6400 Hz high,  1=~640 Hz low
// Port 6 bit 2 : 0=external voltage (fixed tone),  1=SLF control (wow-wow)
// Envelope [7:6]: 00=VCO-gated  01=always-on  10=one-shot  11=alternating
//
// Bell (CHR$ 7):  OUT 6,0  then  OUT 6,131  (0x83 = 640 Hz one-shot)

void sn76477_init(void);
void sn76477_write(uint8_t data);

/*
  Sound                          OUT 6   Notes
  ---------------------------------------------------------------------
  Silence                            0
  -- Always-on envelope (bit6=1) ---------------------------------------
  Tone 640 Hz loud                  67   0x43  bit0+bit1+bit6
  Tone ~6400 Hz (high) loud         65   0x41  bit0+bit6
  Wow-wow (SLF→VCO) loud            69   0x45  bit0+bit2+bit6
  Noise loud                        73   0x49  bit0+bit3+bit6
  VCO+noise 640 Hz loud             91   0x5B  bit0+bit1+bit3+bit4+bit6
  Pulsed noise ~4 Hz loud           97   0x61  bit0+bit5+bit6
  Tremolo 640 Hz                    195   0xC3  alternating env
  -- VCO-gated envelope (softer) ---------------------------------------
  Tone 640 Hz                        3   0x03  bit0+bit1
  Wow-wow                            5   0x05  bit0+bit2
  -- One-shot  (first: OUT 6,0  then immediately:) ---------------------
  BELL  640 Hz                     131   0x83  bit0+bit1+bit7  (CHR$ 7)
  Pew!  640 Hz tone+decay          131   0x83
  Oi!   SLF→VCO glide (varies)     135   0x87  bit0+bit1+bit2+bit7  SLF-phase varies → different pitch/sweep each time (spaceship-invaders)
  Bang  noise                      137   0x89  bit0+bit3+bit7
  Boom  VCO+noise                  155   0x9B  bit0+bit1+bit3+bit4+bit7
  Oi!   noise burst (varies)       157   0x9D  bit0+bit2+bit3+bit7  noise-LFSR varies → different texture each time
  Burst pulsed-noise               161   0xA1  bit0+bit5+bit7
*/


/* Ur Avancerad programmering på ABC80

| Kategori                     | Styrord  | Beskrivning                                      |
|------------------------------|----------|--------------------------------------------------|
| RENA KOMPONENTER             | 1        | Ren ton, högsta frekvens                         |
|                              | 249      | .                                                |
|                              | 3        | .                                                |
|                              | 251      | Ren ton, lägsta frekvens                         |
|                              | 5        | Hög frekvens, siren "oioioi~"                    |
|                              | 253      | Låg frekvens, siren "oioioi~"                    |
|                              | 9        | Brus "vattenfall"                                |
|                              | 17       | "knäpp-knäpp-knäpp" (fiskebåt)                   |
| RENA KOMPONENTER + BRUS      | 25       | Högsta frekvens                                  |
|                              | 217      | .                                                |
|                              | 27       | .                                                |
|                              | 219      | Lägsta frekvens                                  |
|                              | 29       | Hög frekvens, siren "oioioi~"                    |
|                              | 221      | Låg frekvens, siren "oioioi~"                    |
| PULSAT MED SLF               | 41       | Högsta frekvens                                  |
|                              | 209      | .                                                |
|                              | 43       | .                                                |
|                              | 211      | Lägsta frekvens                                  |
|                              | 41       | Hög frekvens, pulsad siren "piop-piop-piop"      |
|                              | 237      | Låg frekvens, pulsad siren                       |
|                              | 33       | Brus, pulsat ("tuff-tuff-lok")                   |
| PULSAT MED SLF + BRUS        | 49       | Högsta frekvens                                  |
|                              | 241      | .                                                |
|                              | 51       | .                                                |
|                              | 243      | Lägsta frekvens                                  |
|                              | 55       | Hög frekvens, pulsad siren "piop-piop-piop"      |
|                              | 247      | Låg frekvens, pulsad siren                       |
| KORT LJUDPULS                | 129      | Hög frekvens                                     |
|                              | 131      | Låg frekvens (=Bell, CHR$(7))                    |
|                              | 135      | "oi..." (Olika varje gång)                       |
|                              | 137      | Brus (ett skott)                                 |
| KORT LJUDPULS MED BRUS       | 153      | Hög frekvens                                     |
|                              | 155      | Låg frekvens                                     |
|                              | 157      | "oi..." (Olika varje gång).                      |

*/