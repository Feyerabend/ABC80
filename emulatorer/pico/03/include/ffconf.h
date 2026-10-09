/*---------------------------------------------------------------------------
 * FatFs configuration for ABC80 Pico (single-volume, no RTC, no threads)
 *---------------------------------------------------------------------------*/
#define FFCONF_DEF      80286   /* must match FF_DEFINED in ff.h */

#define FF_FS_READONLY  0
#define FF_FS_MINIMIZE  0
#define FF_USE_FIND     0
#define FF_USE_MKFS     0
#define FF_USE_FASTSEEK 0
#define FF_USE_EXPAND   0
#define FF_USE_CHMOD    0
#define FF_USE_LABEL    1       /* f_getlabel() for FI command */
#define FF_USE_FORWARD  0
#define FF_USE_STRFUNC  0
#define FF_PRINT_LLI    0
#define FF_PRINT_FLOAT  0
#define FF_STRF_ENCODE  0

#define FF_CODE_PAGE    437     /* US — handles standard 8.3 names fine */

#define FF_USE_LFN      1       /* LFN in BSS (not thread-safe, OK for us) */
#define FF_MAX_LFN      255
#define FF_LFN_UNICODE  0       /* char-based API */
#define FF_LFN_BUF      255
#define FF_SFN_BUF      12

#define FF_FS_RPATH     0       /* no relative paths */

#define FF_VOLUMES      1
#define FF_STR_VOLUME_ID    0
#define FF_MULTI_PARTITION  0
#define FF_MIN_SS       512
#define FF_MAX_SS       512
#define FF_LBA64        0
#define FF_MIN_GPT      0x10000000
#define FF_USE_TRIM     0
#define FF_FS_TINY      0
#define FF_FS_EXFAT     0

#define FF_FS_NORTC     1       /* no RTC — use fixed timestamp */
#define FF_NORTC_MON    1
#define FF_NORTC_MDAY   1
#define FF_NORTC_YEAR   2025

#define FF_FS_NOFSINFO  0
#define FF_FS_LOCK      0
#define FF_FS_REENTRANT 0
#define FF_FS_TIMEOUT   1000
