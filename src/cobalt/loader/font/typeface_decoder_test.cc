// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/loader/font/typeface_decoder.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/render_tree/resource_provider_stub.h"
#include "cobalt/render_tree/typeface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace loader {
namespace font {

namespace {

const char kTtfTestTypeface[] = "icons.ttf";
const char kWoffTestTypeface[] = "icons.woff";
const char kWoff2TestTypeface[] = "icons.woff2";

struct MockTypefaceDecoderCallback {
  void SuccessCallback(const scoped_refptr<render_tree::Typeface>& value) {
    typeface = value;
  }

  MOCK_METHOD1(OnCompleteFunction,
               void(const base::Optional<std::string>& error));

  scoped_refptr<render_tree::Typeface> typeface;
};

class MockTypefaceDecoder : public Decoder {
 public:
  MockTypefaceDecoder();
  ~MockTypefaceDecoder() override {}

  void DecodeChunk(const char* data, size_t size) override;

  void Finish() override;
  bool Suspend() override;
  void Resume(render_tree::ResourceProvider* resource_provider) override;

  scoped_refptr<render_tree::Typeface> Typeface();

  void ExpectOnCompleteWithError(const base::Optional<std::string>& error);

 protected:
  ::testing::StrictMock<MockTypefaceDecoderCallback> typeface_decoder_callback_;
  render_tree::ResourceProviderStub resource_provider_;
  std::unique_ptr<Decoder> typeface_decoder_;
};

MockTypefaceDecoder::MockTypefaceDecoder() {
  typeface_decoder_ = TypefaceDecoder::Create(
      &resource_provider_,
      base::Bind(&MockTypefaceDecoderCallback::SuccessCallback,
                 base::Unretained(&typeface_decoder_callback_)),
      base::Bind(&MockTypefaceDecoderCallback::OnCompleteFunction,
                 base::Unretained(&typeface_decoder_callback_)));
}

void MockTypefaceDecoder::DecodeChunk(const char* data, size_t size) {
  typeface_decoder_->DecodeChunk(data, size);
}

void MockTypefaceDecoder::Finish() { typeface_decoder_->Finish(); }

bool MockTypefaceDecoder::Suspend() { return typeface_decoder_->Suspend(); }

void MockTypefaceDecoder::Resume(
    render_tree::ResourceProvider* resource_provider) {
  typeface_decoder_->Resume(resource_provider);
}

scoped_refptr<render_tree::Typeface> MockTypefaceDecoder::Typeface() {
  return typeface_decoder_callback_.typeface;
}

void MockTypefaceDecoder::ExpectOnCompleteWithError(
    const base::Optional<std::string>& error) {
  EXPECT_CALL(typeface_decoder_callback_, OnCompleteFunction(error));
}

base::FilePath GetTestTypefacePath(const char* file_name) {
  base::FilePath data_directory;
  CHECK(base::PathService::Get(base::DIR_TEST_DATA, &data_directory));
  return data_directory.Append(FILE_PATH_LITERAL("cobalt"))
      .Append(FILE_PATH_LITERAL("loader"))
      .Append(FILE_PATH_LITERAL("testdata"))
      .Append(FILE_PATH_LITERAL(file_name));
}

std::vector<uint8> GetTypefaceData(const base::FilePath& file_path) {
  int64 size;
  std::vector<uint8> typeface_data;

  bool success = base::GetFileSize(file_path, &size);

  CHECK(success) << "Could not get file size.";
  CHECK_GT(size, 0);

  typeface_data.resize(static_cast<size_t>(size));

  int num_of_bytes =
      base::ReadFile(file_path, reinterpret_cast<char*>(&typeface_data[0]),
                     static_cast<int>(size));

  CHECK_EQ(num_of_bytes, static_cast<int>(typeface_data.size()))
      << "Could not read '" << file_path.value() << "'.";
  return typeface_data;
}

}  // namespace

// Test that we can decode a ttf typeface received in one chunk.
TEST(TypefaceDecoderTest, DecodeTtfTypeface) {
  MockTypefaceDecoder typeface_decoder;
  typeface_decoder.ExpectOnCompleteWithError(base::nullopt);

  std::vector<uint8> typeface_data =
      GetTypefaceData(GetTestTypefacePath(kTtfTestTypeface));
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[0]),
                               typeface_data.size());
  typeface_decoder.Finish();

  EXPECT_TRUE(typeface_decoder.Typeface());
}

// Test that we can decode a ttf typeface received in multiple chunks.
TEST(TypefaceDecoderTest, DecodeTtfTypefaceWithMultipleChunks) {
  MockTypefaceDecoder typeface_decoder;
  typeface_decoder.ExpectOnCompleteWithError(base::nullopt);

  std::vector<uint8> typeface_data =
      GetTypefaceData(GetTestTypefacePath(kTtfTestTypeface));
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[0]), 4);
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[4]), 2);
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[6]), 94);
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[100]),
                               100);
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[200]),
                               typeface_data.size() - 200);
  typeface_decoder.Finish();

  EXPECT_TRUE(typeface_decoder.Typeface());
}

// Test that we can decode a woff typeface received in one chunk.
TEST(TypefaceDecoderTest, DecodeWoffTypeface) {
  MockTypefaceDecoder typeface_decoder;
  typeface_decoder.ExpectOnCompleteWithError(base::nullopt);

  std::vector<uint8> typeface_data =
      GetTypefaceData(GetTestTypefacePath(kWoffTestTypeface));
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[0]),
                               typeface_data.size());
  typeface_decoder.Finish();

  EXPECT_TRUE(typeface_decoder.Typeface());
}

// Test that we can decode a woff typeface received in multiple chunks.
TEST(TypefaceDecoderTest, DecodeWoffTypefaceWithMultipleChunks) {
  MockTypefaceDecoder typeface_decoder;
  typeface_decoder.ExpectOnCompleteWithError(base::nullopt);

  std::vector<uint8> typeface_data =
      GetTypefaceData(GetTestTypefacePath(kWoffTestTypeface));
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[0]), 4);
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[4]), 2);
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[6]), 94);
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[100]),
                               100);
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[200]),
                               typeface_data.size() - 200);
  typeface_decoder.Finish();

  EXPECT_TRUE(typeface_decoder.Typeface());
}

// Test that we can decode a woff2 typeface received in one chunk.
TEST(TypefaceDecoderTest, DecodeWoff2Typeface) {
  MockTypefaceDecoder typeface_decoder;
  typeface_decoder.ExpectOnCompleteWithError(base::nullopt);

  std::vector<uint8> typeface_data =
      GetTypefaceData(GetTestTypefacePath(kWoff2TestTypeface));
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[0]),
                               typeface_data.size());
  typeface_decoder.Finish();

  EXPECT_TRUE(typeface_decoder.Typeface());
}

// Test that we can decode a woff2 typeface received in multiple chunks.
TEST(TypefaceDecoderTest, DecodeWoff2TypefaceWithMultipleChunks) {
  MockTypefaceDecoder typeface_decoder;
  typeface_decoder.ExpectOnCompleteWithError(base::nullopt);

  std::vector<uint8> typeface_data =
      GetTypefaceData(GetTestTypefacePath(kWoff2TestTypeface));
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[0]), 4);
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[4]), 2);
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[6]), 94);
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[100]),
                               100);
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[200]),
                               typeface_data.size() - 200);
  typeface_decoder.Finish();

  EXPECT_TRUE(typeface_decoder.Typeface());
}

// Test that decoding a too large typeface is handled gracefully and does not
// crash
// or return a typeface.
TEST(TypefaceDecoderTest, DecodeTooLargeTypeface) {
  MockTypefaceDecoder typeface_decoder;
  typeface_decoder.ExpectOnCompleteWithError(
      std::string("Raw typeface data size too large"));

  std::vector<uint8> typeface_data =
      GetTypefaceData(GetTestTypefacePath(kTtfTestTypeface));
  typeface_decoder.DecodeChunk(
      reinterpret_cast<char*>(&typeface_data[0]),
      render_tree::ResourceProvider::kMaxTypefaceDataSize + 10);
  typeface_decoder.Finish();

  EXPECT_FALSE(typeface_decoder.Typeface());
}

// Test that decoding a too large typeface received in multiple chunks that
// is handled gracefully and does not crash or return a typeface.
TEST(TypefaceDecoderTest, DecodeTooLargeTypefaceWithMultipleChunks) {
  MockTypefaceDecoder typeface_decoder;
  typeface_decoder.ExpectOnCompleteWithError(
      std::string("Raw typeface data size too large"));

  std::vector<uint8> typeface_data =
      GetTypefaceData(GetTestTypefacePath(kTtfTestTypeface));
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[0]), 4);
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[4]), 2);
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[6]), 94);
  typeface_decoder.DecodeChunk(
      reinterpret_cast<char*>(&typeface_data[100]),
      render_tree::ResourceProvider::kMaxTypefaceDataSize);
  typeface_decoder.DecodeChunk(reinterpret_cast<char*>(&typeface_data[200]),
                               100);
  typeface_decoder.Finish();

  EXPECT_FALSE(typeface_decoder.Typeface());
}

}  // namespace font
}  // namespace loader
}  // namespace cobalt
