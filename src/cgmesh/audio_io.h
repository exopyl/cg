#ifndef __WAV_H__
#define __WAV_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "audio.h"

audio_t *import_wav(const char *filename, int verbose);

#ifdef __cplusplus
}
#endif

#endif // __WAV_H__
