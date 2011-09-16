// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SIMD_CONVERT_RGB_TO_YUV_H_
#define MEDIA_BASE_SIMD_CONVERT_RGB_TO_YUV_H_

#include "base/basictypes.h"
#include "media/base/yuv_convert.h"

namespace media {

// Converts an ARGB image to a YV12 image. This function calls ASM functions
// implemented in "convert_rgb_to_yuv_ssse3.asm" to convert the specified ARGB
// image to a YV12 image.
void ConvertRGB32ToYUV_SSSE3(const uint8* rgbframe,
                             uint8* yplane,
                             uint8* uplane,
                             uint8* vplane,
                             int width,
                             int height,
                             int rgbstride,
                             int ystride,
                             int uvstride);

// Converts an RGB image to a YV12 image. This function is almost same as
// ConvertRGB32ToYUV_SSSE3 except its first argument is a pointer to RGB pixels.
void ConvertRGB24ToYUV_SSSE3(const uint8* rgbframe,
                             uint8* yplane,
                             uint8* uplane,
                             uint8* vplane,
                             int width,
                             int height,
                             int rgbstride,
                             int ystride,
                             int uvstride);

// SSE2 version of converting RGBA to YV12.
void ConvertRGB32ToYUV_SSE2(const uint8* rgbframe,
                            uint8* yplane,
                            uint8* uplane,
                            uint8* vplane,
                            int width,
                            int height,
                            int rgbstride,
                            int ystride,
                            int uvstride);

// This is a C reference implementation of the above routine.
// This method should only be used in unit test.
// TODO(hclam): Should use this as the C version of RGB to YUV.
void ConvertRGB32ToYUV_SSE2_Reference(const uint8* rgbframe,
                                      uint8* yplane,
                                      uint8* uplane,
                                      uint8* vplane,
                                      int width,
                                      int height,
                                      int rgbstride,
                                      int ystride,
                                      int uvstride);

// C version of converting RGBA to YV12.
void ConvertRGB32ToYUV_C(const uint8* rgbframe,
                         uint8* yplane,
                         uint8* uplane,
                         uint8* vplane,
                         int width,
                         int height,
                         int rgbstride,
                         int ystride,
                         int uvstride);

// C version of converting RGB24 to YV12.
void ConvertRGB24ToYUV_C(const uint8* rgbframe,
                         uint8* yplane,
                         uint8* uplane,
                         uint8* vplane,
                         int width,
                         int height,
                         int rgbstride,
                         int ystride,
                         int uvstride);

}  // namespace media

#endif  // MEDIA_BASE_SIMD_CONVERT_RGB_TO_YUV_H_
