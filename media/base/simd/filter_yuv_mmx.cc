// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <mmintrin.h>
#include <emmintrin.h>
#endif

#include "build/build_config.h"
#include "media/base/simd/filter_yuv.h"

namespace media {

#if defined(COMPILER_MSVC)
// Warning 4799 is about calling emms before the function exits.
// We calls emms in a frame level so suppress this warning.
#pragma warning(disable: 4799)
#endif

void FilterYUVRows_MMX(uint8* ybuf, const uint8* y0_ptr, const uint8* y1_ptr,
                       int source_width, int source_y_fraction) {
  __m64 zero = _mm_setzero_si64();
  __m64 y1_fraction = _mm_set1_pi16(source_y_fraction);
  __m64 y0_fraction = _mm_set1_pi16(256 - source_y_fraction);

  const __m64* y0_ptr64 = reinterpret_cast<const __m64*>(y0_ptr);
  const __m64* y1_ptr64 = reinterpret_cast<const __m64*>(y1_ptr);
  __m64* dest64 = reinterpret_cast<__m64*>(ybuf);
  __m64* end64 = reinterpret_cast<__m64*>(ybuf + source_width);

  do {
    __m64 y0 = *y0_ptr64++;
    __m64 y1 = *y1_ptr64++;
    __m64 y2 = _mm_unpackhi_pi8(y0, zero);
    __m64 y3 = _mm_unpackhi_pi8(y1, zero);
    y0 = _mm_unpacklo_pi8(y0, zero);
    y1 = _mm_unpacklo_pi8(y1, zero);
    y0 = _mm_mullo_pi16(y0, y0_fraction);
    y1 = _mm_mullo_pi16(y1, y1_fraction);
    y2 = _mm_mullo_pi16(y2, y0_fraction);
    y3 = _mm_mullo_pi16(y3, y1_fraction);
    y0 = _mm_add_pi16(y0, y1);
    y2 = _mm_add_pi16(y2, y3);
    y0 = _mm_srli_pi16(y0, 8);
    y2 = _mm_srli_pi16(y2, 8);
    y0 = _mm_packs_pu16(y0, y2);
    *dest64++ = y0;
  } while (dest64 < end64);
}

#if defined(COMPILER_MSVC)
#pragma warning(default: 4799)
#endif

}  // namespace media
