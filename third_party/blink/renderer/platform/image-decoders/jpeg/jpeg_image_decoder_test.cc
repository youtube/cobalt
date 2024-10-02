/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/image-decoders/jpeg/jpeg_image_decoder.h"

#include <limits>
#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image_metrics.h"
#include "third_party/blink/renderer/platform/image-decoders/image_animation.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

static const size_t kLargeEnoughSize = 1000 * 1000;

namespace {

std::unique_ptr<JPEGImageDecoder> CreateJPEGDecoder(size_t max_decoded_bytes) {
  return std::make_unique<JPEGImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ColorBehavior::TransformToSRGB(),
      max_decoded_bytes);
}

std::unique_ptr<ImageDecoder> CreateJPEGDecoder() {
  return CreateJPEGDecoder(ImageDecoder::kNoDecodedImageByteLimit);
}

void Downsample(size_t max_decoded_bytes,
                const char* image_file_path,
                const gfx::Size& expected_size) {
  scoped_refptr<SharedBuffer> data = ReadFile(image_file_path);
  ASSERT_TRUE(data);

  std::unique_ptr<ImageDecoder> decoder = CreateJPEGDecoder(max_decoded_bytes);
  decoder->SetData(data.get(), true);

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(expected_size.width(), frame->Bitmap().width());
  EXPECT_EQ(expected_size.height(), frame->Bitmap().height());
  EXPECT_EQ(expected_size, decoder->DecodedSize());
}

void ReadYUV(size_t max_decoded_bytes,
             const char* image_file_path,
             const gfx::Size& expected_y_size,
             const gfx::Size& expected_uv_size,
             const bool expect_decoding_failure = false) {
  scoped_refptr<SharedBuffer> data = ReadFile(image_file_path);
  ASSERT_TRUE(data);

  std::unique_ptr<JPEGImageDecoder> decoder =
      CreateJPEGDecoder(max_decoded_bytes);
  decoder->SetData(data.get(), true);

  ASSERT_TRUE(decoder->IsSizeAvailable());
  ASSERT_TRUE(decoder->CanDecodeToYUV());

  gfx::Size size = decoder->DecodedSize();

  gfx::Size y_size = decoder->DecodedYUVSize(cc::YUVIndex::kY);
  gfx::Size u_size = decoder->DecodedYUVSize(cc::YUVIndex::kU);
  gfx::Size v_size = decoder->DecodedYUVSize(cc::YUVIndex::kV);

  EXPECT_EQ(size, y_size);
  EXPECT_EQ(u_size, v_size);

  EXPECT_EQ(expected_y_size, y_size);
  EXPECT_EQ(expected_uv_size, u_size);

  wtf_size_t row_bytes[3];
  row_bytes[0] = decoder->DecodedYUVWidthBytes(cc::YUVIndex::kY);
  row_bytes[1] = decoder->DecodedYUVWidthBytes(cc::YUVIndex::kU);
  row_bytes[2] = decoder->DecodedYUVWidthBytes(cc::YUVIndex::kV);

  size_t planes_data_size = row_bytes[0] * y_size.height() +
                            row_bytes[1] * u_size.height() +
                            row_bytes[2] * v_size.height();
  auto planes_data = std::make_unique<char[]>(planes_data_size);

  void* planes[3];
  planes[0] = planes_data.get();
  planes[1] = static_cast<char*>(planes[0]) + row_bytes[0] * y_size.height();
  planes[2] = static_cast<char*>(planes[1]) + row_bytes[1] * u_size.height();

  decoder->SetImagePlanes(
      std::make_unique<ImagePlanes>(planes, row_bytes, kGray_8_SkColorType));

  decoder->DecodeToYUV();

  EXPECT_EQ(expect_decoding_failure, decoder->Failed());
  EXPECT_TRUE(decoder->HasDisplayableYUVData());
}

}  // anonymous namespace

// Tests failure on a too big image.
TEST(JPEGImageDecoderTest, tooBig) {
  std::unique_ptr<ImageDecoder> decoder = CreateJPEGDecoder(100);
  EXPECT_FALSE(decoder->SetSize(10000u, 10000u));
  EXPECT_TRUE(decoder->Failed());
}

// Tests that the JPEG decoder can downsample image whose width and height are
// multiples of 8, to ensure we compute the correct DecodedSize and pass correct
// parameters to libjpeg to output the image with the expected size.
TEST(JPEGImageDecoderTest, downsampleImageSizeMultipleOf8) {
  const char* jpeg_file = "/images/resources/gracehopper.jpg";  // 256x256

  // 1/8 downsample.
  Downsample(40 * 40 * 4, jpeg_file, gfx::Size(32, 32));

  // 2/8 downsample.
  Downsample(70 * 70 * 4, jpeg_file, gfx::Size(64, 64));

  // 3/8 downsample.
  Downsample(100 * 100 * 4, jpeg_file, gfx::Size(96, 96));

  // 4/8 downsample.
  Downsample(130 * 130 * 4, jpeg_file, gfx::Size(128, 128));

  // 5/8 downsample.
  Downsample(170 * 170 * 4, jpeg_file, gfx::Size(160, 160));

  // 6/8 downsample.
  Downsample(200 * 200 * 4, jpeg_file, gfx::Size(192, 192));

  // 7/8 downsample.
  Downsample(230 * 230 * 4, jpeg_file, gfx::Size(224, 224));
}

// Tests that JPEG decoder can downsample image whose width and height are not
// multiple of 8. Ensures that we round using the same algorithm as libjpeg.
TEST(JPEGImageDecoderTest, downsampleImageSizeNotMultipleOf8) {
  const char* jpeg_file = "/images/resources/icc-v2-gbr.jpg";  // 275x207

  // 1/8 downsample.
  Downsample(40 * 40 * 4, jpeg_file, gfx::Size(35, 26));

  // 2/8 downsample.
  Downsample(70 * 70 * 4, jpeg_file, gfx::Size(69, 52));

  // 3/8 downsample.
  Downsample(100 * 100 * 4, jpeg_file, gfx::Size(104, 78));

  // 4/8 downsample.
  Downsample(130 * 130 * 4, jpeg_file, gfx::Size(138, 104));

  // 5/8 downsample.
  Downsample(170 * 170 * 4, jpeg_file, gfx::Size(172, 130));

  // 6/8 downsample.
  Downsample(200 * 200 * 4, jpeg_file, gfx::Size(207, 156));

  // 7/8 downsample.
  Downsample(230 * 230 * 4, jpeg_file, gfx::Size(241, 182));
}

// Tests that upsampling is not allowed.
TEST(JPEGImageDecoderTest, upsample) {
  const char* jpeg_file = "/images/resources/gracehopper.jpg";  // 256x256
  Downsample(kLargeEnoughSize, jpeg_file, gfx::Size(256, 256));
}

TEST(JPEGImageDecoderTest, yuv) {
  // This image is 256x256 with YUV 4:2:0
  const char* jpeg_file = "/images/resources/gracehopper.jpg";
  ReadYUV(kLargeEnoughSize, jpeg_file, gfx::Size(256, 256),
          gfx::Size(128, 128));

  // Each plane is in its own scan.
  const char* jpeg_file_non_interleaved =
      "/images/resources/cs-uma-ycbcr-420-non-interleaved.jpg";  // 64x64
  ReadYUV(kLargeEnoughSize, jpeg_file_non_interleaved, gfx::Size(64, 64),
          gfx::Size(32, 32));

  const char* jpeg_file_image_size_not_multiple_of8 =
      "/images/resources/cropped_mandrill.jpg";  // 439x154
  ReadYUV(kLargeEnoughSize, jpeg_file_image_size_not_multiple_of8,
          gfx::Size(439, 154), gfx::Size(220, 77));

  // Make sure we revert to RGBA decoding when we're about to downscale,
  // which can occur on memory-constrained android devices.
  scoped_refptr<SharedBuffer> data = ReadFile(jpeg_file);
  ASSERT_TRUE(data);

  std::unique_ptr<JPEGImageDecoder> decoder = CreateJPEGDecoder(230 * 230 * 4);
  decoder->SetData(data.get(), true);

  ASSERT_TRUE(decoder->IsSizeAvailable());
  ASSERT_FALSE(decoder->CanDecodeToYUV());
}

// Tests that a progressive image missing an EOI marker causes a YUV decoding
// failure but also results in displayable YUV data.
TEST(JPEGImageDecoderTest, missingEoi) {
  const char* jpeg_file = "/images/resources/missing-eoi.jpg";  // 1599x899
  ReadYUV((1599 * 899 * 4), jpeg_file, gfx::Size(1599, 899),
          gfx::Size(800, 450),
          /*expect_decoding_failure=*/true);
}

TEST(JPEGImageDecoderTest,
     byteByByteBaselineJPEGWithColorProfileAndRestartMarkers) {
  TestByteByByteDecode(&CreateJPEGDecoder,
                       "/images/resources/"
                       "small-square-with-colorspin-profile.jpg",
                       1u, kAnimationNone);
}

TEST(JPEGImageDecoderTest, byteByByteProgressiveJPEG) {
  TestByteByByteDecode(&CreateJPEGDecoder, "/images/resources/bug106024.jpg",
                       1u, kAnimationNone);
}

TEST(JPEGImageDecoderTest, byteByByteRGBJPEGWithAdobeMarkers) {
  TestByteByByteDecode(&CreateJPEGDecoder,
                       "/images/resources/rgb-jpeg-with-adobe-marker-only.jpg",
                       1u, kAnimationNone);
}

// This test verifies that calling SharedBuffer::MergeSegmentsIntoBuffer() does
// not break JPEG decoding at a critical point: in between a call to decode the
// size (when JPEGImageDecoder stops while it may still have input data to
// read) and a call to do a full decode.
TEST(JPEGImageDecoderTest, mergeBuffer) {
  const char* jpeg_file = "/images/resources/gracehopper.jpg";
  TestMergeBuffer(&CreateJPEGDecoder, jpeg_file);
}

// This tests decoding a JPEG with many progressive scans.  Decoding should
// fail, but not hang (crbug.com/642462).
TEST(JPEGImageDecoderTest, manyProgressiveScans) {
  scoped_refptr<SharedBuffer> test_data =
      ReadFile(kDecodersTestingDir, "many-progressive-scans.jpg");
  ASSERT_TRUE(test_data.get());

  std::unique_ptr<ImageDecoder> test_decoder = CreateJPEGDecoder();
  test_decoder->SetData(test_data.get(), true);
  EXPECT_EQ(1u, test_decoder->FrameCount());
  ASSERT_TRUE(test_decoder->DecodeFrameBufferAtIndex(0));
  EXPECT_TRUE(test_decoder->Failed());
}

// Decode a JPEG with EXIF data that defines a density corrected size. The EXIF
// data has the initial IFD at the end of the data blob, and out-of-line data
// defined just after the header.
// The order of the EXIF data is:
//   <header> <out-of-line data> <Exif IFD> <0th IFD>
TEST(JPEGImageDecoderTest, exifWithInitialIfdLast) {
  scoped_refptr<SharedBuffer> test_data =
      ReadFile(kDecodersTestingDir, "green-exif-ifd-last.jpg");
  ASSERT_TRUE(test_data.get());

  std::unique_ptr<ImageDecoder> test_decoder = CreateJPEGDecoder();
  test_decoder->SetData(test_data.get(), true);
  EXPECT_EQ(1u, test_decoder->FrameCount());
  ASSERT_TRUE(test_decoder->DecodeFrameBufferAtIndex(0));
  EXPECT_EQ(test_decoder->Orientation(), ImageOrientationEnum::kOriginTopRight);
  EXPECT_EQ(test_decoder->DensityCorrectedSize(), gfx::Size(32, 32));
}

TEST(JPEGImageDecoderTest, SupportedSizesSquare) {
  const char* jpeg_file = "/images/resources/gracehopper.jpg";  // 256x256
  scoped_refptr<SharedBuffer> data = ReadFile(jpeg_file);
  ASSERT_TRUE(data);

  std::unique_ptr<ImageDecoder> decoder =
      CreateJPEGDecoder(std::numeric_limits<int>::max());
  decoder->SetData(data.get(), true);
  // This will decode the size and needs to be called to avoid DCHECKs
  ASSERT_TRUE(decoder->IsSizeAvailable());
  Vector<SkISize> expected_sizes = {
      SkISize::Make(32, 32),   SkISize::Make(64, 64),   SkISize::Make(96, 96),
      SkISize::Make(128, 128), SkISize::Make(160, 160), SkISize::Make(192, 192),
      SkISize::Make(224, 224), SkISize::Make(256, 256)};
  auto sizes = decoder->GetSupportedDecodeSizes();
  ASSERT_EQ(expected_sizes.size(), sizes.size());
  for (size_t i = 0; i < sizes.size(); ++i) {
    EXPECT_TRUE(expected_sizes[i] == sizes[i])
        << "Expected " << expected_sizes[i].width() << "x"
        << expected_sizes[i].height() << ". Got " << sizes[i].width() << "x"
        << sizes[i].height();
  }
}

TEST(JPEGImageDecoderTest, SupportedSizesRectangle) {
  // This 272x200 image uses 4:2:2 sampling format. The MCU is therefore 16x8.
  // The width is a multiple of 16 and the height is a multiple of 8, so it's
  // okay for the decoder to downscale it.
  const char* jpeg_file = "/images/resources/icc-v2-gbr-422-whole-mcus.jpg";

  scoped_refptr<SharedBuffer> data = ReadFile(jpeg_file);
  ASSERT_TRUE(data);

  std::unique_ptr<ImageDecoder> decoder =
      CreateJPEGDecoder(std::numeric_limits<int>::max());
  decoder->SetData(data.get(), true);
  // This will decode the size and needs to be called to avoid DCHECKs
  ASSERT_TRUE(decoder->IsSizeAvailable());
  Vector<SkISize> expected_sizes = {
      SkISize::Make(34, 25),   SkISize::Make(68, 50),   SkISize::Make(102, 75),
      SkISize::Make(136, 100), SkISize::Make(170, 125), SkISize::Make(204, 150),
      SkISize::Make(238, 175), SkISize::Make(272, 200)};

  auto sizes = decoder->GetSupportedDecodeSizes();
  ASSERT_EQ(expected_sizes.size(), sizes.size());
  for (size_t i = 0; i < sizes.size(); ++i) {
    EXPECT_TRUE(expected_sizes[i] == sizes[i])
        << "Expected " << expected_sizes[i].width() << "x"
        << expected_sizes[i].height() << ". Got " << sizes[i].width() << "x"
        << sizes[i].height();
  }
}

TEST(JPEGImageDecoderTest,
     SupportedSizesRectangleNotMultipleOfMCUIfMemoryBound) {
  // This 275x207 image uses 4:2:0 sampling format. The MCU is therefore 16x16.
  // Neither the width nor the height is a multiple of the MCU, so downscaling
  // should not be supported. However, we limit the memory so that the decoder
  // is forced to support downscaling.
  const char* jpeg_file = "/images/resources/icc-v2-gbr.jpg";

  scoped_refptr<SharedBuffer> data = ReadFile(jpeg_file);
  ASSERT_TRUE(data);

  // Make the memory limit one fewer byte than what is needed in order to force
  // downscaling.
  std::unique_ptr<ImageDecoder> decoder = CreateJPEGDecoder(275 * 207 * 4 - 1);
  decoder->SetData(data.get(), true);
  // This will decode the size and needs to be called to avoid DCHECKs
  ASSERT_TRUE(decoder->IsSizeAvailable());
  Vector<SkISize> expected_sizes = {
      SkISize::Make(35, 26),   SkISize::Make(69, 52),   SkISize::Make(104, 78),
      SkISize::Make(138, 104), SkISize::Make(172, 130), SkISize::Make(207, 156),
      SkISize::Make(241, 182)};

  auto sizes = decoder->GetSupportedDecodeSizes();
  ASSERT_EQ(expected_sizes.size(), sizes.size());
  for (size_t i = 0; i < sizes.size(); ++i) {
    EXPECT_TRUE(expected_sizes[i] == sizes[i])
        << "Expected " << expected_sizes[i].width() << "x"
        << expected_sizes[i].height() << ". Got " << sizes[i].width() << "x"
        << sizes[i].height();
  }
}

TEST(JPEGImageDecoderTest, SupportedSizesRectangleNotMultipleOfMCU) {
  struct {
    const char* jpeg_file;
    SkISize expected_size;
  } recs[] = {
      {// This 264x192 image uses 4:2:0 sampling format. The MCU is therefore
       // 16x16. The height is a multiple of 16, but the width is not a
       // multiple of 16, so it's not okay for the decoder to downscale it.
       "/images/resources/icc-v2-gbr-420-width-not-whole-mcu.jpg",
       SkISize::Make(264, 192)},
      {// This 272x200 image uses 4:2:0 sampling format. The MCU is therefore
       // 16x16. The width is a multiple of 16, but the width is not a multiple
       // of 16, so it's not okay for the decoder to downscale it.
       "/images/resources/icc-v2-gbr-420-height-not-whole-mcu.jpg",
       SkISize::Make(272, 200)}};
  for (const auto& rec : recs) {
    scoped_refptr<SharedBuffer> data = ReadFile(rec.jpeg_file);
    ASSERT_TRUE(data);
    std::unique_ptr<ImageDecoder> decoder =
        CreateJPEGDecoder(std::numeric_limits<int>::max());
    decoder->SetData(data.get(), true);
    // This will decode the size and needs to be called to avoid DCHECKs
    ASSERT_TRUE(decoder->IsSizeAvailable());
    auto sizes = decoder->GetSupportedDecodeSizes();
    ASSERT_EQ(1u, sizes.size());
    EXPECT_EQ(rec.expected_size, sizes[0])
        << "Expected " << rec.expected_size.width() << "x"
        << rec.expected_size.height() << ". Got " << sizes[0].width() << "x"
        << sizes[0].height();
  }
}

TEST(JPEGImageDecoderTest, SupportedSizesTruncatedIfMemoryBound) {
  const char* jpeg_file = "/images/resources/gracehopper.jpg";  // 256x256
  scoped_refptr<SharedBuffer> data = ReadFile(jpeg_file);
  ASSERT_TRUE(data);

  // Limit the memory so that 128 would be the largest size possible.
  std::unique_ptr<ImageDecoder> decoder = CreateJPEGDecoder(130 * 130 * 4);
  decoder->SetData(data.get(), true);
  // This will decode the size and needs to be called to avoid DCHECKs
  ASSERT_TRUE(decoder->IsSizeAvailable());
  Vector<SkISize> expected_sizes = {
      SkISize::Make(32, 32), SkISize::Make(64, 64), SkISize::Make(96, 96),
      SkISize::Make(128, 128)};
  auto sizes = decoder->GetSupportedDecodeSizes();
  ASSERT_EQ(expected_sizes.size(), sizes.size());
  for (size_t i = 0; i < sizes.size(); ++i) {
    EXPECT_TRUE(expected_sizes[i] == sizes[i])
        << "Expected " << expected_sizes[i].width() << "x"
        << expected_sizes[i].height() << ". Got " << sizes[i].width() << "x"
        << sizes[i].height();
  }
}

TEST(JPEGImageDecoderTest, SupportedScaleNumeratorBound) {
  auto numerator_default = JPEGImageDecoder::DesiredScaleNumerator(10, 9, 8);
  ASSERT_EQ(numerator_default, static_cast<unsigned>(8));

  auto numerator_normal =
      JPEGImageDecoder::DesiredScaleNumerator(1024, 2048, 8);
  ASSERT_EQ(numerator_normal, static_cast<unsigned>(5));

  auto numerator_overflow =
      JPEGImageDecoder::DesiredScaleNumerator(0x4000000, 0x4100000, 8);
  ASSERT_EQ(numerator_overflow, static_cast<unsigned>(7));
}

struct ColorSpaceTestParam {
  std::string file;
  bool expected_success = false;
  BitmapImageMetrics::JpegColorSpace expected_color_space;
  bool expect_yuv_decoding = false;
  gfx::Size expected_uv_size;
};

void PrintTo(const ColorSpaceTestParam& param, std::ostream* os) {
  *os << "{\"" << param.file << "\", " << param.expected_success << ","
      << static_cast<int>(param.expected_color_space) << ","
      << param.expected_uv_size.ToString() << "," << param.expect_yuv_decoding
      << "}";
}

class ColorSpaceTest : public ::testing::TestWithParam<ColorSpaceTestParam> {};

// Tests that the JPEG color space/subsampling is recorded correctly as a UMA
// for a variety of images. When the decode fails, no UMA should be recorded.
TEST_P(ColorSpaceTest, CorrectColorSpaceUMARecorded) {
  base::HistogramTester histogram_tester;
  scoped_refptr<SharedBuffer> data =
      ReadFile(("/images/resources/" + GetParam().file).c_str());
  ASSERT_TRUE(data);

  std::unique_ptr<ImageDecoder> decoder = CreateJPEGDecoder();
  decoder->SetData(data.get(), true);

  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);

  if (GetParam().expected_success) {
    ASSERT_FALSE(decoder->Failed());
    histogram_tester.ExpectUniqueSample("Blink.ImageDecoders.Jpeg.ColorSpace",
                                        GetParam().expected_color_space, 1);
  } else {
    ASSERT_TRUE(decoder->Failed());
    histogram_tester.ExpectTotalCount("Blink.ImageDecoders.Jpeg.ColorSpace", 0);
  }
}

// Tests YUV decoding path with different color encodings (and chroma
// subsamplings if applicable).
TEST_P(ColorSpaceTest, YuvDecode) {
  // Test only successful decoding
  if (!GetParam().expected_success)
    return;

  if (GetParam().expect_yuv_decoding) {
    const auto jpeg_file = ("/images/resources/" + GetParam().file);
    ReadYUV(kLargeEnoughSize, jpeg_file.c_str(), gfx::Size(64, 64),
            GetParam().expected_uv_size,
            /*expect_decoding_failure=*/false);
  }
}

// Tests RGB decoding path with different color encodings (and chroma
// subsamplings if applicable).
TEST_P(ColorSpaceTest, RgbDecode) {
  // Test only successful decoding
  if (!GetParam().expected_success)
    return;

  if (!GetParam().expect_yuv_decoding) {
    const auto jpeg_file = ("/images/resources/" + GetParam().file);
    scoped_refptr<SharedBuffer> data = ReadFile(jpeg_file.c_str());
    ASSERT_TRUE(data);

    std::unique_ptr<ImageDecoder> decoder = CreateJPEGDecoder(kLargeEnoughSize);
    decoder->SetData(data.get(), true);

    gfx::Size size = decoder->DecodedSize();
    EXPECT_EQ(gfx::Size(64, 64), size);
    ASSERT_FALSE(decoder->CanDecodeToYUV());

    const ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
    ASSERT_TRUE(frame);
    EXPECT_EQ(frame->GetStatus(), ImageFrame::kFrameComplete);
    EXPECT_FALSE(decoder->Failed());
    return;
  }
}

const ColorSpaceTest::ParamType kColorSpaceTestParams[] = {
    {"cs-uma-grayscale.jpg", true,
     BitmapImageMetrics::JpegColorSpace::kGrayscale},
    {"cs-uma-rgb.jpg", true, BitmapImageMetrics::JpegColorSpace::kRGB},
    // Each component is in a separate scan. Should not make a difference.
    {"cs-uma-rgb-non-interleaved.jpg", true,
     BitmapImageMetrics::JpegColorSpace::kRGB},
    {"cs-uma-cmyk.jpg", true, BitmapImageMetrics::JpegColorSpace::kCMYK},
    // 4 components/no markers, so we expect libjpeg_turbo to guess CMYK.
    {"cs-uma-cmyk-no-jfif-or-adobe-markers.jpg", true,
     BitmapImageMetrics::JpegColorSpace::kCMYK},
    // 4 components are not legal in JFIF, but we expect libjpeg_turbo to guess
    // CMYK.
    {"cs-uma-cmyk-jfif-marker.jpg", true,
     BitmapImageMetrics::JpegColorSpace::kCMYK},
    {"cs-uma-ycck.jpg", true, BitmapImageMetrics::JpegColorSpace::kYCCK},
    // Contains CMYK data but uses a bad Adobe color transform, so libjpeg_turbo
    // will guess YCCK.
    {"cs-uma-cmyk-unknown-transform.jpg", true,
     BitmapImageMetrics::JpegColorSpace::kYCCK},
    {"cs-uma-ycbcr-410.jpg", true,
     BitmapImageMetrics::JpegColorSpace::kYCbCr410, false},
    {"cs-uma-ycbcr-411.jpg", true,
     BitmapImageMetrics::JpegColorSpace::kYCbCr411, false},
    {"cs-uma-ycbcr-420.jpg", true,
     BitmapImageMetrics::JpegColorSpace::kYCbCr420, true, gfx::Size(32, 32)},
    // Each component is in a separate scan. Should not make a difference.
    {"cs-uma-ycbcr-420-non-interleaved.jpg", true,
     BitmapImageMetrics::JpegColorSpace::kYCbCr420, true, gfx::Size(32, 32)},
    // 3 components/both JFIF and Adobe markers, so we expect libjpeg_turbo to
    // guess YCbCr.
    {"cs-uma-ycbcr-420-both-jfif-adobe.jpg", true,
     BitmapImageMetrics::JpegColorSpace::kYCbCr420, true, gfx::Size(32, 32)},
    {"cs-uma-ycbcr-422.jpg", true,
     BitmapImageMetrics::JpegColorSpace::kYCbCr422, true, gfx::Size(32, 64)},
    {"cs-uma-ycbcr-440.jpg", true,
     BitmapImageMetrics::JpegColorSpace::kYCbCr440, false},
    {"cs-uma-ycbcr-444.jpg", true,
     BitmapImageMetrics::JpegColorSpace::kYCbCr444, true, gfx::Size(64, 64)},
    // Contains RGB data but uses a bad Adobe color transform, so libjpeg_turbo
    // will guess YCbCr.
    {"cs-uma-rgb-unknown-transform.jpg", true,
     BitmapImageMetrics::JpegColorSpace::kYCbCr444, true, gfx::Size(64, 64)},
    {"cs-uma-ycbcr-other.jpg", true,
     BitmapImageMetrics::JpegColorSpace::kYCbCrOther, false},
    // Contains only 2 components. We expect the decode to fail and not produce
    // any samples.
    {"cs-uma-two-channels-jfif-marker.jpg", false}};

INSTANTIATE_TEST_SUITE_P(JPEGImageDecoderTest,
                         ColorSpaceTest,
                         ::testing::ValuesIn(kColorSpaceTestParams));

TEST(JPEGImageDecoderTest, PartialDataWithoutSize) {
  const char* jpeg_file = "/images/resources/gracehopper.jpg";
  scoped_refptr<SharedBuffer> full_data = ReadFile(jpeg_file);
  ASSERT_TRUE(full_data);

  constexpr size_t kDataLengthWithoutSize = 4;
  ASSERT_LT(kDataLengthWithoutSize, full_data->size());
  scoped_refptr<SharedBuffer> partial_data =
      SharedBuffer::Create(full_data->Data(), kDataLengthWithoutSize);

  std::unique_ptr<ImageDecoder> decoder = CreateJPEGDecoder();
  decoder->SetData(partial_data.get(), false);
  EXPECT_FALSE(decoder->IsSizeAvailable());
  EXPECT_FALSE(decoder->Failed());
  decoder->SetData(full_data.get(), true);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_FALSE(decoder->Failed());
}

TEST(JPEGImageDecoderTest, PartialRgbDecodeBlocksYuvDecoding) {
  const char* jpeg_file = "/images/resources/non-interleaved_progressive.jpg";
  scoped_refptr<SharedBuffer> full_data = ReadFile(jpeg_file);
  ASSERT_TRUE(full_data);

  {
    auto yuv_decoder = CreateJPEGDecoder();
    yuv_decoder->SetData(full_data.get(), true);
    EXPECT_TRUE(yuv_decoder->IsSizeAvailable());
    EXPECT_FALSE(yuv_decoder->Failed());
    EXPECT_TRUE(yuv_decoder->CanDecodeToYUV());
  }

  const size_t kJustEnoughDataToStartHeaderParsing =
      (full_data->size() + 1) / 2;
  auto partial_data = SharedBuffer::Create(full_data->Data(),
                                           kJustEnoughDataToStartHeaderParsing);
  ASSERT_TRUE(partial_data);

  auto decoder = CreateJPEGDecoder();
  decoder->SetData(partial_data.get(), false);
  EXPECT_TRUE(decoder->IsSizeAvailable());
  EXPECT_FALSE(decoder->Failed());
  EXPECT_FALSE(decoder->CanDecodeToYUV());

  const ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_NE(frame->GetStatus(), ImageFrame::kFrameComplete);
  decoder->SetData(full_data.get(), true);
  EXPECT_FALSE(decoder->CanDecodeToYUV());
}

}  // namespace blink
