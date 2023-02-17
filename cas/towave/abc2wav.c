/***************************************************
 * ABCcas - by Robert Juhasz, 2008
 *
 * usage: abccas fil.bac
 *
 * generates fil.bac.WAV which can be loaded by ABC80 (LOAD CAS:)
 * Filename in wav-file is always TESTTT.BAC
 ****************************************************/
/*
 *        BIN to ABC .BAS file
 *        Stefano Bodrato 5/2000
 *
 *        WAV conversion taken from ABCcas - by Robert Juhasz, 2008
 *
 *        $Id: abc80.c$
 */
// cleanup S. Lonnert 2023

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h> 
#include <libgen.h>

#include "abc2wav.h"

options_t options = { 0, 0x0, NULL, NULL };

// out options
int faster = FALSE; // somewhat faster, louder, higher .. error prone?
int juhasz = FALSE; // if true, patches for Juhasz original version, overrides faster if set

char filename[11] = "        BAS";


unsigned char buffer[256];
//char outname[80];
static int outbit = 150;
static int ofs = 40;

// publish the bit for a "while": period
void bitout(unsigned char b, FILE* f) {
    int i, period;

    if (faster)
		period = 28;
	else
		period = 29;

	if (juhasz)
		period = 16;

	for (i = 0; i < period; i++) {
        fputc(b, f);
	}
}

// a full byte to publish
// n.b. it will be in "reverse order"
// as higher bits are added later (to the right)
void byteout(unsigned char b, FILE* f) {
    int i, t = 1;

    for (i = 0; i < 8; i++) {
        if (outbit)
            outbit = 0;
        else
            outbit = 150; /* flip output bit */
		bitout(outbit + ofs, f);

        if (t & b) {
            /* send "1" */
            if (outbit)
                outbit = 0;
            else
                outbit = 150; /* flip output bit again if "1" */
        }
		bitout(outbit + ofs, f);

        t *= 2; /* next bit */
    }
}

// a general block (for name block and data block)
void blockout(FILE* fout) {
    int i, csum;

    for (i = 0; i < 32; i++)
        byteout(0, fout); /* 32 0 bytes or 256 bits 0 */
    for (i = 0; i < 3; i++)
        byteout(0x16, fout); /* 3 bytes 16H SYNC */

    byteout(0x2, fout); /* STX 2H */
    for (i = 0; i < 256; i++) {
    	byteout(buffer[i], fout); /* name or data */
    }
    byteout(0x3, fout); /* ETX 3H */

    csum = 3; /* csum includes ETX char!!! (as correctly stated in Mikrodatorns ABC) */
    for (i = 0; i < 256; i++)
        csum += buffer[i]; /* calculate checksum */

    /* 2 byte binary checksum */
    byteout(csum & 0xff, fout);
    byteout((csum >> 8) & 0xff, fout);
}

// name block doesn't contain much
void nameblockout(FILE* fout) {
    int i;

    for (i = 0; i < 3; i++)
        buffer[i] = 0xff; /* Header */

    for (i = 3; i < 14; i++)
        buffer[i] = filename[i - 3]; /* Name */

    for (i = 14; i < 256; i++)
        buffer[i] = 0; /* zeroes */

    blockout(fout);
}

// datablock for all data
void datablockout(FILE* fin, FILE* fout) {
    int blcnt = 0;
    int numbyte= 0;

    while (!feof(fin)) {
        memset(buffer, 0, sizeof(buffer));

        buffer[0] = 0; // always null
        buffer[1] = blcnt & 0xff;
        buffer[2] = (blcnt >> 8) & 0xff;
		numbyte = fread(&buffer[3], 253, 1, fin);
        if (numbyte != 1)
            ;
        blockout(fout);
        blcnt++;
    }
}

// start the output file with a header of Wave
void wavheaderout(FILE *f, int numsamp) {
	int totlen, srate;
	fprintf(f, "%s", "RIFF");

	totlen = 12 - 8 + 24 + 8 + numsamp;
	fputc((totlen & 0xff), f);
	fputc((totlen >> 8) & 0xff, f);
	fputc((totlen >> 16) & 0xff, f);
	fputc((totlen >> 24) & 0xff, f);

	srate = 44100;
	if (juhasz) // patch
		srate = 23908; // (5977 * 2 * 2) original lower rate
	fprintf(f, "%s", "WAVE");
	fprintf(f, "%s", "fmt ");
	fprintf(f, "%c%c%c%c", 0x10, 0, 0, 0);					// format chunk length
	fprintf(f, "%c%c", 0x1, 0x0);							// always 0x1
	fprintf(f, "%c%c", 0x1, 0x0);							// here always 0x1 = Mono (0x2 = Stereo)
	fprintf(f, "%c%c%c%c", srate & 255, srate >> 8, 0, 0);	// sample Rate (Binary, in Hz)
	fprintf(f, "%c%c%c%c", srate & 255, srate >> 8, 0, 0);	// bytes /s same as sample rate if mono 8bit
	fprintf(f, "%c%c", 0x01, 0);							// bytes / sample: 1 = 8 bit Mono, 2 = 8 bit Stereo or 16 bit Mono, 4 = 16 bit Stereo
	fprintf(f, "%c%c", 0x08, 0);							// bits / sample
	fprintf(f, "%s", "data");								// data hdr (ASCII Characters)
	fprintf(f, "%c%c%c%c",
		numsamp & 0xff,
		(numsamp >> 8) & 0xff,
		(numsamp >> 16) & 0xff,
		(numsamp >> 24 ) & 0xff);							// Length Of Data To Follow

	if (faster)
		ofs = 70;
	if (juhasz)
		ofs = 64;
}

// general usage
void usage(char *progname, int opt) {
    fprintf(stderr, USAGE, progname ? progname : DEFAULT_PROGNAME);
    exit(EXIT_FAILURE);
}

//
int prepare(char *outputfile) {
    int numsamp, filelen, numblk, numbyte;
    struct stat filestat;

    // file length
    stat(outputfile, &filestat);
    filelen = filestat.st_size;

    // 253 byte user data per block?
    numblk = filelen / 253;
    if (filelen % 253)
        numblk++; // add, if not exact
    numblk++; // add one for name block 

    // 32 null, 3 sync, 1 stx, 256 data, 1 etx, 2 checksum 
    numbyte = (32 + 3 + 1 + 256 + 1 + 2) * numblk;

    int samp = 58; // default sample
    if (faster)
        samp = 56;
    if (juhasz)
        samp = 32;
    numsamp = samp * 8 * numbyte; // e.g. 32 samples per bit, 8 bits per byte

    printf("length: %d, blocks: %d, bytes: %d, samples: %d\n", filelen, numblk, numbyte, numsamp);
    return numsamp;
}


//
int converting(options_t *options) {
    FILE *fin, *fout;

    if (options->verbose)
        printf("converting ..\n");

    if (!options) {
        // errnum(ERROR_INPUT_OPTIONS);
        return EXIT_FAILURE;
    }

    if (!options->input || !options->output) {
        // errnum(ERROR_FILE_OPTIONS);
        return EXIT_FAILURE;
    }

    fin = options->input;
    fout = options->output;

    if (options->flags & 0x01)
        faster = TRUE;
    if (options->verbose)
        printf("optimized as '%s'.\n", faster ? "faster" : "default");

    if (options->flags & 0x02) {
        juhasz = TRUE;
        if (options->verbose)
            printf("overridden configuration as 'juhasz'.\n");
    }

    // if a suggested filename is given
    // copy to output
    // (as C is highly dependent on implementation details,
    // Swedish chars have not been included here)
    int opt = TRUE;
    if (*options->filename) {

        if (!(strlen(options->filename) > 8 || strlen(options->filename) < 1)) {
            char temp[9];
            strncpy(temp, options->filename, 8);

            if (strlen(temp) < 9) {
                int i, n = strlen(temp);

                for (i = 0; i < n && temp[i] != '\0'; i++) {
                    int c = temp[i];
                    int a = isalpha(c);
                    int d = isdigit(c);

                    if (i == 0) {
                        if (a)
                            filename[0] = toupper(temp[0]);
                        else
                            opt = FALSE;
                    }
                    else {
                        if (a || d)
                            filename[i] = toupper(temp[i]);
                        else
                            opt = FALSE;
                    }
                }
                for ( ; i < n; i++)
                    filename[i] = ' '; // space, 32 ASCII
            }
        }
        else {
            opt = FALSE;
        }
    }
    if (opt == FALSE)
        memcpy(filename, "DEFAULTSBAS", 11);

    // choose between BAC and BAS type depending on original file
    if (options->flags & 0x04)
        filename[10] = 67;
    
    if (options->verbose)
        printf("internal filename: %s\n", filename);

    if (options->verbose)
        printf("parsing ..\n");

    int n = prepare(options->outputname);
    wavheaderout(fout, n);
    nameblockout(fout);
    datablockout(fin, fout);

    if (options->verbose)
        printf("done.\n");

    return EXIT_SUCCESS;
}


//
int main(int argc, char *argv[]) {
    int opt;
    opterr = 0;
    options.input = stdin;
    options.output = stdout;
    options.filename = "DEFAULTSBAS";

    while ((opt = getopt(argc, argv, OPTSTR)) != EOF) {
        switch(opt) {
            case 'i':
                if (! (options.input = fopen(optarg, "r")) ) {
                    perror(ERR_FOPEN_INPUT);
                    exit(EXIT_FAILURE);
                }
                break;

            case 'o':
                if (! (options.output = fopen(optarg, "w")) ) {
                    perror(ERR_FOPEN_OUTPUT);
                    exit(EXIT_FAILURE);
                }
                options.outputname = optarg;
                break;

            case 'e':
                    options.filename = optarg;
                    // parse and put in the global filename
                    // if error parsing    exit(EXIT_FAILURE);
                break;

            case 'f':
                options.flags = (uint32_t) strtoul(optarg, NULL, 16);
                break;

            case 'v':
                options.verbose += 1;
                break;

            case 'h':
            default:
                usage(basename(argv[0]), opt);
                break;
        }
    }

    if (options.input)
        fseek(options.input, 0, SEEK_SET);

    if (converting(&options) != EXIT_SUCCESS) {
        perror(ERR_CONVERSION);
        exit(EXIT_FAILURE);
    }

    fclose(options.input);
    fclose(options.output);

    return EXIT_SUCCESS;
}

