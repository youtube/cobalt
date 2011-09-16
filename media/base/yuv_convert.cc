// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

#include "base/logging.h"
#include "build/build_config.h"
#include "media/base/cpu_features.h"
#include "media/base/simd/convert_rgb_to_yuv.h"
#include "media/base/simd/convert_yuv_to_rgb.h"
#include "media/base/simd/filter_yuv.h"

#if defined(ARCH_CPU_X86_FAMILY)
#if defined(COMPILER_MSVC)
#include <intrin.h>
#else
#include <mmintrin.h>
#endif
#endif

namespace media {

static FilterYUVRowsProc ChooseFilterYUVRowsProc() {
#if defined(ARCH_CPU_X86_FAMILY)
  if (hasSSE2())
    return &FilterYUVRows_SSE2;
  if (hasMMX())
    return &FilterYUVRows_MMX;
#endif
  return &FilterYUVRows_C;
}

static ConvertYUVToRGB32RowProc ChooseConvertYUVToRGB32RowProc() {
#if defined(ARCH_CPU_X86_FAMILY)
  if (hasSSE())
    return &ConvertYUVToRGB32Row_SSE;
  if (hasMMX())
    return &ConvertYUVToRGB32Row_MMX;
#endif
  return &ConvertYUVToRGB32Row_C;
}

static ScaleYUVToRGB32RowProc ChooseScaleYUVToRGB32RowProc() {
#if defined(ARCH_CPU_X86_FAMILY)
#if defined(ARCH_CPU_X86_64)
  // Use 64-bits version if possible.
  return &ScaleYUVToRGB32Row_SSE2_X64;
#endif
  // Choose the best one on 32-bits system.
  if (hasSSE())
    return &ScaleYUVToRGB32Row_SSE;
  if (hasMMX())
    return &ScaleYUVToRGB32Row_MMX;
#endif
  return &ScaleYUVToRGB32Row_C;
}

static ScaleYUVToRGB32RowProc ChooseLinearScaleYUVToRGB32RowProc() {
#if defined(ARCH_CPU_X86_FAMILY)
#if defined(ARCH_CPU_X86_64)
  // Use 64-bits version if possible.
  return &LinearScaleYUVToRGB32Row_MMX_X64;
#endif
  // 32-bits system.
  if (hasSSE())
    return &LinearScaleYUVToRGB32Row_SSE;
  if (hasMMX())
    return &LinearScaleYUVToRGB32Row_MMX;
#endif
  return &LinearScaleYUVToRGB32Row_C;
}

// Empty SIMD registers state after using them.
void EmptyRegisterState() {
#if defined(ARCH_CPU_X86_FAMILY)
  static bool checked = false;
  static bool has_mmx = false;
  if (!checked) {
    has_mmx = hasMMX();
    checked = true;
  }
  if (has_mmx)
    _mm_empty();
#endif
}

// 16.16 fixed point arithmetic
const int kFractionBits = 16;
const int kFractionMax = 1 << kFractionBits;
const int kFractionMask = ((1 << kFractionBits) - 1);

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
  static FilterYUVRowsProc filter_proc = NULL;
  static ConvertYUVToRGB32RowProc convert_proc = NULL;
  static ScaleYUVToRGB32RowProc scale_proc = NULL;
  static ScaleYUVToRGB32RowProc linear_scale_proc = NULL;

  if (!filter_proc)
    filter_proc = ChooseFilterYUVRowsProc();
  if (!convert_proc)
    convert_proc = ChooseConvertYUVToRGB32RowProc();
  if (!scale_proc)
    scale_proc = ChooseScaleYUVToRGB32RowProc();
  if (!linear_scale_proc)
    linear_scale_proc = ChooseLinearScaleYUVToRGB32RowProc();

  // Handle zero sized sources and destinations.
  if ((yuv_type == YV12 && (source_width < 2 || source_height < 2)) ||
      (yuv_type == YV16 && (source_width < 2 || source_height < 1)) ||
      width == 0 || height == 0)
    return;

  // 4096 allows 3 buffers to fit in 12k.
  // Helps performance on CPU with 16K L1 cache.
  // Large enough for 3830x2160 and 30" displays which are 2560x1600.
  const int kFilterBufferSize = 4096;
  // Disable filtering if the screen is too big (to avoid buffer overflows).
  // This should never happen to regular users: they don't have monitors
  // wider than 4096 pixels.
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

  int source_dx = source_width * kFractionMax / width;
  int source_dy = source_height * kFractionMax / height;

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

  // Need padding because FilterRows() will write 1 to 16 extra pixels
  // after the end for SSE2 version.
  uint8 yuvbuf[16 + kFilterBufferSize * 3 + 16];
  uint8* ybuf =
      reinterpret_cast<uint8*>(reinterpret_cast<uintptr_t>(yuvbuf + 15) & ~15);
  uint8* ubuf = ybuf + kFilterBufferSize;
  uint8* vbuf = ubuf + kFilterBufferSize;
  // TODO(fbarchard): Fixed point math is off by 1 on negatives.
  int yscale_fixed = (source_height << kFractionBits) / height;

  // TODO(fbarchard): Split this into separate function for better efficiency.
  for (int y = 0; y < height; ++y) {
    uint8* dest_pixel = rgb_buf + y * rgb_pitch;
    int source_y_subpixel = (y * yscale_fixed);
    if (yscale_fixed >= (kFractionMax * 2)) {
      source_y_subpixel += kFractionMax / 2;  // For 1/2 or less, center filter.
    }
    int source_y = source_y_subpixel >> kFractionBits;

    const uint8* y0_ptr = y_buf + source_y * y_pitch;
    const uint8* y1_ptr = y0_ptr + y_pitch;

    const uint8* u0_ptr = u_buf + (source_y >> y_shift) * uv_pitch;
    const uint8* u1_ptr = u0_ptr + uv_pitch;
    const uint8* v0_ptr = v_buf + (source_y >> y_shift) * uv_pitch;
    const uint8* v1_ptr = v0_ptr + uv_pitch;

    // vertical scaler uses 16.8 fixed point
    int source_y_fraction = (source_y_subpixel & kFractionMask) >> 8;
    int source_uv_fraction =
        ((source_y_subpixel >> y_shift) & kFractionMask) >> 8;

    const uint8* y_ptr = y0_ptr;
    const uint8* u_ptr = u0_ptr;
    const uint8* v_ptr = v0_ptr;
    // Apply vertical filtering if necessary.
    // TODO(fbarchard): Remove memcpy when not necessary.
    if (filter & media::FILTER_BILINEAR_V) {
      if (yscale_fixed != kFractionMax &&
          source_y_fraction && ((source_y + 1) < source_height)) {
        filter_proc(ybuf, y0_ptr, y1_ptr, source_width, source_y_fraction);
      } else {
        memcpy(ybuf, y0_ptr, source_width);
      }
      y_ptr = ybuf;
      ybuf[source_width] = ybuf[source_width-1];
      int uv_source_width = (source_width + 1) / 2;
      if (yscale_fixed != kFractionMax &&
          source_uv_fraction &&
          (((source_y >> y_shift) + 1) < (source_height >> y_shift))) {
        filter_proc(ubuf, u0_ptr, u1_ptr, uv_source_width, source_uv_fraction);
        filter_proc(vbuf, v0_ptr, v1_ptr, uv_source_width, source_uv_fraction);
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
      convert_proc(y_ptr, u_ptr, v_ptr, dest_pixel, width);
    } else {
      if (filter & FILTER_BILINEAR_H) {
        linear_scale_proc(y_ptr, u_ptr, v_ptr, dest_pixel, width, source_dx);
      } else {
        scale_proc(y_ptr, u_ptr, v_ptr, dest_pixel, width, source_dx);
      }
    }
  }

  EmptyRegisterState();
}

void ConvertRGB32ToYUV(const uint8* rgbframe,
                       uint8* yplane,
                       uint8* uplane,
                       uint8* vplane,
                       int width,
                       int height,
                       int rgbstride,
                       int ystride,
                       int uvstride) {
  static void (*convert_proc)(const uint8*, uint8*, uint8*, uint8*,
                              int, int, int, int, int) = NULL;
  if (!convert_proc) {
#if defined(ARCH_CPU_ARM_FAMILY)
    // For ARM processors, always use C version.
    // TODO(hclam): Implement a NEON version.
    convert_proc = &ConvertRGB32ToYUV_C;
#else
    if (hasSSE2())
      convert_proc = &ConvertRGB32ToYUV_SSE2;
    else
      convert_proc = &ConvertRGB32ToYUV_C;
#endif
  }

  convert_proc(rgbframe, yplane, uplane, vplane, width, height,
               rgbstride, ystride, uvstride);
}

void ConvertRGB24ToYUV(const uint8* rgbframe,
                       uint8* yplane,
                       uint8* uplane,
                       uint8* vplane,
                       int width,
                       int height,
                       int rgbstride,
                       int ystride,
                       int uvstride) {
  ConvertRGB24ToYUV_C(rgbframe, yplane, uplane, vplane, width, height,
                      rgbstride, ystride, uvstride);
}

void ConvertYUY2ToYUV(const uint8* src,
                      uint8* yplane,
                      uint8* uplane,
                      uint8* vplane,
                      int width,
                      int height) {
  for (int i = 0; i < height / 2; ++i) {
    for (int j = 0; j < (width / 2); ++j) {
      yplane[0] = src[0];
      *uplane = src[1];
      yplane[1] = src[2];
      *vplane = src[3];
      src += 4;
      yplane += 2;
      uplane++;
      vplane++;
    }
    for (int j = 0; j < (width / 2); ++j) {
      yplane[0] = src[0];
      yplane[1] = src[2];
      src += 4;
      yplane += 2;
    }
  }
}

void ConvertYUVToRGB32(const uint8* yplane,
                       const uint8* uplane,
                       const uint8* vplane,
                       uint8* rgbframe,
                       int width,
                       int height,
                       int ystride,
                       int uvstride,
                       int rgbstride,
                       YUVType yuv_type) {
#if defined(ARCH_CPU_ARM_FAMILY)
  ConvertYUVToRGB32_C(yplane, uplane, vplane, rgbframe,
                      width, height, ystride, uvstride, rgbstride, yuv_type);
#else
  static ConvertYUVToRGB32Proc convert_proc = NULL;
  if (!convert_proc) {
    if (hasSSE())
      convert_proc = &ConvertYUVToRGB32_SSE;
    else if (hasMMX())
      convert_proc = &ConvertYUVToRGB32_MMX;
    else
      convert_proc = &ConvertYUVToRGB32_C;
  }

  convert_proc(yplane, uplane, vplane, rgbframe,
               width, height, ystride, uvstride, rgbstride, yuv_type);
#endif
}

}  // namespace media
