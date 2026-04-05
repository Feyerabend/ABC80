#pragma once
#include <stdint.h>

// ABC80 BASIC error codes and messages.
// Swedish characters use ABC80 / SIS-662241 single-byte codes:
//   0x5B = Ä   0x5C = Ö   0x5D = Å

typedef struct {
    uint8_t     code;
    const char *msg;
} abc80_err_t;

static const abc80_err_t abc80_errors[] = {
    {  0, "EJ TILL\x5DTET \x5CKA \"DIM\""               },
    {  1, "FEL ANTAL INDEX"                             },
    {  2, "OTILL\x5DTET SOM KOMMANDO"                   },
    {  3, "MINNET FULLT"                                },
    {  4, "F\x5CR STORT FLYTTAL"                        },
    {  5, "F\x5CR STORT INDEX"                          },
    {  6, "HITTAR EJ DETTA RADNUMMER"                   },
    {  7, "F\x5CR STORT HELTAL"                         },
    {  8, "FINNS EJ I DETTA SYSTEM"                     },
    {  9, "INDEX UTANF\x5CR STR\x5BNGEN"                },
    { 10, "TEXTEN F\x5DR EJ PLATS I STR\x5BNGEN"        },
    { 11, "F\x5CRST\x5DR EJ"                            },
    { 12, "FELAKTIGT TAL"                               },
    { 13, "FEL ANTAL ELLER TYP AV ARGUMENT"             },
    { 14, "OTILL\x5DTET TECKEN EFTER SATSEN"            },
    { 15, "\"=\" SAKNAS ELLER P\x5D FEL PLATS"          },
    { 16, "RADNUMMER SAKNAS"                            },
    { 17, "OTILL\x5DTEN BLANDNING AV TAL OCH STR\x5BNGAR" },
    { 18, "\")\" SAKNAS ELLER P\x5D FEL PLATS"          },
    { 19, "KAN EJ \x5CPPNA FLER FILER"                  },
    { 20, "F\x5CR L\x5DNG RAD"                          },
    { 21, "HITTAR EJ FILEN"                             },
    { 22, "OTILL\x5DTEN SATS"                           },
    { 23, "\"TO\" SAKNAS"                               },
    { 24, "\"NEXT\" SAKNAS"                             },
    { 25, "FELAKTIG SATS EFTER \"ON\""                  },
    { 26, "FEL I ON-UTTRYCK"                            },
    { 27, "\"NEXT\" UTAN \"FOR\""                       },
    { 28, "FEL VARIABEL EFTER \"NEXT\""                 },
    { 29, "\"RETURN\" UTAN \"GOSUB\""                   },
    { 30, "DATA SLUT"                                   },
    { 31, "FEL DATA TILL KOMMANDO"                      },
    { 32, "FILEN EJ \x5CPPNAD"                          },
    { 33, "\"AS FILE\" SAKNAS"                          },
    { 34, "SLUT P\x5D FILEN"                            },
    { 35, "CHECKSUMMAFEL VID L\x5BSNING"                },
    { 37, "FELAKTIGT RECORDFORMAT"                      },
    { 38, "RECORDNUMMER UTANF\x5CR FILEN"               },
    { 50, "KVADRATROT UR NEGATIVT TAL"                  },
    { 51, "ENHETEN UPPTAGEN"                            },
    { 52, "EJ TILL DENNA ENHET"                         },
    { 53, "FELAKTIG RAD"                                },
    { 57, "FUNKTIONEN EJ DEFINIERAD"                    },
    { 58, "OGILTIGT TECKEN INL\x5BST"                   },
    { 59, "FEL PROGRAMFORMAT"                           },
    { 60, "BIT ADRESS 16 BITAR"                         },
    { 61, "KOMMA SAKNAS"                                },
    { 62, "DOT-ADRESS UTANF\x5CR SK\x5BRMEN"            },
    { 63, "\"AS\" SAKNAS"                               },
    { 64, "FELAKTIG \"RENAME\""                         },
    { 65, "SPILL I ASCII-ARITMETIK"                     },
    { 66, "STR\x5BNG EJ NUMERISK"                       },
    { 0xFF, NULL }   /* sentinel */
};

// Look up an error message by code. Returns NULL if not found.
static inline const char *abc80_error_msg(uint8_t code) {
    for (const abc80_err_t *e = abc80_errors; e->msg; e++)
        if (e->code == code) return e->msg;
    return NULL;
}
