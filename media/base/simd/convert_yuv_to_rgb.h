// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SIMD_CONVERT_YUV_TO_RGB_H_
#define MEDIA_BASE_SIMD_CONVERT_YUV_TO_RGB_H_

#include "base/basictypes.h"
#include "media/base/yuv_convert.h"

namespace media {

typedef void (*ConvertYUVToRGB32Proc)(const uint8*,
                                      const uint8*,
                                      const uint8*,
                                      uint8*,
                                      int,
                                      int,
                                      int,
                                      int,
                                      int,
                                      YUVType);

void ConvertYUVToRGB32_C(const uint8* yplane,
                         const uint8* uplane,
                         const uint8* vplane,
                         uint8* rgbframe,
                         int width,
                         int height,
                         int ystride,
                         int uvstride,
                         int rgbstride,
                         YUVType yuv_type);

void ConvertYUVToRGB32_SSE(const uint8* yplane,
                           const uint8* uplane,
                           const uint8* vplane,
                           uint8* rgbframe,
                           int width,
                           int height,
                           int ystride,
                           int uvstride,
                           int rgbstride,
                           YUVType yuv_type);

void ConvertYUVToRGB32_MMX(const uint8* yplane,
                           const uint8* uplane,
                           const uint8* vplane,
                           uint8* rgbframe,
                           int width,
                           int height,
                           int ystride,
                           int uvstride,
                           int rgbstride,
                           YUVType yuv_type);

}  // namespace media

// Assembly functions are declared without namespace.
extern "C" {

typedef void (*ConvertYUVToRGB32RowProc)(const uint8*,
                                          const uint8*,
                                          const uint8*,
                                          uint8*,
                                          int);
typedef void (*ScaleYUVToRGB32RowProc)(const uint8*,
                                       const uint8*,
                                       const uint8*,
                                       uint8*,
                                       int,
                                       int);

void ConvertYUVToRGB32Row_C(const uint8* yplane,
                            const uint8* uplane,
                            const uint8* vplane,
                            uint8* rgbframe,
                            int width);

void ConvertYUVToRGB32Row_MMX(const uint8* yplane,
                              const uint8* uplane,
                              const uint8* vplane,
                              uint8* rgbframe,
                              int width);

void ConvertYUVToRGB32Row_SSE(const uint8* yplane,
                              const uint8* uplane,
                              const uint8* vplane,
                              uint8* rgbframe,
                              int width);

void ScaleYUVToRGB32Row_C(const uint8* y_buf,
                          const uint8* u_buf,
                          const uint8* v_buf,
                          uint8* rgb_buf,
                          int width,
                          int source_dx);

void ScaleYUVToRGB32Row_MMX(const uint8* y_buf,
                            const uint8* u_buf,
                            const uint8* v_buf,
                            uint8* rgb_buf,
                            int width,
                            int source_dx);

void ScaleYUVToRGB32Row_SSE(const uint8* y_buf,
                            const uint8* u_buf,
                            const uint8* v_buf,
                            uint8* rgb_buf,
                            int width,
                            int source_dx);

void ScaleYUVToRGB32Row_SSE2_X64(const uint8* y_buf,
                                 const uint8* u_buf,
                                 const uint8* v_buf,
                                 uint8* rgb_buf,
                                 int width,
                                 int source_dx);

void LinearScaleYUVToRGB32Row_C(const uint8* y_buf,
                                const uint8* u_buf,
                                const uint8* v_buf,
                                uint8* rgb_buf,
                                int width,
                                int source_dx);

void LinearScaleYUVToRGB32Row_MMX(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int width,
                                  int source_dx);

void LinearScaleYUVToRGB32Row_SSE(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int width,
                                  int source_dx);

void LinearScaleYUVToRGB32Row_MMX_X64(const uint8* y_buf,
                                      const uint8* u_buf,
                                      const uint8* v_buf,
                                      uint8* rgb_buf,
                                      int width,
                                      int source_dx);

}  // extern "C"

#endif  // MEDIA_BASE_SIMD_CONVERT_YUV_TO_RGB_H_
