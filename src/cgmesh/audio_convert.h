#ifndef __AUDIO_CONVERT_H__
#define __AUDIO_CONVERT_H__

#include "audio.h"
#include "mesh.h"
#include "../cgimg/cgimg.h"

extern Img* audio_2_image (Audio *pAudio);
extern Img* audio_fft_2_image (Audio *pAudio);


#endif // __AUDIO_CONVERT_H__
