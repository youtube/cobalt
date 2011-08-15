// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "media/base/djb2.h"
#include "media/base/yuv_convert.h"
#include "media/base/yuv_row.h"
#include "testing/gtest/include/gtest/gtest.h"

// Size of raw image.
static const int kSourceWidth = 640;
static const int kSourceHeight = 360;
static const int kSourceYSize = kSourceWidth * kSourceHeight;
static const int kScaledWidth = 1024;
static const int kScaledHeight = 768;
static const int kBpp = 4;

// Surface sizes.
static const size_t kYUV12Size = kSourceYSize * 12 / 8;
static const size_t kYUV16Size = kSourceYSize * 16 / 8;
static const size_t kYUY2Size =  kSourceYSize * 16 / 8;
static const size_t kRGBSize = kSourceYSize * kBpp;
static const size_t kRGB24Size = kSourceYSize * 3;
static const size_t kRGBSizeConverted = kSourceYSize * kBpp;

TEST(YUVConvertTest, YV12) {
  // Allocate all surfaces.
  scoped_array<uint8> yuv_bytes(new uint8[kYUV12Size]);
  scoped_array<uint8> rgb_bytes(new uint8[kRGBSize]);
  scoped_array<uint8> rgb_converted_bytes(new uint8[kRGBSizeConverted]);

  // Read YUV reference data from file.
  FilePath yuv_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &yuv_url));
  yuv_url = yuv_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("bali_640x360_P420.yuv"));
  EXPECT_EQ(static_cast<int>(kYUV12Size),
            file_util::ReadFile(yuv_url,
                                reinterpret_cast<char*>(yuv_bytes.get()),
                                static_cast<int>(kYUV12Size)));

  // Convert a frame of YUV to 32 bit ARGB.
  media::ConvertYUVToRGB32(yuv_bytes.get(),
                           yuv_bytes.get() + kSourceYSize,
                           yuv_bytes.get() + kSourceYSize * 5 / 4,
                           rgb_converted_bytes.get(),            // RGB output
                           kSourceWidth, kSourceHeight,          // Dimensions
                           kSourceWidth,                         // YStride
                           kSourceWidth / 2,                     // UVStride
                           kSourceWidth * kBpp,                  // RGBStride
                           media::YV12);

  uint32 rgb_hash = DJB2Hash(rgb_converted_bytes.get(), kRGBSizeConverted,
                             kDJB2HashSeed);
  EXPECT_EQ(2413171226u, rgb_hash);
}

TEST(YUVConvertTest, YV16) {
  // Allocate all surfaces.
  scoped_array<uint8> yuv_bytes(new uint8[kYUV16Size]);
  scoped_array<uint8> rgb_bytes(new uint8[kRGBSize]);
  scoped_array<uint8> rgb_converted_bytes(new uint8[kRGBSizeConverted]);

  // Read YV16 reference data from file.
  FilePath yuv_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &yuv_url));
  yuv_url = yuv_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("bali_640x360_P422.yuv"));
  EXPECT_EQ(static_cast<int>(kYUV16Size),
            file_util::ReadFile(yuv_url,
                                reinterpret_cast<char*>(yuv_bytes.get()),
                                static_cast<int>(kYUV16Size)));

  // Convert a frame of YUV to 32 bit ARGB.
  media::ConvertYUVToRGB32(yuv_bytes.get(),                        // Y
                           yuv_bytes.get() + kSourceYSize,         // U
                           yuv_bytes.get() + kSourceYSize * 3 / 2, // V
                           rgb_converted_bytes.get(),              // RGB output
                           kSourceWidth, kSourceHeight,            // Dimensions
                           kSourceWidth,                           // YStride
                           kSourceWidth / 2,                       // UVStride
                           kSourceWidth * kBpp,                    // RGBStride
                           media::YV16);

  uint32 rgb_hash = DJB2Hash(rgb_converted_bytes.get(), kRGBSizeConverted,
                             kDJB2HashSeed);
  EXPECT_EQ(4222342047u, rgb_hash);
}

TEST(YUVScaleTest, YV12) {
  // Read YUV reference data from file.
  FilePath yuv_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &yuv_url));
  yuv_url = yuv_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("bali_640x360_P420.yuv"));
  const size_t size_of_yuv = kSourceYSize * 12 / 8;  // 12 bpp.
  scoped_array<uint8> yuv_bytes(new uint8[size_of_yuv]);
  EXPECT_EQ(static_cast<int>(size_of_yuv),
            file_util::ReadFile(yuv_url,
                                reinterpret_cast<char*>(yuv_bytes.get()),
                                static_cast<int>(size_of_yuv)));

  // Scale a frame of YUV to 32 bit ARGB.
  const size_t size_of_rgb_scaled = kScaledWidth * kScaledHeight * kBpp;
  scoped_array<uint8> rgb_source_bytes(new uint8[size_of_rgb_scaled]);

  media::ScaleYUVToRGB32(yuv_bytes.get(),                          // Y
                         yuv_bytes.get() + kSourceYSize,           // U
                         yuv_bytes.get() + kSourceYSize * 5 / 4,   // V
                         rgb_source_bytes.get(),                   // Rgb output
                         kSourceWidth, kSourceHeight,              // Dimensions
                         kScaledWidth, kScaledHeight,              // Dimensions
                         kSourceWidth,                             // YStride
                         kSourceWidth / 2,                         // UvStride
                         kScaledWidth * kBpp,                      // RgbStride
                         media::YV12,
                         media::ROTATE_0,
                         media::FILTER_NONE);

  uint32 rgb_hash = DJB2Hash(rgb_source_bytes.get(), size_of_rgb_scaled,
                             kDJB2HashSeed);
  EXPECT_EQ(4259656254u, rgb_hash);
}

TEST(YUVFilterScaleTest, YV12) {
  // Read YUV reference data from file.
  FilePath yuv_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &yuv_url));
  yuv_url = yuv_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("bali_640x360_P420.yuv"));
  const size_t size_of_yuv = kSourceYSize * 12 / 8;  // 12 bpp.
  scoped_array<uint8> yuv_bytes(new uint8[size_of_yuv]);
  EXPECT_EQ(static_cast<int>(size_of_yuv),
            file_util::ReadFile(yuv_url,
                                reinterpret_cast<char*>(yuv_bytes.get()),
                                static_cast<int>(size_of_yuv)));

  // Scale a frame of YUV to 32 bit ARGB.
  const size_t size_of_rgb_scaled = kScaledWidth * kScaledHeight * kBpp;
  scoped_array<uint8> rgb_source_bytes(new uint8[size_of_rgb_scaled]);

  media::ScaleYUVToRGB32(yuv_bytes.get(),                        // Y
                         yuv_bytes.get() + kSourceYSize,         // U
                         yuv_bytes.get() + kSourceYSize * 5 / 4, // V
                         rgb_source_bytes.get(),                 // Rgb output
                         kSourceWidth, kSourceHeight,            // Dimensions
                         kScaledWidth, kScaledHeight,            // Dimensions
                         kSourceWidth,                           // YStride
                         kSourceWidth / 2,                       // UvStride
                         kScaledWidth * kBpp,                    // RgbStride
                         media::YV12,
                         media::ROTATE_0,
                         media::FILTER_BILINEAR);

  uint32 rgb_hash = DJB2Hash(rgb_source_bytes.get(), size_of_rgb_scaled,
                             kDJB2HashSeed);
  EXPECT_EQ(2086305576u, rgb_hash);
}

TEST(YUVScaleTest, YV16) {
  // Read YV16 reference data from file.
  FilePath yuv_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &yuv_url));
  yuv_url = yuv_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("bali_640x360_P422.yuv"));
  const size_t size_of_yuv = kSourceYSize * 16 / 8;  // 16 bpp.
  scoped_array<uint8> yuv_bytes(new uint8[size_of_yuv]);
  EXPECT_EQ(static_cast<int>(size_of_yuv),
            file_util::ReadFile(yuv_url,
                                reinterpret_cast<char*>(yuv_bytes.get()),
                                static_cast<int>(size_of_yuv)));

  // Scale a frame of YUV to 32 bit ARGB.
  const size_t size_of_rgb_scaled = kScaledWidth * kScaledHeight * kBpp;
  scoped_array<uint8> rgb_source_bytes(new uint8[size_of_rgb_scaled]);

  media::ScaleYUVToRGB32(yuv_bytes.get(),                        // Y
                         yuv_bytes.get() + kSourceYSize,         // U
                         yuv_bytes.get() + kSourceYSize * 3 / 2, // V
                         rgb_source_bytes.get(),                 // Rgb output
                         kSourceWidth, kSourceHeight,            // Dimensions
                         kScaledWidth, kScaledHeight,            // Dimensions
                         kSourceWidth,                           // YStride
                         kSourceWidth / 2,                       // UvStride
                         kScaledWidth * kBpp,                    // RgbStride
                         media::YV16,
                         media::ROTATE_0,
                         media::FILTER_NONE);

  uint32 rgb_hash = DJB2Hash(rgb_source_bytes.get(), size_of_rgb_scaled,
                             kDJB2HashSeed);
  EXPECT_EQ(974965419u, rgb_hash);
}

TEST(YUVFilterScaleTest, YV16) {
  // Read YV16 reference data from file.
  FilePath yuv_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &yuv_url));
  yuv_url = yuv_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("bali_640x360_P422.yuv"));
  const size_t size_of_yuv = kSourceYSize * 16 / 8;  // 16 bpp.
  scoped_array<uint8> yuv_bytes(new uint8[size_of_yuv]);
  EXPECT_EQ(static_cast<int>(size_of_yuv),
            file_util::ReadFile(yuv_url,
                                reinterpret_cast<char*>(yuv_bytes.get()),
                                static_cast<int>(size_of_yuv)));

  // Scale a frame of YUV to 32 bit ARGB.
  const size_t size_of_rgb_scaled = kScaledWidth * kScaledHeight * kBpp;
  scoped_array<uint8> rgb_source_bytes(new uint8[size_of_rgb_scaled]);

  media::ScaleYUVToRGB32(yuv_bytes.get(),                        // Y
                         yuv_bytes.get() + kSourceYSize,         // U
                         yuv_bytes.get() + kSourceYSize * 3 / 2, // V
                         rgb_source_bytes.get(),                 // Rgb output
                         kSourceWidth, kSourceHeight,            // Dimensions
                         kScaledWidth, kScaledHeight,            // Dimensions
                         kSourceWidth,                           // YStride
                         kSourceWidth / 2,                       // UvStride
                         kScaledWidth * kBpp,                    // RgbStride
                         media::YV16,
                         media::ROTATE_0,
                         media::FILTER_BILINEAR);

  uint32 rgb_hash = DJB2Hash(rgb_source_bytes.get(), size_of_rgb_scaled,
                             kDJB2HashSeed);
  EXPECT_EQ(3857179240u, rgb_hash);
}

// This tests a known worst case YUV value, and for overflow.
TEST(YUVConvertTest, Clamp) {
  // Allocate all surfaces.
  scoped_array<uint8> yuv_bytes(new uint8[1]);
  scoped_array<uint8> rgb_bytes(new uint8[1]);
  scoped_array<uint8> rgb_converted_bytes(new uint8[1]);

  // Values that failed previously in bug report.
  unsigned char y = 255u;
  unsigned char u = 255u;
  unsigned char v = 19u;

  // Prefill extra large destination buffer to test for overflow.
  unsigned char rgb[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char expected[8] = { 255, 255, 104, 255, 4, 5, 6, 7 };
  // Convert a frame of YUV to 32 bit ARGB.
  media::ConvertYUVToRGB32(&y,       // Y
                           &u,       // U
                           &v,       // V
                           &rgb[0],  // RGB output
                           1, 1,     // Dimensions
                           0,        // YStride
                           0,        // UVStride
                           0,        // RGBStride
                           media::YV12);

  int expected_test = memcmp(rgb, expected, sizeof(expected));
  EXPECT_EQ(0, expected_test);
}

TEST(YUVConvertTest, RGB24ToYUV) {
  // Allocate all surfaces.
  scoped_array<uint8> rgb_bytes(new uint8[kRGB24Size]);
  scoped_array<uint8> yuv_converted_bytes(new uint8[kYUV12Size]);

  // Read RGB24 reference data from file.
  FilePath rgb_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &rgb_url));
  rgb_url = rgb_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("bali_640x360_RGB24.rgb"));
  EXPECT_EQ(static_cast<int>(kRGB24Size),
            file_util::ReadFile(rgb_url,
                                reinterpret_cast<char*>(rgb_bytes.get()),
                                static_cast<int>(kRGB24Size)));


  // Convert to I420.
  media::ConvertRGB24ToYUV(rgb_bytes.get(),
                           yuv_converted_bytes.get(),
                           yuv_converted_bytes.get() + kSourceYSize,
                           yuv_converted_bytes.get() + kSourceYSize * 5 / 4,
                           kSourceWidth, kSourceHeight,        // Dimensions
                           kSourceWidth * 3,                   // RGBStride
                           kSourceWidth,                       // YStride
                           kSourceWidth / 2);                  // UVStride

  uint32 rgb_hash = DJB2Hash(yuv_converted_bytes.get(), kYUV12Size,
                             kDJB2HashSeed);
  EXPECT_EQ(320824432u, rgb_hash);
}

TEST(YUVConvertTest, YUY2ToYUV) {
  // Allocate all surfaces.
  scoped_array<uint8> yuy_bytes(new uint8[kYUY2Size]);
  scoped_array<uint8> yuv_converted_bytes(new uint8[kYUV12Size]);

  // Read YUY reference data from file.
  FilePath yuy_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &yuy_url));
  yuy_url = yuy_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("bali_640x360_YUY2.yuv"));
  EXPECT_EQ(static_cast<int>(kYUY2Size),
            file_util::ReadFile(yuy_url,
                                reinterpret_cast<char*>(yuy_bytes.get()),
                                static_cast<int>(kYUY2Size)));

  // Convert to I420.
  media::ConvertYUY2ToYUV(yuy_bytes.get(),
                          yuv_converted_bytes.get(),
                          yuv_converted_bytes.get() + kSourceYSize,
                          yuv_converted_bytes.get() + kSourceYSize * 5 / 4,
                          kSourceWidth, kSourceHeight);

  uint32 yuy_hash = DJB2Hash(yuv_converted_bytes.get(), kYUV12Size,
                             kDJB2HashSeed);
  EXPECT_EQ(666823187u, yuy_hash);
}
