#include "audio_convert.h"

Img* audio_2_image (Audio *pAudio)
{
	if (!pAudio)
		return NULL;
	
	unsigned int length = pAudio->get_length ();
	unsigned int w = 1024;
	unsigned int h = 320;
	unsigned int samples = (unsigned int)(length / w);
	
	float *data = (float*)malloc(w*sizeof(float));
	memset (data, 0, w*sizeof(float));
	for (int i=0; i<w; i++)
		for (int s=0; s<samples; s++)
			for (int c=0; c<pAudio->channels; c++)
				data[i] += pAudio->data[pAudio->channels*(i+s)+c];
	
	
	float min = data[0];
	float max = data[0];
	for (int i=1; i<w; i++)
	{
		if (min > data[i]) min = data[i];
		if (max < data[i]) max = data[i];
	}

	Img *img = new Img (w, h);
	
	for (int i=0; i<w; i++)
	{
		float d = data[i];
		int hd = h*(d-min)/(max-min);
		for (int j=0; j<hd; j++)
			img->set_pixel (i, j, 0, 0, 255, 255);
		for (int j=hd; j<h; j++)
			img->set_pixel (i, j, 255, 0, 0, 255);
	}
	
	free (data);
	
	return img;
}

Img* audio_fft_2_image (Audio *pAudio)
{
	return NULL;
}


