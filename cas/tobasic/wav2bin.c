/**
 * Read and parse a wave file
 * http://truelogic.org/wordpress/2015/09/04/parsing-a-wav-file-in-c/
 **/
// also have a look at https://gist.github.com/SteelPh0enix/e44d4a030dd8816309af84809ed75604

// cleanup of code
// added ABC80 interpretation of tape encoding in frequency modulated signals
// S. Lonnert 2023

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <libgen.h>

#include "wav2bin.h"

#define TRUE 1 
#define FALSE 0

options_t options = { 0, 0x0, NULL, NULL };

// temp buffers for endian conversion (little to big)
unsigned char buffer4[4];
unsigned char buffer2[2];

// WAVE header structure
struct HEADER header;

char *filename;

// globals to keep state while reading signals
int previous = FALSE;
int threshold = 42; // suitable for 44,1 Hz ? alternative for juhanz?

int bit(int count) {
	// zero confirmed
	if (count > threshold) {
		return 0;
	}
	// full one confirmed
	else if (count < threshold && previous == TRUE) {
		previous = FALSE; // .. so reset
		return 1;
	}
	// half of one confirmed
	else if (count < threshold) {
		previous = TRUE; // ok, halfways ..
		return -1;
	}
	else {
		printf("ERROR"); // somethings wrong with (recording of) "tape"
		return -2; // ERROR
	}
}


// make sure that the bytes-per-sample is completely divisible by num.of channels
int check_size(long size_of_each_sample, int header) {
	long bytes_in_each_channel = (size_of_each_sample / header);

	if ((bytes_in_each_channel * header) != size_of_each_sample) {
		printf("error for size: %ld x %ud <> %ld\n", bytes_in_each_channel, header, size_of_each_sample);
		return FALSE;
	}
	return TRUE;
}

// check out the headers of the wave-file
int headers(FILE *in) {
	int read = 0;

	printf("decoding header ..\n");

	// RIFF
	read = fread(header.riff, sizeof(header.riff), 1, in);
	printf("(01-04) %s \n", header.riff); 

	// overall size
	read = fread(buffer4, sizeof(buffer4), 1, in);
	header.overall_size  = buffer4[0] | (buffer4[1] << 8) | (buffer4[2] << 16) | (buffer4[3] << 24);
	printf("(05-08) Overall size: %u bytes, %u Kb\n", header.overall_size, header.overall_size / 1024);

	// wave marker
	read = fread(header.wave, sizeof(header.wave), 1, in);
	printf("(09-12) Wave marker: %s\n", header.wave);

	// fmt marker
	read = fread(header.fmt_chunk_marker, sizeof(header.fmt_chunk_marker), 1, in);
	printf("(13-16) Fmt marker: %s\n", header.fmt_chunk_marker);

	// fmt header length 16 for PCM
	read = fread(buffer4, sizeof(buffer4), 1, in);
	header.length_of_fmt = buffer4[0] | (buffer4[1] << 8) | (buffer4[2] << 16) | (buffer4[3] << 24);
	printf("(17-20) Length of Fmt header: %u \n", header.length_of_fmt); // 0x10, 0, 0, 0

	// format type: PCM = 1, other than 1 indicate some form of compression
	read = fread(buffer2, sizeof(buffer2), 1, in);
 	header.format_type = buffer2[0] | (buffer2[1] << 8);
	if (header.format_type == 1)
		printf("(21-22) Format type: %u %s \n", header.format_type, "PCM");
	else {
		printf("this program doesn't handle other formats than PCM"); // abort?
		return FALSE;
	}

	// channels
	read = fread(buffer2, sizeof(buffer2), 1, in);
	header.channels = buffer2[0] | (buffer2[1] << 8);
	printf("(23-24) Channels: %u \n", header.channels);

	// sample_rate
	read = fread(buffer4, sizeof(buffer4), 1, in);
	header.sample_rate = buffer4[0] | (buffer4[1] << 8) | (buffer4[2] << 16) | (buffer4[3] << 24);
	printf("(25-28) Sample rate: %u\n", header.sample_rate);

	// byte/bit rate
	read = fread(buffer4, sizeof(buffer4), 1, in);
	header.byterate = buffer4[0] | (buffer4[1] << 8) | (buffer4[2] << 16) | (buffer4[3] << 24);
	printf("(29-32) Byte rate: %u, bit rate: %u\n", header.byterate, header.byterate * 8);

	// block_align
	read = fread(buffer2, sizeof(buffer2), 1, in);
	header.block_align = buffer2[0] |	(buffer2[1] << 8);
	printf("(33-34) Block alignment: %u \n", header.block_align);

	// bits per sample
	read = fread(buffer2, sizeof(buffer2), 1, in);
	header.bits_per_sample = buffer2[0] | (buffer2[1] << 8);
	printf("(35-36) Bits per sample: %u \n", header.bits_per_sample);

	// data chunk header
	read = fread(header.data_chunk_header, sizeof(header.data_chunk_header), 1, in);
	printf("(37-40) Data marker: %s \n", header.data_chunk_header);

	// data size
	read = fread(buffer4, sizeof(buffer4), 1, in);
	header.data_size = buffer4[0] | (buffer4[1] << 8) | (buffer4[2] << 16) | (buffer4[3] << 24);
	printf("(41-44) Size of data chunk: %u \n", header.data_size);

	// number of samples
	long num_samples = (8 * header.data_size) / (header.channels * header.bits_per_sample);
	printf("number of samples: %lu \n", num_samples);

	// size of each sample
	long size_of_each_sample = (header.channels * header.bits_per_sample) / 8;
	printf("size of each sample: %ld bytes\n", size_of_each_sample);

	// duration of file
	float duration_in_seconds = (float) header.overall_size / header.byterate;
	printf("approximate duration: %f seconds.\n", duration_in_seconds);
//	printf("(h:m:s = %s)\n", seconds_to_time(duration_in_seconds)); // remake?

	// guesswork ~ half rate?
	if (header.sample_rate < 44000) {
		threshold = 21;
		printf("changing threshold for lower sample rate to %d", threshold);
	}

	int chk = check_size(size_of_each_sample, header.channels);
	if (!chk)
		return FALSE;

	printf("done.\n");
	return TRUE;
}

// read sample and check for signal within range
int samples(FILE *in, int size_of_each_sample) {
	char data_buffer[size_of_each_sample];

	int read = fread(data_buffer, sizeof(data_buffer), 1, in);
	if (read != 1) {
		printf("error reading file\n"); // abort
		return FALSE;
	}

	int data_in_channel;
	data_in_channel = data_buffer[0] & 0x00ff;
	data_in_channel -= 128; // in wave, 8-bit are unsigned, so shifting to signed

	if (data_in_channel < -128 || data_in_channel > 127) {
		printf("data_in_channel out of range\n"); // continue?
		return FALSE;
	}

	return data_in_channel;
}

// signals if there is a change at the edge
int signedflag(FILE *in, int size_of_each_sample) {
	int data_flag = 0;
	int data_in_channel = samples(in, size_of_each_sample);
	if (data_in_channel == FALSE)
		return FALSE;
	if (data_in_channel < 0)
		data_flag = -1;
	else if (data_in_channel > 0)
		data_flag = 1;

	return data_flag;
}

// spit out the binary as decimals (for easy handling)
void bitout(int b, FILE *out) {
	if (b == 0) {
		fprintf(out, "%d", 0);
		return;
	}
	if (b == 1) {
		fprintf(out, "%d", 1);
		return;
	}
	if (b == -2)
		; // return FALSE;
}

// main handling
int process(FILE *in, FILE *out, int verbose) {

	// read header parts and compose locals (again, also in headers)
	long num_samples = (8 * header.data_size) / (header.channels * header.bits_per_sample);
	long size_of_each_sample = (header.channels * header.bits_per_sample) / 8;

	// some locals
	int data_flag = 0;
	int old_data_flag = 0;
	int count_data = 0;

	// go through all samples
	for (int i = 0; i < num_samples; i++) {
		data_flag = signedflag(in, size_of_each_sample);
		if (data_flag == FALSE)
			return FALSE;
		// check to see if the flank has changed
		if (old_data_flag != data_flag) {
			// if something changed, check for a zero or one
			// write bit
			bitout(bit(count_data), out);
			count_data = 0;
		} else {
			// how many times same signal (usually indicates bit 0 or 1)
			++count_data;
		}
		old_data_flag = data_flag;
	}
	bitout(bit(count_data), out);

	if (verbose)
		printf("binary saved.");

	return TRUE;
}

// general function
void usage(char *progname, int opt) {
    fprintf(stderr, USAGE, progname ? progname : DEFAULT_PROGNAME);
    exit(EXIT_FAILURE);
}

// requirements for mandatory input and options
int converting(options_t *options) {

    if (!options) {
        //errno = EINVAL;
        return EXIT_FAILURE;
    }

    if (!options->input || !options->output) {
        //errno = ENOENT;
        return EXIT_FAILURE;
    }

    // the program in a nutshell
	int h = headers(options->input);
	if (h) process(options->input, options->output, options->verbose);

    return EXIT_SUCCESS;
}

// start with arguments at the command line ..
int main(int argc, char *argv[]) {
    int opt;
    opterr = 0;
    options.input = stdin;
    options.output = stdout;

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
                break;

            case 'f': // limit error messages
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

//	free(options.input);
//	free(options.output);

    return EXIT_SUCCESS;
}

