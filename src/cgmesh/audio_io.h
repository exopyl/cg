#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "audio.h"

audio_t *import_wav(const char *filename, int verbose);

#ifdef __cplusplus
}
#endif
