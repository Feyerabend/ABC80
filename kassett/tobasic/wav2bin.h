#ifndef _WAVBIN_H
#define _WAVBIN_H

#define FALSE 0
#define TRUE 1

#define DEFAULT_PROGNAME "compiler"
#define USAGE "%s [-v] [-f hexflag] [-i inputfile] [-o outputfile] [-h]"
#define ERR_FOPEN_INPUT "fopen(input, rb)"
#define ERR_FOPEN_OUTPUT "fopen(output, wb)"
#define ERR_CONVERSION "conversion error"
#define OPTSTR "vi:o:f:h"

/* WAVE file header format */
struct HEADER {
    unsigned char riff[4];
    unsigned int overall_size;
    unsigned char wave[4];
    unsigned char fmt_chunk_marker[4];
    unsigned int length_of_fmt;
    unsigned int format_type;
    unsigned int channels;
    unsigned int sample_rate;
    unsigned int byterate;
    unsigned int block_align;
    unsigned int bits_per_sample;
    unsigned char data_chunk_header[4];
    unsigned int data_size;
};

/* File handling options - replaced uint32_t with ANSI unsigned long */
typedef struct options_t {
    int verbose;
    unsigned long flags;
    FILE *input;
    FILE *output;
} options_t;

/* ANSI standard extern configurations for getopt */
extern char* optarg;
extern int opterr;
extern int optind;

#endif
