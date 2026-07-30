// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/vt_video_encode_accelerator_mac.h"

#include "media/base/video_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(VTVideoEncodeAcceleratorTest, CalculatePsnr_PerfectMatch) {
  // MSE == 0 should return the max cap of 128.0 dB.
  EXPECT_DOUBLE_EQ(
      VTVideoEncodeAccelerator::CalculatePsnrForTesting(0.0, PIXEL_FORMAT_I420),
      128.0);
  EXPECT_DOUBLE_EQ(
      VTVideoEncodeAccelerator::CalculatePsnrForTesting(0.0, PIXEL_FORMAT_NV12),
      128.0);
}

TEST(VTVideoEncodeAcceleratorTest, CalculatePsnr_8Bit) {
  // Test PSNR calculations for 8-bit formats (max_value = 255).
  // PSNR = 10 * log10(255^2 / 1) = 48.1308 dB.
  EXPECT_NEAR(
      VTVideoEncodeAccelerator::CalculatePsnrForTesting(1.0, PIXEL_FORMAT_I420),
      48.13, 0.01);
  EXPECT_NEAR(
      VTVideoEncodeAccelerator::CalculatePsnrForTesting(1.0, PIXEL_FORMAT_NV12),
      48.13, 0.01);

  // PSNR = 10 * log10(255^2 / 100) = 28.1308 dB.
  EXPECT_NEAR(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                  100.0, PIXEL_FORMAT_I420),
              28.13, 0.01);
  EXPECT_NEAR(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                  100.0, PIXEL_FORMAT_NV12),
              28.13, 0.01);
}

TEST(VTVideoEncodeAcceleratorTest, CalculatePsnr_CapMaxPSNR) {
  // Extremely small MSE should be capped at 128.0 dB (lossless).
  EXPECT_DOUBLE_EQ(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                       1e-9, PIXEL_FORMAT_I420),
                   128.0);
  EXPECT_DOUBLE_EQ(VTVideoEncodeAccelerator::CalculatePsnrForTesting(
                       1e-9, PIXEL_FORMAT_NV12),
                   128.0);
}

}  // namespace media
