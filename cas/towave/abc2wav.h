#ifndef _ABCWAV_H
#define _ABCWAV_H

#define FALSE 0
#define TRUE 1

#define DEFAULT_PROGNAME "abccas"
#define USAGE "%s [-v] [-f hexflag] [-i inputfile] [-o outputfile] [-h]"
#define ERR_FOPEN_INPUT "fopen(input, r)"
#define ERR_FOPEN_OUTPUT "fopen(output, w)"
#define ERR_CONVERSION "conversion error"
#define OPTSTR "vi:o:f:he:"

enum {
    ERR_TEST = 0x0001,

    ERROR_INPUT_OPTIONS = 0x0407,
    ERROR_FILE_OPTIONS = 0x0408
} ERROR;

// file handling
typedef struct options_t {
    int verbose;
    uint32_t flags;
    char *filename;
    char *inputname;
    char *outputname;
    FILE *input, *output;
} options_t;

extern int errno;
extern char* optarg;
extern int opterr, optind;

#endif

