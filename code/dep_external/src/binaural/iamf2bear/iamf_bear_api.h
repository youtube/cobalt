/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
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

#if defined(_WIN32)
// EXPORT_API can be used to define the dllimport storage-class attribute.
#ifdef DLL_EXPORTS
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif
#else
#define EXPORT_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

EXPORT_API void* CreateBearAPI(char* tf_data_path);
EXPORT_API void DestroyBearAPI(void* pv_thiz);
EXPORT_API int ConfigureBearDirectSpeakerChannel(void* pv_thiz, int layout,
                                                 size_t nsample_per_frame,
                                                 int sample_rate);
EXPORT_API int SetBearDirectSpeakerChannel(void* pv_thiz, int source_id,
                                           float** in);
// EXPORT_API int ConfigureBearObjectChannel(...);
// EXPORT_API void SetBearObjectChannel(...);
// EXPORT_API int ConfigureBearHOAChannel(...);
// EXPORT_API void SetBearHOAChannel(...);
EXPORT_API void DestroyBearChannel(void* pv_thiz, int source_id);
EXPORT_API int GetBearRenderedAudio(void* pv_thiz, int source_id, float** out);

#ifdef __cplusplus
}
#endif
#endif
