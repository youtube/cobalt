// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/simd/convert_yuv_to_rgb.h"
#include "media/base/simd/yuv_to_rgb_table.h"

#define packuswb(x) ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))
#define paddsw(x, y) (((x) + (y)) < -32768 ? -32768 : \
    (((x) + (y)) > 32767 ? 32767 : ((x) + (y))))

static inline void YUVPixel(uint8 y,
                            uint8 u,
                            uint8 v,
                            uint8* rgb_buf) {

  int b = kCoefficientsRgbY[256+u][0];
  int g = kCoefficientsRgbY[256+u][1];
  int r = kCoefficientsRgbY[256+u][2];
  int a = kCoefficientsRgbY[256+u][3];

  b = paddsw(b, kCoefficientsRgbY[512+v][0]);
  g = paddsw(g, kCoefficientsRgbY[512+v][1]);
  r = paddsw(r, kCoefficientsRgbY[512+v][2]);
  a = paddsw(a, kCoefficientsRgbY[512+v][3]);

  b = paddsw(b, kCoefficientsRgbY[y][0]);
  g = paddsw(g, kCoefficientsRgbY[y][1]);
  r = paddsw(r, kCoefficientsRgbY[y][2]);
  a = paddsw(a, kCoefficientsRgbY[y][3]);

  b >>= 6;
  g >>= 6;
  r >>= 6;
  a >>= 6;

  *reinterpret_cast<uint32*>(rgb_buf) = (packuswb(b)) |
                                        (packuswb(g) << 8) |
                                        (packuswb(r) << 16) |
                                        (packuswb(a) << 24);
}

extern "C" {

void ConvertYUVToRGB32Row_C(const uint8* y_buf,
                            const uint8* u_buf,
                            const uint8* v_buf,
                            uint8* rgb_buf,
                            int width) {
  for (int x = 0; x < width; x += 2) {
    uint8 u = u_buf[x >> 1];
    uint8 v = v_buf[x >> 1];
    uint8 y0 = y_buf[x];
    YUVPixel(y0, u, v, rgb_buf);
    if ((x + 1) < width) {
      uint8 y1 = y_buf[x + 1];
      YUVPixel(y1, u, v, rgb_buf + 4);
    }
    rgb_buf += 8;  // Advance 2 pixels.
  }
}

// 16.16 fixed point is used.  A shift by 16 isolates the integer.
// A shift by 17 is used to further subsample the chrominence channels.
// & 0xffff isolates the fixed point fraction.  >> 2 to get the upper 2 bits,
// for 1/65536 pixel accurate interpolation.
void ScaleYUVToRGB32Row_C(const uint8* y_buf,
                          const uint8* u_buf,
                          const uint8* v_buf,
                          uint8* rgb_buf,
                          int width,
                          int source_dx) {
  int x = 0;
  for (int i = 0; i < width; i += 2) {
    int y = y_buf[x >> 16];
    int u = u_buf[(x >> 17)];
    int v = v_buf[(x >> 17)];
    YUVPixel(y, u, v, rgb_buf);
    x += source_dx;
    if ((i + 1) < width) {
      y = y_buf[x >> 16];
      YUVPixel(y, u, v, rgb_buf+4);
      x += source_dx;
    }
    rgb_buf += 8;
  }
}

void LinearScaleYUVToRGB32Row_C(const uint8* y_buf,
                                const uint8* u_buf,
                                const uint8* v_buf,
                                uint8* rgb_buf,
                                int width,
                                int source_dx) {
  int x = 0;
  if (source_dx >= 0x20000) {
    x = 32768;
  }
  for (int i = 0; i < width; i += 2) {
    int y0 = y_buf[x >> 16];
    int y1 = y_buf[(x >> 16) + 1];
    int u0 = u_buf[(x >> 17)];
    int u1 = u_buf[(x >> 17) + 1];
    int v0 = v_buf[(x >> 17)];
    int v1 = v_buf[(x >> 17) + 1];
    int y_frac = (x & 65535);
    int uv_frac = ((x >> 1) & 65535);
    int y = (y_frac * y1 + (y_frac ^ 65535) * y0) >> 16;
    int u = (uv_frac * u1 + (uv_frac ^ 65535) * u0) >> 16;
    int v = (uv_frac * v1 + (uv_frac ^ 65535) * v0) >> 16;
    YUVPixel(y, u, v, rgb_buf);
    x += source_dx;
    if ((i + 1) < width) {
      y0 = y_buf[x >> 16];
      y1 = y_buf[(x >> 16) + 1];
      y_frac = (x & 65535);
      y = (y_frac * y1 + (y_frac ^ 65535) * y0) >> 16;
      YUVPixel(y, u, v, rgb_buf+4);
      x += source_dx;
    }
    rgb_buf += 8;
  }
}

}

namespace media {

void ConvertYUVToRGB32_C(const uint8* yplane,
                         const uint8* uplane,
                         const uint8* vplane,
                         uint8* rgbframe,
                         int width,
                         int height,
                         int ystride,
                         int uvstride,
                         int rgbstride,
                         YUVType yuv_type) {
  unsigned int y_shift = yuv_type;
  for (int y = 0; y < height; ++y) {
    uint8* rgb_row = rgbframe + y * rgbstride;
    const uint8* y_ptr = yplane + y * ystride;
    const uint8* u_ptr = uplane + (y >> y_shift) * uvstride;
    const uint8* v_ptr = vplane + (y >> y_shift) * uvstride;

    ConvertYUVToRGB32Row_C(y_ptr,
                           u_ptr,
                           v_ptr,
                           rgb_row,
                           width);
  }
}

}  // namespace media
