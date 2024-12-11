#include <stdio.h>
#include <stdlib.h>

#include "audio.h"
#include "audio_rfft.h"

Audio::Audio ()
{
     data = NULL;
}

Audio::~Audio()
{
     if (data)
	  free (data);
}

int Audio::init (unsigned int _length, unsigned int _channels, unsigned int _rate)
{
     length = _length;
     channels = _channels;
     rate = _rate;
     if (length * channels)
	  data = (float *) calloc(length * channels, sizeof(float));
     else
	  data = NULL;
     
     return 0;
}

void Audio::get_spectrum (unsigned int start, unsigned int length, float *spectrum)
{
	unsigned int i, j;

	if (start > length || start + length > length)
		return;

	// mix all channels
	for (i = 0; i < length; i++) {
		float s = 0.0;
		for (j = 0; j < channels; j++)
			s += data[(start + i) * channels + j];
		s /= channels;
		spectrum[i] = s;
	}

	// compute spectrum via FFT
	rfft(spectrum, length);
}

void Audio::dump (void)
{
	printf ("length : %d\n", length);
	printf ("rate : %d\n", rate);
	printf ("channels : %d\n", channels);
}

