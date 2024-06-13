/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file iamf_bear_api.h
 * @brief API header for IAMF decoder to BEAR renderer Interface.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef IANF_BEAR_API_H_
#define IANF_BEAR_API_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __linux__
#define EXPORT_API
#elif defined __APPLE__
#define EXPORT_API
#elif defined _WIN32
// EXPORT_API can be used to define the dllimport storage-class attribute.
#ifdef DLL_EXPORTS
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

EXPORT_API void* CreateBearAPI(char* tf_data_path);
EXPORT_API void DestoryBearAPI(void* pv_thiz);
EXPORT_API int ConfigureBearDirectSpeakerChannel(void* pv_thiz, int layout,
                                                 size_t nsample_per_frame,
                                                 int sample_rate);
EXPORT_API int SetBearDirectSpeakerChannel(void* pv_thiz, int source_id,
                                           float** in);
// EXPORT_API int ConfigureBearObjectChannel(...);
// EXPORT_API void SetBearObjectChannel(...);
// EXPORT_API int ConfigureBearHOAChannel(...);
// EXPORT_API void SetBearHOAChannel(...);
EXPORT_API void DestoryBearChannel(void* pv_thiz, int source_id);
EXPORT_API int GetBearRenderedAudio(void* pv_thiz, int source_id, float** out);

#ifdef __cplusplus
}
#endif
#endif
