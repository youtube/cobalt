// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This webpage shows layout of YV12 and other YUV formats
// http://www.fourcc.org/yuv.php
// The actual conversion is best described here
// http://en.wikipedia.org/wiki/YUV
// An article on optimizing YUV conversion using tables instead of multiplies
// http://lestourtereaux.free.fr/papers/data/yuvrgb.pdf
//
// YV12 is a full plane of Y and a half height, half width chroma planes
// YV16 is a full plane of Y and a full height, half width chroma planes
//
// ARGB pixel format is output, which on little endian is stored as BGRA.
// The alpha is set to 255, allowing the application to use RGBA or RGB32.

#include "media/base/yuv_convert.h"

// Header for low level row functions.
#include "media/base/yuv_row.h"

#if USE_MMX
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <mmintrin.h>
#endif
#endif

#if USE_SSE2
#include <emmintrin.h>
#endif

namespace media {

// 16.16 fixed point arithmetic
const int kFractionBits = 16;
const int kFractionMax = 1 << kFractionBits;
const int kFractionMask = ((1 << kFractionBits) - 1);

// Convert a frame of YUV to 32 bit ARGB.
void ConvertYUVToRGB32(const uint8* y_buf,
                       const uint8* u_buf,
                       const uint8* v_buf,
                       uint8* rgb_buf,
                       int source_width,
                       int source_height,
                       int y_pitch,
                       int uv_pitch,
                       int rgb_pitch,
                       YUVType yuv_type) {
  unsigned int y_shift = yuv_type;
  for (int y = 0; y < source_height; ++y) {
    uint8* rgb_row = rgb_buf + y * rgb_pitch;
    const uint8* y_ptr = y_buf + y * y_pitch;
    const uint8* u_ptr = u_buf + (y >> y_shift) * uv_pitch;
    const uint8* v_ptr = v_buf + (y >> y_shift) * uv_pitch;

    FastConvertYUVToRGB32Row(y_ptr,
                             u_ptr,
                             v_ptr,
                             rgb_row,
                             source_width);
  }

  // MMX used for FastConvertYUVToRGB32Row requires emms instruction.
  EMMS();
}

#if USE_SSE2
// FilterRows combines two rows of the image using linear interpolation.
// SSE2 version blends 8 pixels at a time.
static void FilterRows(uint8* ybuf, const uint8* y0_ptr, const uint8* y1_ptr,
                       int source_width, int source_y_fraction) {
  __m128i zero = _mm_setzero_si128();
  __m128i y1_fraction = _mm_set1_epi16(
      static_cast<uint16>(source_y_fraction >> 8));
  __m128i y0_fraction = _mm_set1_epi16(
      static_cast<uint16>(256 - (source_y_fraction >> 8)));

  uint8* end = ybuf + source_width;
  if (ybuf < end) {
    do {
      __m128i y0 = _mm_loadl_epi64(reinterpret_cast<__m128i const*>(y0_ptr));
      __m128i y1 = _mm_loadl_epi64(reinterpret_cast<__m128i const*>(y1_ptr));
      y0 = _mm_unpacklo_epi8(y0, zero);
      y1 = _mm_unpacklo_epi8(y1, zero);
      y0 = _mm_mullo_epi16(y0, y0_fraction);
      y1 = _mm_mullo_epi16(y1, y1_fraction);
      y0 = _mm_add_epi16(y0, y1);  // 8.8 fixed point result
      y0 = _mm_srli_epi16(y0, 8);
      y0 = _mm_packus_epi16(y0, y0);
      _mm_storel_epi64(reinterpret_cast<__m128i *>(ybuf), y0);
      y0_ptr += 8;
      y1_ptr += 8;
      ybuf += 8;
    } while (ybuf < end);
  }
}

#elif USE_MMX
// MMX version blends 4 pixels at a time.
static void FilterRows(uint8* ybuf, const uint8* y0_ptr, const uint8* y1_ptr,
                       int source_width, int source_y_fraction) {
  __m64 zero = _mm_setzero_si64();
  __m64 y1_fraction = _mm_set1_pi16(
      static_cast<int16>(source_y_fraction >> 8));
  __m64 y0_fraction = _mm_set1_pi16(
      static_cast<int16>(256 - (source_y_fraction >> 8)));

  uint8* end = ybuf + source_width;
  if (ybuf < end) {
    do {
      __m64 y0 = _mm_cvtsi32_si64(*reinterpret_cast<const int *>(y0_ptr));
      __m64 y1 = _mm_cvtsi32_si64(*reinterpret_cast<const int *>(y1_ptr));
      y0 = _mm_unpacklo_pi8(y0, zero);
      y1 = _mm_unpacklo_pi8(y1, zero);
      y0 = _mm_mullo_pi16(y0, y0_fraction);
      y1 = _mm_mullo_pi16(y1, y1_fraction);
      y0 = _mm_add_pi16(y0, y1);  // 8.8 fixed point result
      y0 = _mm_srli_pi16(y0, 8);
      y0 = _mm_packs_pu16(y0, y0);
      *reinterpret_cast<int *>(ybuf) = _mm_cvtsi64_si32(y0);
      y0_ptr += 4;
      y1_ptr += 4;
      ybuf += 4;
    } while (ybuf < end);
  }
}
#else  // no MMX or SSE2
// C version blends 4 pixels at a time.
static void FilterRows(uint8* ybuf, const uint8* y0_ptr, const uint8* y1_ptr,
                       int source_width, int source_y_fraction) {
  int y1_fraction = source_y_fraction >> 8;
  int y0_fraction = 256 - (source_y_fraction >> 8);

  int y0_fraction = kFractionMax - source_y_fraction;
  int y1_fraction = source_y_fraction;
  uint8* end = ybuf + source_width;
  if (ybuf < end) {
    do {
      ybuf[0] = (y0_ptr[0] * (y0_fraction) + y1_ptr[0] * (y1_fraction)) >> 8;
      ybuf[1] = (y0_ptr[1] * (y0_fraction) + y1_ptr[1] * (y1_fraction)) >> 8;
      ybuf[2] = (y0_ptr[2] * (y0_fraction) + y1_ptr[2] * (y1_fraction)) >> 8;
      ybuf[3] = (y0_ptr[3] * (y0_fraction) + y1_ptr[3] * (y1_fraction)) >> 8;
      y0_ptr += 4;
      y1_ptr += 4;
      ybuf += 4;
    } while (ybuf < end);
  }
}
#endif

// Scale a frame of YUV to 32 bit ARGB.
void ScaleYUVToRGB32(const uint8* y_buf,
                     const uint8* u_buf,
                     const uint8* v_buf,
                     uint8* rgb_buf,
                     int source_width,
                     int source_height,
                     int width,
                     int height,
                     int y_pitch,
                     int uv_pitch,
                     int rgb_pitch,
                     YUVType yuv_type,
                     Rotate view_rotate,
                     ScaleFilter filter) {
  const int kFilterBufferSize = 8192;
  // Disable filtering if the screen is too big (to avoid buffer overflows).
  // This should never happen to regular users: they don't have monitors
  // wider than 8192 pixels.
  // TODO(fbarchard): Allow rotated videos to filter.
  if (source_width > kFilterBufferSize || view_rotate)
    filter = FILTER_NONE;

  unsigned int y_shift = yuv_type;
  // Diagram showing origin and direction of source sampling.
  // ->0   4<-
  // 7       3
  //
  // 6       5
  // ->1   2<-
  // Rotations that start at right side of image.
  if ((view_rotate == ROTATE_180) ||
      (view_rotate == ROTATE_270) ||
      (view_rotate == MIRROR_ROTATE_0) ||
      (view_rotate == MIRROR_ROTATE_90)) {
    y_buf += source_width - 1;
    u_buf += source_width / 2 - 1;
    v_buf += source_width / 2 - 1;
    source_width = -source_width;
  }
  // Rotations that start at bottom of image.
  if ((view_rotate == ROTATE_90) ||
      (view_rotate == ROTATE_180) ||
      (view_rotate == MIRROR_ROTATE_90) ||
      (view_rotate == MIRROR_ROTATE_180)) {
    y_buf += (source_height - 1) * y_pitch;
    u_buf += ((source_height >> y_shift) - 1) * uv_pitch;
    v_buf += ((source_height >> y_shift) - 1) * uv_pitch;
    source_height = -source_height;
  }

  // Handle zero sized destination.
  if (width == 0 || height == 0)
    return;
  int source_dx = source_width * kFractionMax / width;
  int source_dy = source_height * kFractionMax / height;
  int source_dx_uv = source_dx;

  if ((view_rotate == ROTATE_90) ||
      (view_rotate == ROTATE_270)) {
    int tmp = height;
    height = width;
    width = tmp;
    tmp = source_height;
    source_height = source_width;
    source_width = tmp;
    int original_dx = source_dx;
    int original_dy = source_dy;
    source_dx = ((original_dy >> kFractionBits) * y_pitch) << kFractionBits;
    source_dx_uv = ((original_dy >> kFractionBits) * uv_pitch) << kFractionBits;
    source_dy = original_dx;
    if (view_rotate == ROTATE_90) {
      y_pitch = -1;
      uv_pitch = -1;
      source_height = -source_height;
    } else {
      y_pitch = 1;
      uv_pitch = 1;
    }
  }

  // Need padding because FilterRows() may write up to 15 extra pixels
  // after the end for SSE2 version.
  uint8 ybuf[kFilterBufferSize + 16];
  uint8 ubuf[kFilterBufferSize / 2 + 16];
  uint8 vbuf[kFilterBufferSize / 2 + 16];
  // TODO(fbarchard): Fixed point math is off by 1 on negatives.
  int yscale_fixed = (source_height << kFractionBits) / height;
  for (int y = 0; y < height; ++y) {
    uint8* dest_pixel = rgb_buf + y * rgb_pitch;
    int source_y_subpixel = (y * yscale_fixed);
    int source_y = source_y_subpixel >> kFractionBits;

    const uint8* y0_ptr = y_buf + source_y * y_pitch;
    const uint8* y1_ptr = y0_ptr + y_pitch;

    const uint8* u0_ptr = u_buf + (source_y >> y_shift) * uv_pitch;
    const uint8* u1_ptr = u0_ptr + uv_pitch;
    const uint8* v0_ptr = v_buf + (source_y >> y_shift) * uv_pitch;
    const uint8* v1_ptr = v0_ptr + uv_pitch;

    int source_y_fraction = source_y_subpixel & kFractionMask;
    int source_uv_fraction = (source_y_subpixel >> y_shift) & kFractionMask;

    const uint8* y_ptr = y0_ptr;
    const uint8* u_ptr = u0_ptr;
    const uint8* v_ptr = v0_ptr;
    // Apply vertical filtering if necessary.
    // TODO(fbarchard): Remove memcpy when not necessary.
    if (filter == media::FILTER_BILINEAR) {
      if (yscale_fixed != kFractionMax &&
          source_y_fraction && ((source_y + 1) < source_height)) {
        FilterRows(ybuf, y0_ptr, y1_ptr, source_width, source_y_fraction);
      } else {
        memcpy(ybuf, y0_ptr, source_width);
      }
      y_ptr = ybuf;
      ybuf[source_width] = ybuf[source_width-1];
      int uv_source_width = (source_width + 1) / 2;
      if (yscale_fixed != kFractionMax &&
          source_uv_fraction &&
          (((source_y >> y_shift) + 1) < (source_height >> y_shift))) {
        FilterRows(ubuf, u0_ptr, u1_ptr, uv_source_width, source_uv_fraction);
        FilterRows(vbuf, v0_ptr, v1_ptr, uv_source_width, source_uv_fraction);
      } else {
        memcpy(ubuf, u0_ptr, uv_source_width);
        memcpy(vbuf, v0_ptr, uv_source_width);
      }
      u_ptr = ubuf;
      v_ptr = vbuf;
      ubuf[uv_source_width] = ubuf[uv_source_width - 1];
      vbuf[uv_source_width] = vbuf[uv_source_width - 1];
    }
    if (source_dx == kFractionMax) {  // Not scaled
      FastConvertYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                               dest_pixel, width);
    } else {
      if (filter == FILTER_BILINEAR)
        LinearScaleYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                                 dest_pixel, width, source_dx);
      else {
// Specialized scalers and rotation.
#if USE_MMX && defined(_MSC_VER)
        if (width == (source_width * 2)) {
          DoubleYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                              dest_pixel, width);
        } else if ((source_dx & kFractionMask) == 0) {
          // Scaling by integer scale factor. ie half.
          ConvertYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                               dest_pixel, width,
                               source_dx >> kFractionBits);
        } else if (source_dx_uv == source_dx) { // Not rotated.
          ScaleYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                             dest_pixel, width, source_dx);
        } else {
          RotateConvertYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                                     dest_pixel, width,
                                     source_dx >> kFractionBits,
                                     source_dx_uv >> kFractionBits);
        }
#else
        ScaleYUVToRGB32Row(y_ptr, u_ptr, v_ptr,
                           dest_pixel, width, source_dx);
#endif
      }
    }
  }
  // MMX used for FastConvertYUVToRGB32Row and FilterRows requires emms.
  EMMS();
}

}  // namespace media
