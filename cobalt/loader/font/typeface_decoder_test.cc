/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/loader/font/typeface_decoder.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/render_tree/resource_provider_stub.h"
#include "cobalt/render_tree/typeface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace loader {
namespace font {

namespace {

const char kTtfTestTypeface[] = "icons.ttf";
const char kWoffTestTypeface[] = "icons.woff";

struct TypefaceDecoderCallback {
  void SuccessCallback(const scoped_refptr<render_tree::Typeface>& value) {
    typeface = value;
  }

  void ErrorCallback(const std::string& error_message) {
    DLOG(WARNING) << error_message;
  }

  scoped_refptr<render_tree::Typeface> typeface;
};

FilePath GetTestTypefacePath(const char* file_name) {
  FilePath data_directory;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &data_directory));
  return data_directory.Append(FILE_PATH_LITERAL("cobalt"))
      .Append(FILE_PATH_LITERAL("loader"))
      .Append(FILE_PATH_LITERAL("testdata"))
      .Append(FILE_PATH_LITERAL(file_name));
}

std::vector<uint8> GetTypefaceData(const FilePath& file_path) {
  int64 size;
  std::vector<uint8> typeface_data;

  bool success = file_util::GetFileSize(file_path, &size);

  CHECK(success) << "Could not get file size.";
  CHECK_GT(size, 0);

  typeface_data.resize(static_cast<size_t>(size));

  int num_of_bytes =
      file_util::ReadFile(file_path, reinterpret_cast<char*>(&typeface_data[0]),
                          static_cast<int>(size));

  CHECK_EQ(num_of_bytes, typeface_data.size()) << "Could not read '"
                                               << file_path.value() << "'.";
  return typeface_data;
}

}  // namespace

// Test that we can decode a ttf typeface received in one chunk.
TEST(TypefaceDecoderTest, DecodeTtfTypeface) {
  render_tree::ResourceProviderStub resource_provider;

  TypefaceDecoderCallback typeface_decoder_result;
  scoped_ptr<Decoder> typeface_decoder(new TypefaceDecoder(
      &resource_provider,
      base::Bind(&TypefaceDecoderCallback::SuccessCallback,
                 base::Unretained(&typeface_decoder_result)),
      base::Bind(&TypefaceDecoderCallback::ErrorCallback,
                 base::Unretained(&typeface_decoder_result))));

  std::vector<uint8> typeface_data =
      GetTypefaceData(GetTestTypefacePath(kTtfTestTypeface));
  typeface_decoder->DecodeChunk(reinterpret_cast<char*>(&typeface_data[0]),
                                typeface_data.size());
  typeface_decoder->Finish();

  EXPECT_TRUE(typeface_decoder_result.typeface != NULL);
}

// Test that we can decode a ttf typeface received in multiple chunks.
TEST(TypefaceDecoderTest, DecodeTtfTypefaceWithMultipleChunks) {
  render_tree::ResourceProviderStub resource_provider;

  TypefaceDecoderCallback typeface_decoder_result;
  scoped_ptr<Decoder> typeface_decoder(new TypefaceDecoder(
      &resource_provider,
      base::Bind(&TypefaceDecoderCallback::SuccessCallback,
                 base::Unretained(&typeface_decoder_result)),
      base::Bind(&TypefaceDecoderCallback::ErrorCallback,
                 base::Unretained(&typeface_decoder_result))));

  std::vector<uint8> typeface_data =
      GetTypefaceData(GetTestTypefacePath(kTtfTestTypeface));
  typeface_decoder->DecodeChunk(reinterpret_cast<char*>(&typeface_data[0]), 4);
  typeface_decoder->DecodeChunk(reinterpret_cast<char*>(&typeface_data[4]), 2);
  typeface_decoder->DecodeChunk(reinterpret_cast<char*>(&typeface_data[6]), 94);
  typeface_decoder->DecodeChunk(reinterpret_cast<char*>(&typeface_data[100]),
                                100);
  typeface_decoder->DecodeChunk(reinterpret_cast<char*>(&typeface_data[200]),
                                typeface_data.size() - 200);
  typeface_decoder->Finish();

  EXPECT_TRUE(typeface_decoder_result.typeface != NULL);
}

// Test that we can decode a woff typeface received in one chunk.
TEST(TypefaceDecoderTest, DecodeWoffTypeface) {
  render_tree::ResourceProviderStub resource_provider;

  TypefaceDecoderCallback typeface_decoder_result;
  scoped_ptr<Decoder> typeface_decoder(new TypefaceDecoder(
      &resource_provider,
      base::Bind(&TypefaceDecoderCallback::SuccessCallback,
                 base::Unretained(&typeface_decoder_result)),
      base::Bind(&TypefaceDecoderCallback::ErrorCallback,
                 base::Unretained(&typeface_decoder_result))));

  std::vector<uint8> typeface_data =
      GetTypefaceData(GetTestTypefacePath(kWoffTestTypeface));
  typeface_decoder->DecodeChunk(reinterpret_cast<char*>(&typeface_data[0]),
                                typeface_data.size());
  typeface_decoder->Finish();

  EXPECT_TRUE(typeface_decoder_result.typeface != NULL);
}

// Test that we can decode a woff typeface received in multiple chunks.
TEST(TypefaceDecoderTest, DecodeWoffTypefaceWithMultipleChunks) {
  render_tree::ResourceProviderStub resource_provider;

  TypefaceDecoderCallback typeface_decoder_result;
  scoped_ptr<Decoder> typeface_decoder(new TypefaceDecoder(
      &resource_provider,
      base::Bind(&TypefaceDecoderCallback::SuccessCallback,
                 base::Unretained(&typeface_decoder_result)),
      base::Bind(&TypefaceDecoderCallback::ErrorCallback,
                 base::Unretained(&typeface_decoder_result))));

  std::vector<uint8> typeface_data =
      GetTypefaceData(GetTestTypefacePath(kWoffTestTypeface));
  typeface_decoder->DecodeChunk(reinterpret_cast<char*>(&typeface_data[0]), 4);
  typeface_decoder->DecodeChunk(reinterpret_cast<char*>(&typeface_data[4]), 2);
  typeface_decoder->DecodeChunk(reinterpret_cast<char*>(&typeface_data[6]), 94);
  typeface_decoder->DecodeChunk(reinterpret_cast<char*>(&typeface_data[100]),
                                100);
  typeface_decoder->DecodeChunk(reinterpret_cast<char*>(&typeface_data[200]),
                                typeface_data.size() - 200);
  typeface_decoder->Finish();

  EXPECT_TRUE(typeface_decoder_result.typeface != NULL);
}

// Test that decoding a too large typeface is handled gracefully and does not
// crash
// or return a typeface.
TEST(TypefaceDecoderTest, DecodeTooLargeTypeface) {
  render_tree::ResourceProviderStub resource_provider;

  TypefaceDecoderCallback typeface_decoder_result;
  scoped_ptr<Decoder> typeface_decoder(new TypefaceDecoder(
      &resource_provider,
      base::Bind(&TypefaceDecoderCallback::SuccessCallback,
                 base::Unretained(&typeface_decoder_result)),
      base::Bind(&TypefaceDecoderCallback::ErrorCallback,
                 base::Unretained(&typeface_decoder_result))));

  std::vector<uint8> typeface_data =
      GetTypefaceData(GetTestTypefacePath(kTtfTestTypeface));
  typeface_decoder->DecodeChunk(
      reinterpret_cast<char*>(&typeface_data[0]),
      render_tree::ResourceProvider::kMaxTypefaceDataSize + 10);
  typeface_decoder->Finish();

  EXPECT_TRUE(typeface_decoder_result.typeface == NULL);
}

// Test that decoding a too large typeface received in multiple chunks that
// is handled gracefully and does not crash or return a typeface.
TEST(TypefaceDecoderTest, DecodeTooLargeTypefaceWithMultipleChunks) {
  render_tree::ResourceProviderStub resource_provider;

  TypefaceDecoderCallback typeface_decoder_result;
  scoped_ptr<Decoder> typeface_decoder(new TypefaceDecoder(
      &resource_provider,
      base::Bind(&TypefaceDecoderCallback::SuccessCallback,
                 base::Unretained(&typeface_decoder_result)),
      base::Bind(&TypefaceDecoderCallback::ErrorCallback,
                 base::Unretained(&typeface_decoder_result))));

  std::vector<uint8> typeface_data =
      GetTypefaceData(GetTestTypefacePath(kTtfTestTypeface));
  typeface_decoder->DecodeChunk(reinterpret_cast<char*>(&typeface_data[0]), 4);
  typeface_decoder->DecodeChunk(reinterpret_cast<char*>(&typeface_data[4]), 2);
  typeface_decoder->DecodeChunk(reinterpret_cast<char*>(&typeface_data[6]), 94);
  typeface_decoder->DecodeChunk(
      reinterpret_cast<char*>(&typeface_data[100]),
      render_tree::ResourceProvider::kMaxTypefaceDataSize);
  typeface_decoder->DecodeChunk(reinterpret_cast<char*>(&typeface_data[200]),
                                100);
  typeface_decoder->Finish();

  EXPECT_TRUE(typeface_decoder_result.typeface == NULL);
}

}  // namespace font
}  // namespace loader
}  // namespace cobalt
