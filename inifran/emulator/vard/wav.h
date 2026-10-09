/*
 * wav.h -- kassettbandet som WAV i minnet, till boken ABC80 inifrån
 *
 * Kärnan (karna/kassett.c) ser bandet som flanker med sin plats i
 * T-cykler. wav.c gör om en WAV-fil i minnet till flanker och flanker
 * till en WAV-fil i minnet, utan filer och utan malloc, så att samma
 * kod går i terminalen (kassettfil.c) och i webbläsaren (webb/webb.c).
 */

#ifndef WAV_H
#define WAV_H

#include <stdint.h>

/* Flankerna i WAV-filen fil (storlek bytes). Ger antalet flanker; bara
 * de storsta första skrivs i flanker, och med flanker == NULL räknas de
 * bara. Vid fel -1, med en förklaring i *fel. */
long wav_till_flanker(const uint8_t *fil, long storlek,
                      long long *flanker, long storsta, const char **fel);

/* Flankerna som en WAV-fil som räcker till slut och en halv sekund
 * till. Ger antalet bytes; med ut == NULL räknas de bara. */
long wav_fran_flanker(const long long *flanker, long antal, long long slut,
                      uint8_t *ut);

#endif
