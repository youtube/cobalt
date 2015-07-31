// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SIMD_FILTER_YUV_H_
#define MEDIA_BASE_SIMD_FILTER_YUV_H_

#include "base/basictypes.h"

namespace media {

typedef void (*FilterYUVRowsProc)(uint8*,
                                  const uint8*,
                                  const uint8*,
                                  int,
                                  int);

void FilterYUVRows_C(uint8* ybuf, const uint8* y0_ptr, const uint8* y1_ptr,
                     int source_width, int source_y_fraction);

void FilterYUVRows_MMX(uint8* ybuf, const uint8* y0_ptr, const uint8* y1_ptr,
                       int source_width, int source_y_fraction);

void FilterYUVRows_SSE2(uint8* ybuf, const uint8* y0_ptr, const uint8* y1_ptr,
                        int source_width, int source_y_fraction);

}  // namespace media

#endif  // MEDIA_BASE_SIMD_FILTER_YUV_H_
