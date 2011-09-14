// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "media/base/cpu_features.h"
#include "media/base/djb2.h"
#include "media/base/yuv_convert.h"
#include "media/base/yuv_convert_internal.h"
#include "media/base/yuv_row.h"
#include "testing/gtest/include/gtest/gtest.h"

// Size of raw image.
static const int kSourceWidth = 640;
static const int kSourceHeight = 360;
static const int kSourceYSize = kSourceWidth * kSourceHeight;
static const int kSourceUOffset = kSourceYSize;
static const int kSourceVOffset = kSourceYSize * 5 / 4;
static const int kScaledWidth = 1024;
static const int kScaledHeight = 768;
static const int kBpp = 4;

// Surface sizes for various test files.
static const int kYUV12Size = kSourceYSize * 12 / 8;
static const int kYUV16Size = kSourceYSize * 16 / 8;
static const int kYUY2Size =  kSourceYSize * 16 / 8;
static const int kRGBSize = kSourceYSize * kBpp;
static const int kRGBSizeScaled = kScaledWidth * kScaledHeight * kBpp;
static const int kRGB24Size = kSourceYSize * 3;
static const int kRGBSizeConverted = kSourceYSize * kBpp;

// Helper for reading test data into a scoped_array<uint8>.
static void ReadData(const FilePath::CharType* filename,
                     int expected_size,
                     scoped_array<uint8>* data) {
  data->reset(new uint8[expected_size]);

  FilePath path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &path));
  path = path.Append(FILE_PATH_LITERAL("media"))
             .Append(FILE_PATH_LITERAL("test"))
             .Append(FILE_PATH_LITERAL("data"))
             .Append(filename);

  // Verify file size is correct.
  int64 actual_size = 0;
  file_util::GetFileSize(path, &actual_size);
  CHECK_EQ(actual_size, expected_size);

  // Verify bytes read are correct.
  int bytes_read = file_util::ReadFile(
      path, reinterpret_cast<char*>(data->get()), expected_size);
  CHECK_EQ(bytes_read, expected_size);
}

static void ReadYV12Data(scoped_array<uint8>* data) {
  ReadData(FILE_PATH_LITERAL("bali_640x360_P420.yuv"), kYUV12Size, data);
}

static void ReadYV16Data(scoped_array<uint8>* data) {
  ReadData(FILE_PATH_LITERAL("bali_640x360_P422.yuv"), kYUV16Size, data);
}

static void ReadRGB24Data(scoped_array<uint8>* data) {
  ReadData(FILE_PATH_LITERAL("bali_640x360_RGB24.rgb"), kRGB24Size, data);
}

static void ReadYUY2Data(scoped_array<uint8>* data) {
  ReadData(FILE_PATH_LITERAL("bali_640x360_YUY2.yuv"), kYUY2Size, data);
}

TEST(YUVConvertTest, YV12) {
  // Allocate all surfaces.
  scoped_array<uint8> yuv_bytes;
  scoped_array<uint8> rgb_bytes(new uint8[kRGBSize]);
  scoped_array<uint8> rgb_converted_bytes(new uint8[kRGBSizeConverted]);

  // Read YUV reference data from file.
  ReadYV12Data(&yuv_bytes);

  // Convert a frame of YUV to 32 bit ARGB.
  media::ConvertYUVToRGB32(yuv_bytes.get(),
                           yuv_bytes.get() + kSourceUOffset,
                           yuv_bytes.get() + kSourceVOffset,
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
  scoped_array<uint8> yuv_bytes;
  scoped_array<uint8> rgb_bytes(new uint8[kRGBSize]);
  scoped_array<uint8> rgb_converted_bytes(new uint8[kRGBSizeConverted]);

  // Read YUV reference data from file.
  ReadYV16Data(&yuv_bytes);

  // Convert a frame of YUV to 32 bit ARGB.
  media::ConvertYUVToRGB32(yuv_bytes.get(),                        // Y
                           yuv_bytes.get() + kSourceUOffset,       // U
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

struct YUVScaleTestData {
  YUVScaleTestData(media::YUVType y, media::ScaleFilter s, uint32 r)
      : yuv_type(y),
        scale_filter(s),
        rgb_hash(r) {
  }

  media::YUVType yuv_type;
  media::ScaleFilter scale_filter;
  uint32 rgb_hash;
};

class YUVScaleTest : public ::testing::TestWithParam<YUVScaleTestData> {
 public:
  YUVScaleTest() {
    switch (GetParam().yuv_type) {
      case media::YV12:
        ReadYV12Data(&yuv_bytes_);
        break;
      case media::YV16:
        ReadYV16Data(&yuv_bytes_);
        break;
    }

    rgb_bytes_.reset(new uint8[kRGBSizeScaled]);
  }

  // Helpers for getting the proper Y, U and V plane offsets.
  uint8* y_plane() { return yuv_bytes_.get(); }
  uint8* u_plane() { return yuv_bytes_.get() + kSourceYSize; }
  uint8* v_plane() {
    switch (GetParam().yuv_type) {
      case media::YV12:
        return yuv_bytes_.get() + kSourceVOffset;
      case media::YV16:
        return yuv_bytes_.get() + kSourceYSize * 3 / 2;
    }
    return NULL;
  }

  scoped_array<uint8> yuv_bytes_;
  scoped_array<uint8> rgb_bytes_;
};

TEST_P(YUVScaleTest, Normal) {
  media::ScaleYUVToRGB32(y_plane(),                    // Y
                         u_plane(),                    // U
                         v_plane(),                    // V
                         rgb_bytes_.get(),             // RGB output
                         kSourceWidth, kSourceHeight,  // Dimensions
                         kScaledWidth, kScaledHeight,  // Dimensions
                         kSourceWidth,                 // YStride
                         kSourceWidth / 2,             // UvStride
                         kScaledWidth * kBpp,          // RgbStride
                         GetParam().yuv_type,
                         media::ROTATE_0,
                         GetParam().scale_filter);

  uint32 rgb_hash = DJB2Hash(rgb_bytes_.get(), kRGBSizeScaled, kDJB2HashSeed);
  EXPECT_EQ(GetParam().rgb_hash, rgb_hash);
}

TEST_P(YUVScaleTest, ZeroSourceSize) {
  media::ScaleYUVToRGB32(y_plane(),                    // Y
                         u_plane(),                    // U
                         v_plane(),                    // V
                         rgb_bytes_.get(),             // RGB output
                         0, 0,                         // Dimensions
                         kScaledWidth, kScaledHeight,  // Dimensions
                         kSourceWidth,                 // YStride
                         kSourceWidth / 2,             // UvStride
                         kScaledWidth * kBpp,          // RgbStride
                         GetParam().yuv_type,
                         media::ROTATE_0,
                         GetParam().scale_filter);

  // Testing for out-of-bound read/writes with AddressSanitizer.
}

TEST_P(YUVScaleTest, ZeroDestinationSize) {
  media::ScaleYUVToRGB32(y_plane(),                    // Y
                         u_plane(),                    // U
                         v_plane(),                    // V
                         rgb_bytes_.get(),             // RGB output
                         kSourceWidth, kSourceHeight,  // Dimensions
                         0, 0,                         // Dimensions
                         kSourceWidth,                 // YStride
                         kSourceWidth / 2,             // UvStride
                         kScaledWidth * kBpp,          // RgbStride
                         GetParam().yuv_type,
                         media::ROTATE_0,
                         GetParam().scale_filter);

  // Testing for out-of-bound read/writes with AddressSanitizer.
}

INSTANTIATE_TEST_CASE_P(
    YUVScaleFormats, YUVScaleTest,
    ::testing::Values(
        YUVScaleTestData(media::YV12, media::FILTER_NONE, 4259656254u),
        YUVScaleTestData(media::YV16, media::FILTER_NONE, 974965419u),
        YUVScaleTestData(media::YV12, media::FILTER_BILINEAR, 2086305576u),
        YUVScaleTestData(media::YV16, media::FILTER_BILINEAR, 3857179240u)));

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
  scoped_array<uint8> rgb_bytes;
  scoped_array<uint8> yuv_converted_bytes(new uint8[kYUV12Size]);

  // Read RGB24 reference data from file.
  ReadRGB24Data(&rgb_bytes);

  // Convert to I420.
  media::ConvertRGB24ToYUV(rgb_bytes.get(),
                           yuv_converted_bytes.get(),
                           yuv_converted_bytes.get() + kSourceUOffset,
                           yuv_converted_bytes.get() + kSourceVOffset,
                           kSourceWidth, kSourceHeight,        // Dimensions
                           kSourceWidth * 3,                   // RGBStride
                           kSourceWidth,                       // YStride
                           kSourceWidth / 2);                  // UVStride

  uint32 rgb_hash = DJB2Hash(yuv_converted_bytes.get(), kYUV12Size,
                             kDJB2HashSeed);
  EXPECT_EQ(320824432u, rgb_hash);
}

TEST(YUVConvertTest, RGB32ToYUV) {
  // Allocate all surfaces.
  scoped_array<uint8> yuv_bytes(new uint8[kYUV12Size]);
  scoped_array<uint8> rgb_bytes(new uint8[kRGBSize]);
  scoped_array<uint8> yuv_converted_bytes(new uint8[kYUV12Size]);
  scoped_array<uint8> rgb_converted_bytes(new uint8[kRGBSize]);

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
                           yuv_bytes.get() + kSourceUOffset,
                           yuv_bytes.get() + kSourceVOffset,
                           rgb_bytes.get(),            // RGB output
                           kSourceWidth, kSourceHeight,          // Dimensions
                           kSourceWidth,                         // YStride
                           kSourceWidth / 2,                     // UVStride
                           kSourceWidth * kBpp,                  // RGBStride
                           media::YV12);

  // Convert RGB32 to YV12.
  media::ConvertRGB32ToYUV(rgb_bytes.get(),
                           yuv_converted_bytes.get(),
                           yuv_converted_bytes.get() + kSourceUOffset,
                           yuv_converted_bytes.get() + kSourceVOffset,
                           kSourceWidth, kSourceHeight,        // Dimensions
                           kSourceWidth * 4,                   // RGBStride
                           kSourceWidth,                       // YStride
                           kSourceWidth / 2);                  // UVStride

  // Convert YV12 back to RGB32.
  media::ConvertYUVToRGB32(yuv_converted_bytes.get(),
                           yuv_converted_bytes.get() + kSourceUOffset,
                           yuv_converted_bytes.get() + kSourceVOffset,
                           rgb_converted_bytes.get(),            // RGB output
                           kSourceWidth, kSourceHeight,          // Dimensions
                           kSourceWidth,                         // YStride
                           kSourceWidth / 2,                     // UVStride
                           kSourceWidth * kBpp,                  // RGBStride
                           media::YV12);

  int error = 0;
  for (int i = 0; i < kRGBSize; ++i) {
    int diff = rgb_converted_bytes[i] - rgb_bytes[i];
    if (diff < 0)
      diff = -diff;
    error += diff;
  }

  // Make sure error is within bound.
  LOG(INFO) << "Average error per channel: " << error / kRGBSize;
  EXPECT_GT(5, error / kRGBSize);
}

TEST(YUVConvertTest, YUY2ToYUV) {
  // Allocate all surfaces.
  scoped_array<uint8> yuy_bytes;
  scoped_array<uint8> yuv_converted_bytes(new uint8[kYUV12Size]);

  // Read YUY reference data from file.
  ReadYUY2Data(&yuy_bytes);

  // Convert to I420.
  media::ConvertYUY2ToYUV(yuy_bytes.get(),
                          yuv_converted_bytes.get(),
                          yuv_converted_bytes.get() + kSourceUOffset,
                          yuv_converted_bytes.get() + kSourceVOffset,
                          kSourceWidth, kSourceHeight);

  uint32 yuy_hash = DJB2Hash(yuv_converted_bytes.get(), kYUV12Size,
                             kDJB2HashSeed);
  EXPECT_EQ(666823187u, yuy_hash);
}

#if !defined(ARCH_CPU_ARM_FAMILY)
TEST(YUVConvertTest, RGB32ToYUV_SSE2_MatchReference) {
  if (!media::hasSSE2()) {
    LOG(WARNING) << "System doesn't support SSE2, test not executed.";
    return;
  }

  // Allocate all surfaces.
  scoped_array<uint8> yuv_bytes(new uint8[kYUV12Size]);
  scoped_array<uint8> rgb_bytes(new uint8[kRGBSize]);
  scoped_array<uint8> yuv_converted_bytes(new uint8[kYUV12Size]);
  scoped_array<uint8> yuv_reference_bytes(new uint8[kYUV12Size]);

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
  media::ConvertYUVToRGB32(
      yuv_bytes.get(),
      yuv_bytes.get() + kSourceUOffset,
      yuv_bytes.get() + kSourceVOffset,
      rgb_bytes.get(),            // RGB output
      kSourceWidth, kSourceHeight,          // Dimensions
      kSourceWidth,                         // YStride
      kSourceWidth / 2,                     // UVStride
      kSourceWidth * kBpp,                  // RGBStride
      media::YV12);

  // Convert RGB32 to YV12 with SSE2 version.
  media::ConvertRGB32ToYUV_SSE2(
      rgb_bytes.get(),
      yuv_converted_bytes.get(),
      yuv_converted_bytes.get() + kSourceUOffset,
      yuv_converted_bytes.get() + kSourceVOffset,
      kSourceWidth, kSourceHeight,        // Dimensions
      kSourceWidth * 4,                   // RGBStride
      kSourceWidth,                       // YStride
      kSourceWidth / 2);                  // UVStride

  // Convert RGB32 to YV12 with reference version.
  media::ConvertRGB32ToYUV_SSE2_Reference(
      rgb_bytes.get(),
      yuv_reference_bytes.get(),
      yuv_reference_bytes.get() + kSourceUOffset,
      yuv_reference_bytes.get() + kSourceVOffset,
      kSourceWidth, kSourceHeight,        // Dimensions
      kSourceWidth * 4,                   // RGBStride
      kSourceWidth,                       // YStride
      kSourceWidth / 2);                  // UVStride

  // Now convert a odd width and height, this overrides part of the buffer
  // generated above but that is fine because the point of this test is to
  // match the result with the reference code.

  // Convert RGB32 to YV12 with SSE2 version.
  media::ConvertRGB32ToYUV_SSE2(
      rgb_bytes.get(),
      yuv_converted_bytes.get(),
      yuv_converted_bytes.get() + kSourceUOffset,
      yuv_converted_bytes.get() + kSourceVOffset,
      7, 7,                               // Dimensions
      kSourceWidth * 4,                   // RGBStride
      kSourceWidth,                       // YStride
      kSourceWidth / 2);                  // UVStride

  // Convert RGB32 to YV12 with reference version.
  media::ConvertRGB32ToYUV_SSE2_Reference(
      rgb_bytes.get(),
      yuv_reference_bytes.get(),
      yuv_reference_bytes.get() + kSourceUOffset,
      yuv_reference_bytes.get() + kSourceVOffset,
      7, 7,                               // Dimensions
      kSourceWidth * 4,                   // RGBStride
      kSourceWidth,                       // YStride
      kSourceWidth / 2);                  // UVStride

  int error = 0;
  for (int i = 0; i < kYUV12Size; ++i) {
    int diff = yuv_reference_bytes[i] - yuv_converted_bytes[i];
    if (diff < 0)
      diff = -diff;
    error += diff;
  }

  // Make sure there's no difference from the reference.
  EXPECT_EQ(0, error);
}
#endif
