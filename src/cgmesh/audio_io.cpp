#include <stdio.h>
#include <math.h>
#include <string.h>

#include "../cgmath/cgmath.h"

#include "audio.h"

int Audio::load (char *filename)
{
	if (!filename)
		return -1;

	if (strcmp (filename+(strlen(filename)-4), ".wav") == 0)
	     return import_wav (filename, 1);

	return -1;
}

//
// WAV
//
// https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
//
typedef struct {
     char chunkID[4]; // "RIFF"
     unsigned int Chunksize;
     char Format[4]; // "WAVE"

     char Subchunk1ID[4]; // "fmt "
     unsigned int Subchunk1Size;
     unsigned short AudioFormat;
     unsigned short NumChannels;
     unsigned int samplerate;
     unsigned int bytespersec;
     unsigned short blocksize;
     unsigned short bps;

     char Subchunk2ID[4]; // "data"
     unsigned int Subchunk2Size;
} wav_hdr_t;

static float get_sample(FILE *f, unsigned int bps)
{
	int c1 = fgetc(f);
	if (c1 == EOF)
		return NAN;
	if (bps == 8)
		return c1 / 128.;
	// little-endian signed
	int c2 = fgetc(f);
	if (c2 == EOF)
		return NAN;
	return (((int)((signed char)c2) << 8) | c1) / 32768.;
}

int Audio::import_wav (char const *filename, int verbose)
{
	wav_hdr_t hdr;

	FILE *f = fopen(filename, "rb");

	if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
		printf ("could not read %s\n", filename);
		fclose(f);
		return NULL;
	}

	if (verbose)
	{
	     printf("rate %d\n", hdr.samplerate);
	     printf("bps %d\n", hdr.bps);
	     printf("NumChannels %d\n", hdr.NumChannels);
	     printf("Chunksize %d\n", hdr.Chunksize);
	     printf("Subchunk1Size %d\n", hdr.Subchunk1Size);
	}
/*	
	while (hdr.Subchunk2ID[0] != 'd' ||
	       hdr.Subchunk2ID[1] != 'a' ||
	       hdr.Subchunk2ID[2] != 't' ||
	       hdr.Subchunk2ID[3] != 'a')
	{
		printf("data: %c%c%c%c\n", hdr.Subchunk2ID[0], hdr.Subchunk2ID[1], hdr.Subchunk2ID[2], hdr.Subchunk2ID[3]);
		printf("Subchunk2Size %d\n", hdr.Subchunk2Size);

		if (fseek(f, hdr.Subchunk2Size, SEEK_CUR) == -1) {
			printf("could not read %s\n", filename);
			fclose(f);
			return NULL;
		}

		if (fread(&hdr.Subchunk2ID, sizeof(hdr.Subchunk2ID) + sizeof(hdr.Subchunk2Size), 1, f) != 1) {
			printf("could not read %s\n", filename);
			fclose(f);
			return NULL;
		}
	}
*/

	if (verbose)
	{
	     printf("Subchunk2ID: \"%c%c%c%c\"\n", hdr.Subchunk2ID[0], hdr.Subchunk2ID[1], hdr.Subchunk2ID[2], hdr.Subchunk2ID[3]);
	     printf("datasize %d\n", hdr.Subchunk2Size);
	}

	if (hdr.bps != 16 && hdr.bps != 8) {
		printf("invalid format (%d bps)\n", hdr.bps);
		fclose(f);
		return NULL;
	}
  
	// convert audio data to NSArray
	unsigned int datasize = hdr.Subchunk2Size;
	unsigned int NumChannels = hdr.NumChannels;
	unsigned int length = datasize / (hdr.NumChannels * hdr.bps / 8);

	init (length, NumChannels, hdr.samplerate);
	unsigned int i, j;
	for (i = 0; i < length; i++) {
		for (j = 0; j < NumChannels; j++) {
			float v = get_sample(f, hdr.bps);
			if (isnan(v)) {
				printf("unexpected end of file %s\n", filename);
				fclose(f);
				return -1;
			}
			data[i * NumChannels + j] = v;
		}
	}

	fclose(f);
	return 0;
}
//
///////////////////////
