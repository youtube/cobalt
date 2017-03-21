// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_SIMD_CONVERT_RGB_TO_YUV_H_
#define COBALT_MEDIA_BASE_SIMD_CONVERT_RGB_TO_YUV_H_

#include <stdint.h>

#include "media/base/yuv_convert.h"

namespace media {

// These methods are exported for testing purposes only.  Library users should
// only call the methods listed in yuv_convert.h.

MEDIA_EXPORT void ConvertRGB32ToYUV_SSSE3(const uint8_t* rgbframe,
                                          uint8_t* yplane, uint8_t* uplane,
                                          uint8_t* vplane, int width,
                                          int height, int rgbstride,
                                          int ystride, int uvstride);

MEDIA_EXPORT void ConvertRGB24ToYUV_SSSE3(const uint8_t* rgbframe,
                                          uint8_t* yplane, uint8_t* uplane,
                                          uint8_t* vplane, int width,
                                          int height, int rgbstride,
                                          int ystride, int uvstride);

MEDIA_EXPORT void ConvertRGB32ToYUV_SSE2(const uint8_t* rgbframe,
                                         uint8_t* yplane, uint8_t* uplane,
                                         uint8_t* vplane, int width, int height,
                                         int rgbstride, int ystride,
                                         int uvstride);

MEDIA_EXPORT void ConvertRGB32ToYUV_SSE2_Reference(
    const uint8_t* rgbframe, uint8_t* yplane, uint8_t* uplane, uint8_t* vplane,
    int width, int height, int rgbstride, int ystride, int uvstride);

MEDIA_EXPORT void ConvertRGB32ToYUV_C(const uint8_t* rgbframe, uint8_t* yplane,
                                      uint8_t* uplane, uint8_t* vplane,
                                      int width, int height, int rgbstride,
                                      int ystride, int uvstride);

MEDIA_EXPORT void ConvertRGB24ToYUV_C(const uint8_t* rgbframe, uint8_t* yplane,
                                      uint8_t* uplane, uint8_t* vplane,
                                      int width, int height, int rgbstride,
                                      int ystride, int uvstride);

}  // namespace media

#endif  // COBALT_MEDIA_BASE_SIMD_CONVERT_RGB_TO_YUV_H_
