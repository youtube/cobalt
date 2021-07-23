// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace loader {
namespace image {

TEST(ImageDecoderUnitTest, ValidImageType) {
  EXPECT_EQ(DetermineImageType(reinterpret_cast<const uint8*>("\xFF\xD8\xFF")),
            ImageDecoder::kImageTypeJPEG);
  EXPECT_EQ(DetermineImageType(reinterpret_cast<const uint8*>("GIF87a")),
            ImageDecoder::kImageTypeGIF);
  EXPECT_EQ(DetermineImageType(reinterpret_cast<const uint8*>("GIF89a")),
            ImageDecoder::kImageTypeGIF);
  EXPECT_EQ(DetermineImageType(reinterpret_cast<const uint8*>(
                "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A")),
            ImageDecoder::kImageTypePNG);
  EXPECT_EQ(
      DetermineImageType(reinterpret_cast<const uint8*>("RIFF    WEBPVP")),
      ImageDecoder::kImageTypeWebP);
  EXPECT_EQ(
      DetermineImageType(reinterpret_cast<const uint8*>("{\"v\":\"4.6.8\"")),
      ImageDecoder::kImageTypeJSON);
  EXPECT_EQ(DetermineImageType(
                reinterpret_cast<const uint8*>("  {\t\"v\":\"4.6.8\"")),
            ImageDecoder::kImageTypeJSON);
  EXPECT_EQ(DetermineImageType(
                reinterpret_cast<const uint8*>("\r [ \"v\":\"4.6.8\"")),
            ImageDecoder::kImageTypeJSON);
}

TEST(ImageDecoderUnitTest, InvalidImageType) {
  EXPECT_EQ(DetermineImageType(reinterpret_cast<const uint8*>(
                "\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x01"
                "\x00")),
            ImageDecoder::kImageTypeInvalid);
  EXPECT_EQ(
      DetermineImageType(reinterpret_cast<const uint8*>("{}\0\0\0\0\0\0")),
      ImageDecoder::kImageTypeInvalid);
  EXPECT_EQ(
      DetermineImageType(reinterpret_cast<const uint8*>("{a\0\0\0\0\0\0")),
      ImageDecoder::kImageTypeInvalid);
  EXPECT_EQ(
      DetermineImageType(reinterpret_cast<const uint8*>("{\"\"\0\0\0\0\0")),
      ImageDecoder::kImageTypeInvalid);
  EXPECT_EQ(
      DetermineImageType(reinterpret_cast<const uint8*>("{\"{\0\0\0\0\0")),
      ImageDecoder::kImageTypeInvalid);
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
