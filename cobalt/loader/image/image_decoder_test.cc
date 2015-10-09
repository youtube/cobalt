/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/loader/image/image_decoder.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/render_tree/resource_provider_stub.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace loader {
namespace image {

namespace {

struct ImageDecoderCallback {
  void SuccessCallback(const scoped_refptr<render_tree::Image>& value) {
    image = value;
  }

  void ErrorCallback(const std::string& error_message) {
    DLOG(WARNING) << error_message;
  }

  scoped_refptr<render_tree::Image> image;
};

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

// TODO(***REMOVED***): Test special images like the image has gAMA chunk information,
// interlaced pngs, pngs with 16 bit depth, and large pngs.
// TODO(***REMOVED***): Add layout tests for image decoder.

// Test that we can properly decode the PNG image.
TEST(ImageDecoderTest, DecodePNGImage) {
  render_tree::ResourceProviderStub resource_provider;

  ImageDecoderCallback image_decoder_result;
  scoped_ptr<Decoder> image_decoder(new ImageDecoder(
      &resource_provider, base::Bind(&ImageDecoderCallback::SuccessCallback,
                                     base::Unretained(&image_decoder_result)),
      base::Bind(&ImageDecoderCallback::ErrorCallback,
                 base::Unretained(&image_decoder_result))));

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("image_decoder_test_png_image.png"));
  image_decoder->DecodeChunk(reinterpret_cast<char*>(&image_data[0]),
                             image_data.size());
  image_decoder->Finish();

  // All pixels in the PNG image should have premultiplied alpha RGBA
  // values of (127, 0, 0, 127).
  uint8 r = 127;
  uint8 g = 0;
  uint8 b = 0;
  uint8 a = 127;
  uint32 expected_color =
      static_cast<uint32>((r << 24) | (g << 16) | (b << 8) | a);

  render_tree::ImageStub* data =
      base::polymorphic_downcast<render_tree::ImageStub*>(
          image_decoder_result.image.get());

  math::Size size = data->GetSize();
  uint8* pixels = data->GetImageData()->GetMemory();

  EXPECT_TRUE(
      CheckSameColor(pixels, size.width(), size.height(), expected_color));
}

// Test that we can properly decode the PNG image with multiple chunks.
TEST(ImageDecoderTest, DecodePNGImageWithMultipleChunks) {
  render_tree::ResourceProviderStub resource_provider;

  ImageDecoderCallback image_decoder_result;
  scoped_ptr<Decoder> image_decoder(new ImageDecoder(
      &resource_provider, base::Bind(&ImageDecoderCallback::SuccessCallback,
                                     base::Unretained(&image_decoder_result)),
      base::Bind(&ImageDecoderCallback::ErrorCallback,
                 base::Unretained(&image_decoder_result))));

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("image_decoder_test_png_image.png"));
  image_decoder->DecodeChunk(reinterpret_cast<char*>(&image_data[0]), 4);
  image_decoder->DecodeChunk(reinterpret_cast<char*>(&image_data[4]), 2);
  image_decoder->DecodeChunk(reinterpret_cast<char*>(&image_data[6]), 94);
  image_decoder->DecodeChunk(reinterpret_cast<char*>(&image_data[100]), 100);
  image_decoder->DecodeChunk(reinterpret_cast<char*>(&image_data[200]),
                             image_data.size() - 200);
  image_decoder->Finish();

  // All pixels in the PNG image should have premultiplied alpha RGBA
  // values of (127, 0, 0, 127).
  uint8 r = 127;
  uint8 g = 0;
  uint8 b = 0;
  uint8 a = 127;
  uint32 expected_color =
      static_cast<uint32>((r << 24) | (g << 16) | (b << 8) | a);

  render_tree::ImageStub* data =
      base::polymorphic_downcast<render_tree::ImageStub*>(
          image_decoder_result.image.get());

  math::Size size = data->GetSize();
  uint8* pixels = data->GetImageData()->GetMemory();

  EXPECT_TRUE(
      CheckSameColor(pixels, size.width(), size.height(), expected_color));
}

// Test that we can properly decode the JPEG image.
TEST(ImageDecoderTest, DecodeJPEGImage) {
  render_tree::ResourceProviderStub resource_provider;

  ImageDecoderCallback image_decoder_result;
  scoped_ptr<Decoder> image_decoder(new ImageDecoder(
      &resource_provider, base::Bind(&ImageDecoderCallback::SuccessCallback,
                                     base::Unretained(&image_decoder_result)),
      base::Bind(&ImageDecoderCallback::ErrorCallback,
                 base::Unretained(&image_decoder_result))));

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("image_decoder_test_jpeg_image.jpg"));
  image_decoder->DecodeChunk(reinterpret_cast<char*>(&image_data[0]),
                             image_data.size());
  image_decoder->Finish();

  // All pixels in the JPEG image should have RGBA values of (128, 88, 0, 255).
  uint8 r = 128;
  uint8 g = 88;
  uint8 b = 0;
  uint8 a = 255;
  uint32 expected_color =
      static_cast<uint32>((r << 24) | (g << 16) | (b << 8) | a);

  render_tree::ImageStub* data =
      base::polymorphic_downcast<render_tree::ImageStub*>(
          image_decoder_result.image.get());

  math::Size size = data->GetSize();
  uint8* pixels = data->GetImageData()->GetMemory();

  EXPECT_TRUE(
      CheckSameColor(pixels, size.width(), size.height(), expected_color));
}

// Test that we can properly decode the JPEG image with multiple chunks.
TEST(ImageDecoderTest, DecodeJPEGImageWithMultipleChunks) {
  render_tree::ResourceProviderStub resource_provider;

  ImageDecoderCallback image_decoder_result;
  scoped_ptr<Decoder> image_decoder(new ImageDecoder(
      &resource_provider, base::Bind(&ImageDecoderCallback::SuccessCallback,
                                     base::Unretained(&image_decoder_result)),
      base::Bind(&ImageDecoderCallback::ErrorCallback,
                 base::Unretained(&image_decoder_result))));

  std::vector<uint8> image_data =
      GetImageData(GetTestImagePath("image_decoder_test_jpeg_image.jpg"));
  image_decoder->DecodeChunk(reinterpret_cast<char*>(&image_data[0]), 2);
  image_decoder->DecodeChunk(reinterpret_cast<char*>(&image_data[2]), 4);
  image_decoder->DecodeChunk(reinterpret_cast<char*>(&image_data[6]), 94);
  image_decoder->DecodeChunk(reinterpret_cast<char*>(&image_data[100]), 200);
  image_decoder->DecodeChunk(reinterpret_cast<char*>(&image_data[300]),
                             image_data.size() - 300);
  image_decoder->Finish();

  // All pixels in the JPEG image should have RGBA values of (128, 88, 0, 255).
  uint8 r = 128;
  uint8 g = 88;
  uint8 b = 0;
  uint8 a = 255;
  uint32 expected_color =
      static_cast<uint32>((r << 24) | (g << 16) | (b << 8) | a);

  render_tree::ImageStub* data =
      base::polymorphic_downcast<render_tree::ImageStub*>(
          image_decoder_result.image.get());

  math::Size size = data->GetSize();
  uint8* pixels = data->GetImageData()->GetMemory();

  EXPECT_TRUE(
      CheckSameColor(pixels, size.width(), size.height(), expected_color));
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
