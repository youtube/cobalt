// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_SIMD_CONVERT_YUV_TO_RGB_H_
#define COBALT_MEDIA_BASE_SIMD_CONVERT_YUV_TO_RGB_H_

#include <stddef.h>
#include <stdint.h>

#include "media/base/yuv_convert.h"

namespace media {

// These methods are exported for testing purposes only.  Library users should
// only call the methods listed in yuv_convert.h.

MEDIA_EXPORT void ConvertYUVToRGB32_C(const uint8_t* yplane,
                                      const uint8_t* uplane,
                                      const uint8_t* vplane, uint8_t* rgbframe,
                                      int width, int height, int ystride,
                                      int uvstride, int rgbstride,
                                      YUVType yuv_type);

MEDIA_EXPORT void ConvertYUVToRGB32Row_C(const uint8_t* yplane,
                                         const uint8_t* uplane,
                                         const uint8_t* vplane,
                                         uint8_t* rgbframe, ptrdiff_t width,
                                         const int16_t* convert_table);

MEDIA_EXPORT void ConvertYUVAToARGB_C(
    const uint8_t* yplane, const uint8_t* uplane, const uint8_t* vplane,
    const uint8_t* aplane, uint8_t* rgbframe, int width, int height,
    int ystride, int uvstride, int avstride, int rgbstride, YUVType yuv_type);

MEDIA_EXPORT void ConvertYUVAToARGBRow_C(const uint8_t* yplane,
                                         const uint8_t* uplane,
                                         const uint8_t* vplane,
                                         const uint8_t* aplane,
                                         uint8_t* rgbframe, ptrdiff_t width,
                                         const int16_t* convert_table);

MEDIA_EXPORT void ConvertYUVToRGB32_SSE(const uint8_t* yplane,
                                        const uint8_t* uplane,
                                        const uint8_t* vplane,
                                        uint8_t* rgbframe, int width,
                                        int height, int ystride, int uvstride,
                                        int rgbstride, YUVType yuv_type);

MEDIA_EXPORT void ConvertYUVAToARGB_MMX(
    const uint8_t* yplane, const uint8_t* uplane, const uint8_t* vplane,
    const uint8_t* aplane, uint8_t* rgbframe, int width, int height,
    int ystride, int uvstride, int avstride, int rgbstride, YUVType yuv_type);

MEDIA_EXPORT void ScaleYUVToRGB32Row_C(const uint8_t* y_buf,
                                       const uint8_t* u_buf,
                                       const uint8_t* v_buf, uint8_t* rgb_buf,
                                       ptrdiff_t width, ptrdiff_t source_dx,
                                       const int16_t* convert_table);

MEDIA_EXPORT void LinearScaleYUVToRGB32Row_C(const uint8_t* y_buf,
                                             const uint8_t* u_buf,
                                             const uint8_t* v_buf,
                                             uint8_t* rgb_buf, ptrdiff_t width,
                                             ptrdiff_t source_dx,
                                             const int16_t* convert_table);

MEDIA_EXPORT void LinearScaleYUVToRGB32RowWithRange_C(
    const uint8_t* y_buf, const uint8_t* u_buf, const uint8_t* v_buf,
    uint8_t* rgb_buf, int dest_width, int source_x, int source_dx,
    const int16_t* convert_table);

}  // namespace media

// Assembly functions are declared without namespace.
extern "C" {

// We use ptrdiff_t instead of int for yasm routine parameters to portably
// sign-extend int. On Win64, MSVC does not sign-extend the value in the stack
// home of int function parameters, and yasm routines are unaware of this lack
// of extension and fault.  ptrdiff_t is portably sign-extended and fixes this
// issue on at least Win64.  The C-equivalent RowProc versions' prototypes
// include the same change to ptrdiff_t to reuse the typedefs.

MEDIA_EXPORT void ConvertYUVAToARGBRow_MMX(const uint8_t* yplane,
                                           const uint8_t* uplane,
                                           const uint8_t* vplane,
                                           const uint8_t* aplane,
                                           uint8_t* rgbframe, ptrdiff_t width,
                                           const int16_t* convert_table);

MEDIA_EXPORT void ConvertYUVToRGB32Row_SSE(const uint8_t* yplane,
                                           const uint8_t* uplane,
                                           const uint8_t* vplane,
                                           uint8_t* rgbframe, ptrdiff_t width,
                                           const int16_t* convert_table);

MEDIA_EXPORT void ScaleYUVToRGB32Row_SSE(const uint8_t* y_buf,
                                         const uint8_t* u_buf,
                                         const uint8_t* v_buf, uint8_t* rgb_buf,
                                         ptrdiff_t width, ptrdiff_t source_dx,
                                         const int16_t* convert_table);

MEDIA_EXPORT void ScaleYUVToRGB32Row_SSE2_X64(const uint8_t* y_buf,
                                              const uint8_t* u_buf,
                                              const uint8_t* v_buf,
                                              uint8_t* rgb_buf, ptrdiff_t width,
                                              ptrdiff_t source_dx,
                                              const int16_t* convert_table);

MEDIA_EXPORT void LinearScaleYUVToRGB32Row_SSE(
    const uint8_t* y_buf, const uint8_t* u_buf, const uint8_t* v_buf,
    uint8_t* rgb_buf, ptrdiff_t width, ptrdiff_t source_dx,
    const int16_t* convert_table);

MEDIA_EXPORT void LinearScaleYUVToRGB32Row_MMX_X64(
    const uint8_t* y_buf, const uint8_t* u_buf, const uint8_t* v_buf,
    uint8_t* rgb_buf, ptrdiff_t width, ptrdiff_t source_dx,
    const int16_t* convert_table);

}  // extern "C"

#endif  // COBALT_MEDIA_BASE_SIMD_CONVERT_YUV_TO_RGB_H_
