// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SIMD_CONVERT_RGB_TO_YUV_SSSE3_H_
#define MEDIA_BASE_SIMD_CONVERT_RGB_TO_YUV_SSSE3_H_

#ifdef __cplusplus
extern "C" {
#endif

// The header file for ASM functions that convert a row of RGB pixels with SSSE3
// instructions so we can call them from C++ code. These functions are
// implemented in "convert_rgb_to_yuv_ssse3.asm".

// Convert a row of 24-bit RGB pixels to YV12 pixels.
void ConvertRGBToYUVRow_SSSE3(const uint8* rgb,
                              uint8* y,
                              uint8* u,
                              uint8* v,
                              int width);

// Convert a row of 32-bit RGB pixels to YV12 pixels.
void ConvertARGBToYUVRow_SSSE3(const uint8* argb,
                               uint8* y,
                               uint8* u,
                               uint8* v,
                               int width);

// Convert a row of 24-bit RGB pixels to YV12 pixels.
void ConvertRGBToYUVEven_SSSE3(const uint8* rgb,
                               uint8* y,
                               uint8* u,
                               uint8* v,
                               int width);
void ConvertRGBToYUVOdd_SSSE3(const uint8* rgb,
                              uint8* y,
                              uint8* u,
                              uint8* v,
                              int width);

// Convert a row of 32-bit RGB pixels to YV12 pixels.
void ConvertARGBToYUVEven_SSSE3(const uint8* argb,
                                uint8* y,
                                uint8* u,
                                uint8* v,
                                int width);
void ConvertARGBToYUVOdd_SSSE3(const uint8* argb,
                               uint8* y,
                               uint8* u,
                               uint8* v,
                               int width);
#ifdef __cplusplus
}
#endif

#endif  // MEDIA_BASE_SIMD_CONVERT_RGB_TO_YUV_SSSE3_H_
