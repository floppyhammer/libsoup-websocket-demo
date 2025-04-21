#pragma once

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

char* load_wav_c(const char* filename, guint8* channels, gint32* sampleRate, guint8* bitsPerSample, int* size);

#ifdef __cplusplus
}
#endif
