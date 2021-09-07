// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/loader/image/animated_webp_image.h"
#include "cobalt/loader/image/image_decoder_mock.h"
#include "cobalt/loader/image/jpeg_image_decoder.h"
#include "starboard/configuration.h"

namespace cobalt {
namespace loader {
namespace image {
namespace {
base::FilePath GetTestImagePath(const char* file_name) {
  base::FilePath data_directory;
  CHECK(base::PathService::Get(base::DIR_TEST_DATA, &data_directory));
  return data_directory.Append(FILE_PATH_LITERAL("cobalt"))
      .Append(FILE_PATH_LITERAL("loader"))
      .Append(FILE_PATH_LITERAL("testdata"))
      .Append(FILE_PATH_LITERAL(file_name));
}

std::vector<uint8> GetImageData(const base::FilePath& file_path) {
  int64 size;
  std::vector<uint8> image_data;

  bool success = base::GetFileSize(file_path, &size);

  CHECK(success) << "Could not get file size.";
  CHECK_GT(size, 0);

  image_data.resize(static_cast<size_t>(size));

  int num_of_bytes =
      base::ReadFile(file_path, reinterpret_cast<char*>(&image_data[0]),
                     static_cast<int>(size));

  CHECK_EQ(num_of_bytes, static_cast<int>(image_data.size()))
      << "Could not read '" << file_path.value() << "'.";
  return image_data;
}

// Check if pixels are the same as |test_color|.
::testing::AssertionResult CheckSameColor(const uint8* pixels, int width,
                                          int height, uint32 test_color) {
  // Iterate through each pixel testing that the value is the expected value.
  for (int index = 0; index < width * height; ++index) {
    uint32 current_color = static_cast<uint32>(
        (pixels[0] << 24) | (pixels[1] << 16) | (pixels[2] << 8) | pixels[3]);
    pixels += 4;
    if (current_color != test_color) {
      return ::testing::AssertionFailure()
             << "'current_color' should be " << test_color << " but is "
             << current_color;
    }
  }
  return ::testing::AssertionSuccess();
}

// Check if color component in the plane are the same as |test_value|.
::testing::AssertionResult CheckSameValue(const uint8* pixels, int width,
                                          int height, int pitch_in_bytes,
                                          uint8 test_value) {
  while (height > 0) {
    for (int i = 0; i < width; ++i) {
      if (pixels[i] != test_value) {
        return ::testing::AssertionFailure()
               << "'pixels[" << i << "]' should be "
               << static_cast<int>(test_value) << " but is "
               << static_cast<int>(pixels[i]);
      }
    }
    pixels += pitch_in_bytes;
    --height;
  }
  return ::testing::AssertionSuccess();
}

// Check if all pixels in an image are the same as |test_color| if it is single
// plane image, or if it is the same as the y/u/v colors if it is multi plane
// image.  Note that in the multi plane case, the function only supports
// kMultiPlaneImageFormatYUV3PlaneBT601FullRange for now..
::testing::AssertionResult CheckUniformColoredImage(
    render_tree::ImageStub* image_data, uint32 test_color, uint8 y_color,
    uint8 u_color, uint8 v_color) {
  if (image_data->is_multi_plane_image()) {
    auto raw_image_memory = image_data->GetRawImageMemory()->GetMemory();
    auto descriptor = image_data->multi_plane_descriptor();

    if (descriptor.image_format() !=
        render_tree::kMultiPlaneImageFormatYUV3PlaneBT601FullRange) {
      return ::testing::AssertionFailure()
             << "'image_format()' should be "
             << render_tree::kMultiPlaneImageFormatYUV3PlaneBT601FullRange
             << " but is " << descriptor.image_format();
    }
    if (descriptor.num_planes() != 3) {
      return ::testing::AssertionFailure()
             << "'num_planes()' should be 3 but is "
             << descriptor.image_format();
    }
    auto result = CheckSameValue(
        raw_image_memory + descriptor.GetPlaneOffset(0),
        descriptor.GetPlaneDescriptor(0).size.width(),
        descriptor.GetPlaneDescriptor(0).size.height(),
        descriptor.GetPlaneDescriptor(0).pitch_in_bytes, y_color);
    if (!result) {
      return result;
    }
    result = CheckSameValue(raw_image_memory + descriptor.GetPlaneOffset(1),
                            descriptor.GetPlaneDescriptor(1).size.width(),
                            descriptor.GetPlaneDescriptor(1).size.height(),
                            descriptor.GetPlaneDescriptor(1).pitch_in_bytes,
                            u_color);
    if (!result) {
      return result;
    }
    return CheckSameValue(raw_image_memory + descriptor.GetPlaneOffset(2),
                          descriptor.GetPlaneDescriptor(2).size.width(),
                          descriptor.GetPlaneDescriptor(2).size.height(),
                          descriptor.GetPlaneDescriptor(2).pitch_in_bytes,
                          v_color);
  }

  math::Size size = image_data->GetSize();
  uint8* pixels = image_data->GetImageData()->GetMemory();

  return CheckSameColor(pixels, size.width(), size.height(), test_color);
}

// FakeResourceProviderStub has the identical behavior as ResourceProviderStub,
// except the GetTypeId, which makes the ImageDecode not to create the
// FailureImageDecoder based on the TypeId Check.
class FakeResourceProviderStub : public render_tree::ResourceProviderStub {
  base::TypeId GetTypeId() const override {
    return base::GetTypeId<FakeResourceProviderStub>();
  }
};

}  // namespace

// TODO: Test special images like the image has gAMA chunk information,
// pngs with 16 bit depth, and large pngs.

TEST(ImageDecoderTest, DecodeImageWithContentLength0) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(
      std::string("No content returned, but expected some."));

  const char kImageWithContentLength0Headers[] = {
      "HTTP/1.1 200 OK\0"
      "Content-Length: 0\0"
      "Content-Type: image/jpeg\0"
      "Expires: Wed, 20 Apr 2016 19:33:44 GMT\0"
      "Date: Wed, 20 Apr 2016 18:33:44 GMT\0"
      "Server: UploadServer\0"
      "Accept-Ranges: bytes\0"
      "Cache-Control: public, max-age=3600\0"};
  scoped_refptr<net::HttpResponseHeaders> headers(new net::HttpResponseHeaders(
      std::string(kImageWithContentLength0Headers,
                  kImageWithContentLength0Headers +
                      sizeof(kImageWithContentLength0Headers))));
  image_decoder.OnResponseStarted(NULL, headers);
  image_decoder.Finish();

  EXPECT_FALSE(image_decoder.image());
}

TEST(ImageDecoderTest, DecodeNonImageTypeWithContentLength0) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(std::string(
      "No content returned, but expected some. Not an image mime type."));

  const char kHTMLWithContentLength0Headers[] = {
      "HTTP/1.1 200 OK\0"
      "Content-Length: 0\0"
      "Content-Type: text/html\0"
      "Expires: Wed, 20 Apr 2016 19:33:44 GMT\0"
      "Date: Wed, 20 Apr 2016 18:33:44 GMT\0"
      "Server: UploadServer\0"
      "Accept-Ranges: bytes\0"
      "Cache-Control: public, max-age=3600\0"};
  scoped_refptr<net::HttpResponseHeaders> headers(new net::HttpResponseHeaders(
      std::string(kHTMLWithContentLength0Headers,
                  kHTMLWithContentLength0Headers +
                      sizeof(kHTMLWithContentLength0Headers))));
  image_decoder.OnResponseStarted(NULL, headers);
  image_decoder.Finish();

  EXPECT_FALSE(image_decoder.image());
}

TEST(ImageDecoderTest, DecodeNonImageType) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(std::string("Not an image mime type."));

  const char kHTMLHeaders[] = {
      "HTTP/1.1 200 OK\0"
      "Content-Type: text/html; charset=UTF-8\0"
      "Expires: Wed, 20 Apr 2016 19:33:44 GMT\0"
      "Date: Wed, 20 Apr 2016 18:33:44 GMT\0"
      "Server: gws\0"
      "Accept-Ranges: none\0"
      "Cache-Control: private, max-age=0\0"};
  scoped_refptr<net::HttpResponseHeaders> headers(new net::HttpResponseHeaders(
      std::string(kHTMLHeaders, kHTMLHeaders + sizeof(kHTMLHeaders))));

  const char kContent[] = {
      "<!DOCTYPE html><html><head></head><body></body></html>"};

  image_decoder.OnResponseStarted(NULL, headers);
  image_decoder.DecodeChunk(kContent, sizeof(kContent));
  image_decoder.Finish();

  EXPECT_FALSE(image_decoder.image());
}

TEST(ImageDecoderTest, DecodeNoContentType) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(std::string("Not an image mime type."));

  const char kHTMLHeaders[] = {
      "HTTP/1.1 200 OK\0"
      "Expires: Wed, 20 Apr 2016 19:33:44 GMT\0"
      "Date: Wed, 20 Apr 2016 18:33:44 GMT\0"
      "Server: gws\0"
      "Accept-Ranges: none\0"
      "Cache-Control: private, max-age=0\0"};
  scoped_refptr<net::HttpResponseHeaders> headers(new net::HttpResponseHeaders(
      std::string(kHTMLHeaders, kHTMLHeaders + sizeof(kHTMLHeaders))));

  const char kContent[] = {
      "<!DOCTYPE html><html><head></head><body></body></html>"};

  image_decoder.OnResponseStarted(NULL, headers);
  image_decoder.DecodeChunk(kContent, sizeof(kContent));
  image_decoder.Finish();

  EXPECT_FALSE(image_decoder.image());
}

TEST(ImageDecoderTest, DecodeImageWithNoContent) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(
      std::string("No content returned. Not an image mime type."));

  const char kHTMLWithNoContentHeaders[] = {
      "HTTP/1.1 204 No Content\0"
      "Expires: Tue, 27 Apr 1971 19:44:06 EST\0"
      "Content-Length: 0\0"
      "Content-Type: text/html; charset=utf-8\0"
      "Date: Wed, 20 Apr 2016 18:33:44 GMT\0"
      "Server: Ytfe_Worker\0"
      "Accept-Ranges: bytes\0"
      "Cache-Control: no-cache\0"};
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string(
          kHTMLWithNoContentHeaders,
          kHTMLWithNoContentHeaders + sizeof(kHTMLWithNoContentHeaders))));
  image_decoder.OnResponseStarted(NULL, headers);
  image_decoder.Finish();

  EXPECT_FALSE(image_decoder.image());
}

TEST(ImageDecoderTest, DecodeImageWithLessThanHeaderBytes) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(
      std::string("No enough image data for header."));

  const char kPartialWebPHeader[] = {"RIFF"};
  image_decoder.DecodeChunk(kPartialWebPHeader, sizeof(kPartialWebPHeader));
  image_decoder.Finish();

  EXPECT_FALSE(image_decoder.image());
}

TEST(ImageDecoderTest, FailedToDecodeImage) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(
      std::string("PNGImageDecoder failed to decode image."));

  const char kPartialPNGImage[] = {
      "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A\x00\x00\x00\x0D\x49\x48\x44\x52\x00\x00"
      "\x02\x6C\x00\x00\x01\x2C"};
  image_decoder.DecodeChunk(kPartialPNGImage, sizeof(kPartialPNGImage));
  image_decoder.Finish();

  EXPECT_FALSE(image_decoder.image());
}

TEST(ImageDecoderTest, UnsupportedImageFormat) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(std::string("Unsupported image format."));

  const char kPartialICOImage[] = {
      "\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x01\x00"};
  image_decoder.DecodeChunk(kPartialICOImage, sizeof(kPartialICOImage));
  image_decoder.Finish();

  EXPECT_FALSE(image_decoder.image());
}

// Test that we can properly decode the PNG image.
TEST(ImageDecoderTest, DecodePNGImage) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(base::nullopt);

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("non_interlaced_png.png"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]),
                            image_data.size());
  image_decoder.Finish();

  // All pixels in the PNG image should have premultiplied alpha RGBA
  // values of (127, 0, 0, 127).
  uint8 r = 127;
  uint8 g = 0;
  uint8 b = 0;
  uint8 a = 127;
  uint32 expected_color =
      static_cast<uint32>((r << 24) | (g << 16) | (b << 8) | a);

  StaticImage* static_image =
      base::polymorphic_downcast<StaticImage*>(image_decoder.image().get());
  ASSERT_TRUE(static_image);
  render_tree::ImageStub* data =
      base::polymorphic_downcast<render_tree::ImageStub*>(
          static_image->image().get());

  math::Size size = data->GetSize();
  uint8* pixels = data->GetImageData()->GetMemory();

  EXPECT_TRUE(
      CheckSameColor(pixels, size.width(), size.height(), expected_color));
}

// Test that we can properly decode the PNG image with multiple chunks.
TEST(ImageDecoderTest, DecodePNGImageWithMultipleChunks) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(base::nullopt);

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("non_interlaced_png.png"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]), 4);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[4]), 2);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[6]), 94);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[100]), 100);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[200]),
                            image_data.size() - 200);
  image_decoder.Finish();

  // All pixels in the PNG image should have premultiplied alpha RGBA
  // values of (127, 0, 0, 127).
  uint8 r = 127;
  uint8 g = 0;
  uint8 b = 0;
  uint8 a = 127;
  uint32 expected_color =
      static_cast<uint32>((r << 24) | (g << 16) | (b << 8) | a);

  StaticImage* static_image =
      base::polymorphic_downcast<StaticImage*>(image_decoder.image().get());
  ASSERT_TRUE(static_image);
  render_tree::ImageStub* data =
      base::polymorphic_downcast<render_tree::ImageStub*>(
          static_image->image().get());

  math::Size size = data->GetSize();
  uint8* pixels = data->GetImageData()->GetMemory();

  EXPECT_TRUE(
      CheckSameColor(pixels, size.width(), size.height(), expected_color));
}

// Test that we can properly decode the the interlaced PNG.
TEST(ImageDecoderTest, DecodeInterlacedPNGImage) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(base::nullopt);

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("interlaced_png.png"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]),
                            image_data.size());
  image_decoder.Finish();

  // All pixels in the PNG image should have premultiplied alpha RGBA
  // values of (128, 88, 0, 255).
  uint8 r = 128;
  uint8 g = 88;
  uint8 b = 0;
  uint8 a = 255;
  uint32 expected_color =
      static_cast<uint32>((r << 24) | (g << 16) | (b << 8) | a);

  StaticImage* static_image =
      base::polymorphic_downcast<StaticImage*>(image_decoder.image().get());
  ASSERT_TRUE(static_image);
  render_tree::ImageStub* data =
      base::polymorphic_downcast<render_tree::ImageStub*>(
          static_image->image().get());

  math::Size size = data->GetSize();
  uint8* pixels = data->GetImageData()->GetMemory();

  EXPECT_TRUE(
      CheckSameColor(pixels, size.width(), size.height(), expected_color));
}

// Test that we can properly decode the interlaced PNG with multiple chunks.
TEST(ImageDecoderTest, DecodeInterlacedPNGImageWithMultipleChunks) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(base::nullopt);

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("interlaced_png.png"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]), 4);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[4]), 2);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[6]), 94);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[100]), 100);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[200]),
                            image_data.size() - 200);
  image_decoder.Finish();

  // All pixels in the PNG image should have premultiplied alpha RGBA
  // values of (128, 88, 0, 255).
  uint8 r = 128;
  uint8 g = 88;
  uint8 b = 0;
  uint8 a = 255;
  uint32 expected_color =
      static_cast<uint32>((r << 24) | (g << 16) | (b << 8) | a);

  StaticImage* static_image =
      base::polymorphic_downcast<StaticImage*>(image_decoder.image().get());
  ASSERT_TRUE(static_image);
  render_tree::ImageStub* data =
      base::polymorphic_downcast<render_tree::ImageStub*>(
          static_image->image().get());

  math::Size size = data->GetSize();
  uint8* pixels = data->GetImageData()->GetMemory();

  EXPECT_TRUE(
      CheckSameColor(pixels, size.width(), size.height(), expected_color));
}

// Test that we can properly decode the JPEG image.
TEST(ImageDecoderTest, DecodeJPEGImage) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(base::nullopt);

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("baseline_jpeg.jpg"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]),
                            image_data.size());
  image_decoder.Finish();

  StaticImage* static_image =
      base::polymorphic_downcast<StaticImage*>(image_decoder.image().get());
  ASSERT_TRUE(static_image);
  render_tree::ImageStub* data =
      base::polymorphic_downcast<render_tree::ImageStub*>(
          static_image->image().get());

  // All pixels in the JPEG image should have RGBA values of (128, 88, 0, 255),
  // or YUV values of (90, 77, 155).
  uint8 r = 128, g = 88, b = 0, a = 255;
  uint32 expected_color =
      static_cast<uint32>((r << 24) | (g << 16) | (b << 8) | a);
  EXPECT_TRUE(CheckUniformColoredImage(data, expected_color, 90, 77, 155));
}

// Test that we can properly decode the JPEG image with multiple chunks.
TEST(ImageDecoderTest, DecodeJPEGImageWithMultipleChunks) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(base::nullopt);

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("baseline_jpeg.jpg"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]), 2);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[2]), 4);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[6]), 94);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[100]), 200);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[300]),
                            image_data.size() - 300);
  image_decoder.Finish();

  StaticImage* static_image =
      base::polymorphic_downcast<StaticImage*>(image_decoder.image().get());
  ASSERT_TRUE(static_image);
  render_tree::ImageStub* data =
      base::polymorphic_downcast<render_tree::ImageStub*>(
          static_image->image().get());

  // All pixels in the JPEG image should have RGBA values of (128, 88, 0, 255),
  // or YUV values of (90, 77, 155).
  uint8 r = 128, g = 88, b = 0, a = 255;
  uint32 expected_color =
      static_cast<uint32>((r << 24) | (g << 16) | (b << 8) | a);
  EXPECT_TRUE(CheckUniformColoredImage(data, expected_color, 90, 77, 155));
}

// Test that we can properly decode the progressive JPEG image.
TEST(ImageDecoderTest, DecodeProgressiveJPEGImage) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(base::nullopt);

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("progressive_jpeg.jpg"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]),
                            image_data.size());
  image_decoder.Finish();

  StaticImage* static_image =
      base::polymorphic_downcast<StaticImage*>(image_decoder.image().get());
  ASSERT_TRUE(static_image);

  render_tree::ImageStub* data =
      base::polymorphic_downcast<render_tree::ImageStub*>(
          static_image->image().get());

  // All pixels in the JPEG image should have RGBA values of (64, 32, 17, 255),
  // or YUV values of (40, 115, 145).
  uint8 r = 64, g = 32, b = 17, a = 255;
  uint32 expected_rgba =
      static_cast<uint32>((r << 24) | (g << 16) | (b << 8) | a);
  EXPECT_TRUE(CheckUniformColoredImage(data, expected_rgba, 40, 115, 145));
}

// Test that we can properly decode the progressive JPEG with multiple chunks.
TEST(ImageDecoderTest, DecodeProgressiveJPEGImageWithMultipleChunks) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(base::nullopt);

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("progressive_jpeg.jpg"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]), 2);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[2]), 4);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[6]), 94);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[100]), 200);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[300]),
                            image_data.size() - 300);
  image_decoder.Finish();

  StaticImage* static_image =
      base::polymorphic_downcast<StaticImage*>(image_decoder.image().get());
  ASSERT_TRUE(static_image);
  render_tree::ImageStub* data =
      base::polymorphic_downcast<render_tree::ImageStub*>(
          static_image->image().get());

  // All pixels in the JPEG image should have RGBA values of (64, 32, 17, 255),
  // or YUV values of (40, 115, 145).
  uint8 r = 64, g = 32, b = 17, a = 255;
  uint32 expected_rgba =
      static_cast<uint32>((r << 24) | (g << 16) | (b << 8) | a);
  EXPECT_TRUE(CheckUniformColoredImage(data, expected_rgba, 40, 115, 145));
}

// Test that we can properly decode the progressive JPEG image while forcing the
// output to be single plane.
TEST(ImageDecoderTest, DecodeProgressiveJPEGImageToSinglePlane) {
  FakeResourceProviderStub resource_provider;
  base::NullDebuggerHooks debugger_hooks;
  const bool kAllowImageDecodingToMultiPlane = false;
  JPEGImageDecoder jpeg_image_decoder(&resource_provider, debugger_hooks,
                                      kAllowImageDecodingToMultiPlane);

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("progressive_jpeg.jpg"));
  jpeg_image_decoder.DecodeChunk(image_data.data(), image_data.size());
  auto image = jpeg_image_decoder.FinishAndMaybeReturnImage();

  StaticImage* static_image =
      base::polymorphic_downcast<StaticImage*>(image.get());
  ASSERT_TRUE(static_image);

  render_tree::ImageStub* data =
      base::polymorphic_downcast<render_tree::ImageStub*>(
          static_image->image().get());

  ASSERT_TRUE(!data->is_multi_plane_image());

  // All pixels in the JPEG image should have RGBA values of (64, 32, 17, 255),
  // or YUV values of (44, 115, 145).
  uint8 r = 64, g = 32, b = 17, a = 255;
  uint32 expected_rgba =
      static_cast<uint32>((r << 24) | (g << 16) | (b << 8) | a);
  EXPECT_TRUE(CheckUniformColoredImage(data, expected_rgba, 44, 115, 145));
}

// Test that we can properly decode the progressive JPEG with multiple chunks
// while forcing the output to be single plane.
TEST(ImageDecoderTest,
     DecodeProgressiveJPEGImageWithMultipleChunksToSinglePlane) {
  FakeResourceProviderStub resource_provider;
  base::NullDebuggerHooks debugger_hooks;
  const bool kAllowImageDecodingToMultiPlane = false;
  JPEGImageDecoder jpeg_image_decoder(&resource_provider, debugger_hooks,
                                      kAllowImageDecodingToMultiPlane);

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("progressive_jpeg.jpg"));
  jpeg_image_decoder.DecodeChunk(image_data.data(), 2);
  jpeg_image_decoder.DecodeChunk(image_data.data() + 2, 4);
  jpeg_image_decoder.DecodeChunk(image_data.data() + 6, 94);
  jpeg_image_decoder.DecodeChunk(image_data.data() + 100, 200);
  jpeg_image_decoder.DecodeChunk(image_data.data() + 300,
                                 image_data.size() - 300);
  auto image = jpeg_image_decoder.FinishAndMaybeReturnImage();

  StaticImage* static_image =
      base::polymorphic_downcast<StaticImage*>(image.get());
  ASSERT_TRUE(static_image);
  render_tree::ImageStub* data =
      base::polymorphic_downcast<render_tree::ImageStub*>(
          static_image->image().get());

  ASSERT_TRUE(!data->is_multi_plane_image());

  // All pixels in the JPEG image should have RGBA values of (64, 32, 17, 255),
  // or YUV values of (44, 115, 145).
  uint8 r = 64, g = 32, b = 17, a = 255;
  uint32 expected_rgba =
      static_cast<uint32>((r << 24) | (g << 16) | (b << 8) | a);
  EXPECT_TRUE(CheckUniformColoredImage(data, expected_rgba, 44, 115, 145));
}

// Test that we can properly decode the WEBP image.
TEST(ImageDecoderTest, DecodeWEBPImage) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(base::nullopt);

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("webp_image.webp"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]),
                            image_data.size());
  image_decoder.Finish();

  // All pixels in the WEBP image should have RGBA values of (16, 8, 70, 70).
  uint8 r = 16;
  uint8 g = 8;
  uint8 b = 70;
  uint8 a = 70;
  uint32 expected_color =
      static_cast<uint32>((r << 24) | (g << 16) | (b << 8) | a);

  StaticImage* static_image =
      base::polymorphic_downcast<StaticImage*>(image_decoder.image().get());
  ASSERT_TRUE(static_image);
  render_tree::ImageStub* data =
      base::polymorphic_downcast<render_tree::ImageStub*>(
          static_image->image().get());

  math::Size size = data->GetSize();
  uint8* pixels = data->GetImageData()->GetMemory();

  EXPECT_TRUE(
      CheckSameColor(pixels, size.width(), size.height(), expected_color));
}

// Test that we can properly decode the WEBP image with multiple chunks.
TEST(ImageDecoderTest, DecodeWEBPImageWithMultipleChunks) {
  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(base::nullopt);

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("webp_image.webp"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]), 2);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[2]), 4);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[6]), 94);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[100]),
                            image_data.size() - 100);
  image_decoder.Finish();

  // All pixels in the WEBP image should have RGBA values of (16, 8, 70, 70).
  uint8 r = 16;
  uint8 g = 8;
  uint8 b = 70;
  uint8 a = 70;
  uint32 expected_color =
      static_cast<uint32>((r << 24) | (g << 16) | (b << 8) | a);

  StaticImage* static_image =
      base::polymorphic_downcast<StaticImage*>(image_decoder.image().get());
  ASSERT_TRUE(static_image);
  render_tree::ImageStub* data =
      base::polymorphic_downcast<render_tree::ImageStub*>(
          static_image->image().get());

  math::Size size = data->GetSize();
  uint8* pixels = data->GetImageData()->GetMemory();

  EXPECT_TRUE(
      CheckSameColor(pixels, size.width(), size.height(), expected_color));
}

// Test that we can properly decode animated WEBP image.
TEST(ImageDecoderTest, DecodeAnimatedWEBPImage) {
  base::Thread thread("AnimatedWebP");
  thread.Start();

  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(base::nullopt);

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("vsauce_sm.webp"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]),
                            image_data.size());
  image_decoder.Finish();

  scoped_refptr<AnimatedWebPImage> animated_webp_image =
      base::polymorphic_downcast<AnimatedWebPImage*>(
          image_decoder.image().get());
  ASSERT_TRUE(animated_webp_image);

  animated_webp_image->Play(thread.task_runner());
  animated_webp_image->Stop();

  // The image should contain the whole undecoded data from the file.
  EXPECT_EQ(4261474u, animated_webp_image->GetEstimatedSizeInBytes());

  EXPECT_EQ(math::Size(480, 270), animated_webp_image->GetSize());
  EXPECT_TRUE(animated_webp_image->IsOpaque());
}

// Test that we can properly decode animated WEBP image in multiple chunks.
TEST(ImageDecoderTest, DecodeAnimatedWEBPImageWithMultipleChunks) {
  base::Thread thread("AnimatedWebP");
  thread.Start();

  std::unique_ptr<FakeResourceProviderStub> resource_provider(
      new FakeResourceProviderStub());
  MockImageDecoder image_decoder(resource_provider.get());
  image_decoder.ExpectCallWithError(base::nullopt);

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("vsauce_sm.webp"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]), 2);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[2]), 4);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[6]), 94);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[100]),
                            image_data.size() - 100);
  image_decoder.Finish();

  scoped_refptr<AnimatedWebPImage> animated_webp_image =
      base::polymorphic_downcast<AnimatedWebPImage*>(
          image_decoder.image().get());
  ASSERT_TRUE(animated_webp_image);

  animated_webp_image->Play(thread.task_runner());
  animated_webp_image->Stop();

  // The image should contain the whole undecoded data from the file.
  EXPECT_EQ(4261474u, animated_webp_image->GetEstimatedSizeInBytes());

  EXPECT_EQ(math::Size(480, 270), animated_webp_image->GetSize());
  EXPECT_TRUE(animated_webp_image->IsOpaque());
}
}  // namespace image
}  // namespace loader
}  // namespace cobalt
