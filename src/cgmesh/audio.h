#ifndef __AUDIO_H__
#define __AUDIO_H__

class Audio
{
public:
	Audio ();
	~Audio ();

	int init (unsigned int _length, unsigned int _channels, unsigned int _rate);
	int load (char *filename);

	unsigned int get_length (void) { return length; };
	unsigned int get_rate (void) { return rate; };
	void get_spectrum (unsigned int start, unsigned int length, float *spectrum);

	void dump (void);

public:
	int import_wav (char const *filename, int verbose);

	unsigned int length;
	unsigned int rate;
	unsigned int channels;
	float *data;
};

#endif // __AUDIO_H__

