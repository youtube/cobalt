// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "media/base/cpu_features.h"
#include "media/base/simd/convert_rgb_to_yuv.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Reference code that converts RGB pixels to YUV pixels.
int ConvertRGBToY(const uint8* rgb) {
  int y = 25 * rgb[0] + 129 * rgb[1] + 66 * rgb[2];
  y = ((y + 128) >> 8) + 16;
  return std::max(0, std::min(255, y));
}

int ConvertRGBToU(const uint8* rgb, int size, bool subsampling) {
  int u = 0;
  if (!subsampling) {
    u = 112 * rgb[0] - 74 * rgb[1] - 38 * rgb[2];
  } else {
    int u0 = 112 * rgb[0] - 74 * rgb[1] - 38 * rgb[2];
    int u1 = 112 * rgb[size] - 74 * rgb[size + 1] - 38 * rgb[size + 2];
    u = (u0 + u1 + 1) / 2;
  }
  u = ((u + 128) >> 8) + 128;
  return std::max(0, std::min(255, u));
}

int ConvertRGBToV(const uint8* rgb, int size, bool subsampling) {
  int v = 0;
  if (!subsampling) {
    v = -18 * rgb[0] - 94 * rgb[1] + 112 * rgb[2];
  } else {
    int v0 = -18 * rgb[0] - 94 * rgb[1] + 112 * rgb[2];
    int v1 = -18 * rgb[size] - 94 * rgb[size + 1] + 112 * rgb[size + 2];
    v = (v0 + v1 + 1) / 2;
  }
  v = ((v + 128) >> 8) + 128;
  return std::max(0, std::min(255, v));
}

}  // namespace

// A side-by-side test that verifies our ASM functions that convert RGB pixels
// to YUV pixels can output the expected results. This test converts RGB pixels
// to YUV pixels with our ASM functions (which use SSE, SSE2, SSE3, and SSSE3)
// and compare the output YUV pixels with the ones calculated with out reference
// functions implemented in C++.
TEST(YUVConvertTest, SideBySideRGB) {
  // We skip this test on PCs which does not support SSE3 because this test
  // needs it.
  if (!media::hasSSSE3())
    return;

  // This test checks a subset of all RGB values so this test does not take so
  // long time.
  const int kStep = 8;
  const int kWidth = 256 / kStep;

#ifdef ENABLE_SUBSAMPLING
  const bool kSubsampling = true;
#else
  const bool kSubsampling = false;
#endif

  for (int size = 3; size <= 4; ++size) {
    // Create the output buffers.
    scoped_array<uint8> rgb(new uint8[kWidth * size]);
    scoped_array<uint8> y(new uint8[kWidth]);
    scoped_array<uint8> u(new uint8[kWidth / 2]);
    scoped_array<uint8> v(new uint8[kWidth / 2]);

    // Choose the function that converts from RGB pixels to YUV ones.
    void (*convert)(const uint8*, uint8*, uint8*, uint8*,
                    int, int, int, int, int) = NULL;
    if (size == 3)
      convert = media::ConvertRGB24ToYUV_SSSE3;
    else
      convert = media::ConvertRGB32ToYUV_SSSE3;

    int total_error = 0;
    for (int r = 0; r < kWidth; ++r) {
      for (int g = 0; g < kWidth; ++g) {

        // Fill the input pixels.
        for (int b = 0; b < kWidth; ++b) {
          rgb[b * size + 0] = b * kStep;
          rgb[b * size + 1] = g * kStep;
          rgb[b * size + 2] = r * kStep;
          if (size == 4)
            rgb[b * size + 3] = 255;
        }

        // Convert the input RGB pixels to YUV ones.
        convert(rgb.get(), y.get(), u.get(), v.get(), kWidth, 1, kWidth * size,
                kWidth, kWidth / 2);

        // Check the output Y pixels.
        for (int i = 0; i < kWidth; ++i) {
          const uint8* p = &rgb[i * size];
          int error = ConvertRGBToY(p) - y[i];
          total_error += error > 0 ? error : -error;
        }

        // Check the output U pixels.
        for (int i = 0; i < kWidth / 2; ++i) {
          const uint8* p = &rgb[i * 2 * size];
          int error = ConvertRGBToU(p, size, kSubsampling) - u[i];
          total_error += error > 0 ? error : -error;
        }

        // Check the output V pixels.
        for (int i = 0; i < kWidth / 2; ++i) {
          const uint8* p = &rgb[i * 2 * size];
          int error = ConvertRGBToV(p, size, kSubsampling) - v[i];
          total_error += error > 0 ? error : -error;
        }
      }
    }

    EXPECT_EQ(0, total_error);
  }
}
