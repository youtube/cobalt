// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/loader/image/image_decoder.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/loader/image/animated_webp_image.h"
#include "cobalt/render_tree/resource_provider_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace loader {
namespace image {

namespace {

struct MockImageDecoderCallback {
  void SuccessCallback(const scoped_refptr<Image>& value) { image = value; }

  MOCK_METHOD1(ErrorCallback, void(const std::string& message));

  scoped_refptr<Image> image;
};

class MockImageDecoder : public Decoder {
 public:
  MockImageDecoder();
  ~MockImageDecoder() override {}

  LoadResponseType OnResponseStarted(
      Fetcher* fetcher,
      const scoped_refptr<net::HttpResponseHeaders>& headers) override;

  void DecodeChunk(const char* data, size_t size) override;

  void Finish() override;
  bool Suspend() override;
  void Resume(render_tree::ResourceProvider* resource_provider) override;

  scoped_refptr<Image> image();

  void ExpectCallWithError(const std::string& message);

 protected:
  ::testing::StrictMock<MockImageDecoderCallback> image_decoder_callback_;
  render_tree::ResourceProviderStub resource_provider_;
  scoped_ptr<Decoder> image_decoder_;
};

MockImageDecoder::MockImageDecoder() {
  image_decoder_.reset(
      new ImageDecoder(&resource_provider_,
                       base::Bind(&MockImageDecoderCallback::SuccessCallback,
                                  base::Unretained(&image_decoder_callback_)),
                       base::Bind(&MockImageDecoderCallback::ErrorCallback,
                                  base::Unretained(&image_decoder_callback_))));
}

LoadResponseType MockImageDecoder::OnResponseStarted(
    Fetcher* fetcher, const scoped_refptr<net::HttpResponseHeaders>& headers) {
  return image_decoder_->OnResponseStarted(fetcher, headers);
}

void MockImageDecoder::DecodeChunk(const char* data, size_t size) {
  image_decoder_->DecodeChunk(data, size);
}

void MockImageDecoder::Finish() { image_decoder_->Finish(); }

bool MockImageDecoder::Suspend() { return image_decoder_->Suspend(); }

void MockImageDecoder::Resume(
    render_tree::ResourceProvider* resource_provider) {
  image_decoder_->Resume(resource_provider);
}

scoped_refptr<Image> MockImageDecoder::image() {
  return image_decoder_callback_.image;
}

void MockImageDecoder::ExpectCallWithError(const std::string& message) {
  EXPECT_CALL(image_decoder_callback_, ErrorCallback(message));
}

FilePath GetTestImagePath(const char* file_name) {
  FilePath data_directory;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &data_directory));
  return data_directory.Append(FILE_PATH_LITERAL("cobalt"))
      .Append(FILE_PATH_LITERAL("loader"))
      .Append(FILE_PATH_LITERAL("testdata"))
      .Append(FILE_PATH_LITERAL(file_name));
}

std::vector<uint8> GetImageData(const FilePath& file_path) {
  int64 size;
  std::vector<uint8> image_data;

  bool success = file_util::GetFileSize(file_path, &size);

  CHECK(success) << "Could not get file size.";
  CHECK_GT(size, 0);

  image_data.resize(static_cast<size_t>(size));

  int num_of_bytes =
      file_util::ReadFile(file_path, reinterpret_cast<char*>(&image_data[0]),
                          static_cast<int>(size));

  CHECK_EQ(num_of_bytes, image_data.size()) << "Could not read '"
                                            << file_path.value() << "'.";
  return image_data;
}

// Check if pixels are the same as |test_color|.
bool CheckSameColor(const uint8* pixels, int width, int height,
                    uint32 test_color) {
  // Iterate through each pixel testing that the value is the expected value.
  for (int index = 0; index < width * height; ++index) {
    uint32 current_color = static_cast<uint32>(
        (pixels[0] << 24) | (pixels[1] << 16) | (pixels[2] << 8) | pixels[3]);
    pixels += 4;
    if (current_color != test_color) {
      return false;
    }
  }
  return true;
}

}  // namespace

// TODO: Test special images like the image has gAMA chunk information,
// pngs with 16 bit depth, and large pngs.

TEST(ImageDecoderTest, DecodeImageWithContentLength0) {
  MockImageDecoder image_decoder;
  image_decoder.ExpectCallWithError(
      "No content returned, but expected some.");

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
  MockImageDecoder image_decoder;
  image_decoder.ExpectCallWithError(
      "No content returned, but expected some. Not an image mime type.");

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
  MockImageDecoder image_decoder;
  image_decoder.ExpectCallWithError("Not an image mime type.");

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
  MockImageDecoder image_decoder;
  image_decoder.ExpectCallWithError("Not an image mime type.");

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
  MockImageDecoder image_decoder;
  image_decoder.ExpectCallWithError(
      "No content returned. Not an image mime type.");

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
  MockImageDecoder image_decoder;
  image_decoder.ExpectCallWithError("No enough image data for header.");

  const char kPartialWebPHeader[] = {"RIFF"};
  image_decoder.DecodeChunk(kPartialWebPHeader, sizeof(kPartialWebPHeader));
  image_decoder.Finish();

  EXPECT_FALSE(image_decoder.image());
}

TEST(ImageDecoderTest, FailedToDecodeImage) {
  MockImageDecoder image_decoder;
  image_decoder.ExpectCallWithError("PNGImageDecoder failed to decode image.");

  const char kPartialPNGImage[] = {
      "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A\x00\x00\x00\x0D\x49\x48\x44\x52\x00\x00"
      "\x02\x6C\x00\x00\x01\x2C"};
  image_decoder.DecodeChunk(kPartialPNGImage, sizeof(kPartialPNGImage));
  image_decoder.Finish();

  EXPECT_FALSE(image_decoder.image());
}

TEST(ImageDecoderTest, UnsupportedImageFormat) {
  MockImageDecoder image_decoder;
  image_decoder.ExpectCallWithError("Unsupported image format.");

  const char kPartialICOImage[] = {
      "\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x01\x00"};
  image_decoder.DecodeChunk(kPartialICOImage, sizeof(kPartialICOImage));
  image_decoder.Finish();

  EXPECT_FALSE(image_decoder.image());
}

// Test that we can properly decode the PNG image.
TEST(ImageDecoderTest, DecodePNGImage) {
  MockImageDecoder image_decoder;

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
  MockImageDecoder image_decoder;

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
  MockImageDecoder image_decoder;

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
  MockImageDecoder image_decoder;

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
  MockImageDecoder image_decoder;

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("baseline_jpeg.jpg"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]),
                            image_data.size());
  image_decoder.Finish();

  // All pixels in the JPEG image should have RGBA values of (128, 88, 0, 255).
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

// Test that we can properly decode the JPEG image with multiple chunks.
TEST(ImageDecoderTest, DecodeJPEGImageWithMultipleChunks) {
  MockImageDecoder image_decoder;

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("baseline_jpeg.jpg"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]), 2);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[2]), 4);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[6]), 94);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[100]), 200);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[300]),
                            image_data.size() - 300);
  image_decoder.Finish();

  // All pixels in the JPEG image should have RGBA values of (128, 88, 0, 255).
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

// Test that we can properly decode the progressive JPEG image.
TEST(ImageDecoderTest, DecodeProgressiveJPEGImage) {
  MockImageDecoder image_decoder;

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("progressive_jpeg.jpg"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]),
                            image_data.size());
  image_decoder.Finish();

  // All pixels in the JPEG image should have RGBA values of (64, 32, 17, 255).
  uint8 r = 64;
  uint8 g = 32;
  uint8 b = 17;
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

// Test that we can properly decode the progressive JPEG with multiple chunks.
TEST(ImageDecoderTest, DecodeProgressiveJPEGImageWithMultipleChunks) {
  MockImageDecoder image_decoder;

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("progressive_jpeg.jpg"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]), 2);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[2]), 4);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[6]), 94);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[100]), 200);
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[300]),
                            image_data.size() - 300);
  image_decoder.Finish();

  // All pixels in the JPEG image should have RGBA values of (64, 32, 17, 255).
  uint8 r = 64;
  uint8 g = 32;
  uint8 b = 17;
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

// Test that we can properly decode the WEBP image.
TEST(ImageDecoderTest, DecodeWEBPImage) {
  MockImageDecoder image_decoder;

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
  MockImageDecoder image_decoder;

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
  MockImageDecoder image_decoder;

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("vsauce_sm.webp"));
  image_decoder.DecodeChunk(reinterpret_cast<char*>(&image_data[0]),
                            image_data.size());
  image_decoder.Finish();

  scoped_refptr<AnimatedWebPImage> animated_webp_image =
      base::polymorphic_downcast<AnimatedWebPImage*>(
          image_decoder.image().get());
  ASSERT_TRUE(animated_webp_image);

  base::Thread thread("AnimatedWebP test");
  thread.Start();
  animated_webp_image->Play(thread.message_loop_proxy());
  animated_webp_image->Stop();
  thread.Stop();

  // The image should contain the whole undecoded data from the file.
  EXPECT_EQ(4261474u, animated_webp_image->GetEstimatedSizeInBytes());

  EXPECT_EQ(math::Size(480, 270), animated_webp_image->GetSize());
  EXPECT_TRUE(animated_webp_image->IsOpaque());
}

// Test that we can properly decode animated WEBP image in multiple chunks.
TEST(ImageDecoderTest, DecodeAnimatedWEBPImageWithMultipleChunks) {
  MockImageDecoder image_decoder;

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

  base::Thread thread("AnimatedWebP test");
  thread.Start();
  animated_webp_image->Play(thread.message_loop_proxy());
  animated_webp_image->Stop();
  thread.Stop();

  // The image should contain the whole undecoded data from the file.
  EXPECT_EQ(4261474u, animated_webp_image->GetEstimatedSizeInBytes());

  EXPECT_EQ(math::Size(480, 270), animated_webp_image->GetSize());
  EXPECT_TRUE(animated_webp_image->IsOpaque());
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
