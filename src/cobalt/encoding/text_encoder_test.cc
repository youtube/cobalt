
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

#include "cobalt/encoding/text_encoder.h"

#include <string>
#include <utility>
#include <vector>

#include "cobalt/dom/testing/stub_window.h"
#include "cobalt/script/array_buffer_view.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/typed_arrays.h"

#include "testing/gtest/include/gtest/gtest.h"


namespace cobalt {
namespace encoding {
namespace {

//////////////////////////////////////////////////////////////////////////
// TextEncoderTest
//////////////////////////////////////////////////////////////////////////

class TextEncoderTest : public ::testing::Test {
 protected:
  TextEncoderTest();
  ~TextEncoderTest();

  cobalt::dom::testing::StubWindow stub_window_;
  scoped_refptr<TextEncoder> text_encoder_;
};

TextEncoderTest::TextEncoderTest() : text_encoder_(new TextEncoder()) {}

TextEncoderTest::~TextEncoderTest() {}

//////////////////////////////////////////////////////////////////////////
// Test cases
//////////////////////////////////////////////////////////////////////////

TEST_F(TextEncoderTest, Constructor) {
  EXPECT_EQ("utf-8", text_encoder_->encoding());
}

TEST_F(TextEncoderTest, Encode) {
  std::vector<std::pair<std::string, std::vector<uint8>>> tests = {
      {"Hello world!",
       {72, 101, 108, 108, 111, 32, 119, 111, 114, 108, 100, 33}},
      {"Hej! Привет! 你好！",
       {72,  101, 106, 33, 32, 208, 159, 209, 128, 208, 184, 208, 178, 208,
        181, 209, 130, 33, 32, 228, 189, 160, 229, 165, 189, 239, 188, 129}},
      {"Да!", {208, 148, 208, 176, 041}},
  };

  std::string input;
  std::vector<uint8> want;
  for (const auto &test : tests) {
    std::tie(input, want) = test;
    script::Handle<script::Uint8Array> got =
        text_encoder_->Encode(stub_window_.environment_settings(), input);
    auto *array_got = static_cast<uint8 *>(got->RawData());

    // Compare the result against the expectations.
    ASSERT_EQ(got->Length(), want.size());
    for (uint32 i = 0; i < got->Length(); ++i) {
      EXPECT_EQ(array_got[i], want[i]);
    }
  }
}

TEST_F(TextEncoderTest, EncodeInto) {
  struct Result {
    std::vector<uint8> bytes;
    uint64_t read, written;
  };

  // Scenario I: Buffer with the right capacity.
  std::vector<std::pair<std::string, Result>> tests = {
      {"Hello world!",
       {{72, 101, 108, 108, 111, 32, 119, 111, 114, 108, 100, 33}, 12, 12}},
      {"Hej! Привет! 你好！",
       {{72,  101, 106, 33, 32, 208, 159, 209, 128, 208, 184, 208, 178, 208,
         181, 209, 130, 33, 32, 228, 189, 160, 229, 165, 189, 239, 188, 129},
        16,
        28}},
      {"Да!", {{208, 148, 208, 176, 041}, 3, 5}},
  };

  Result want;
  std::string input;
  for (const auto &test : tests) {
    std::tie(input, want) = test;
    script::Handle<script::Uint8Array> destination = script::Uint8Array::New(
        stub_window_.global_environment(), want.bytes.size());
    TextEncoderEncodeIntoResult got = text_encoder_->EncodeInto(
        stub_window_.environment_settings(), input, destination);

    // Verify the read / written values.
    ASSERT_TRUE(got.has_read());
    ASSERT_TRUE(got.has_written());
    EXPECT_EQ(want.read, got.read());
    EXPECT_EQ(want.written, got.written());

    // Verify the actual data.
    auto *array_got = static_cast<uint8 *>(destination->RawData());
    ASSERT_LE(got.written(), want.bytes.size());
    for (uint32 i = 0; i < got.written(); ++i) {
      EXPECT_EQ(array_got[i], want.bytes[i]);
    }
  }
}

TEST_F(TextEncoderTest, EncodeIntoInsufficientCapacity) {
  struct Result {
    std::vector<uint8> bytes;
    uint64_t read, written;
  };

  // Scenario II: Intentionally remove last byte from the right capacity.
  std::vector<std::pair<std::string, Result>> tests = {
      {"Hello world!",
       {{72, 101, 108, 108, 111, 32, 119, 111, 114, 108, 100}, 11, 11}},
      {"Hej! Привет! 你好！",
       {{72,  101, 106, 33, 32, 208, 159, 209, 128, 208, 184, 208, 178, 208,
         181, 209, 130, 33, 32, 228, 189, 160, 229, 165, 189, 0,   0},
        15,
        25}},
      {"Да!", {{208, 148, 208, 176}, 2, 4}},
  };

  Result want;
  std::string input;
  for (const auto &test : tests) {
    std::tie(input, want) = test;
    script::Handle<script::Uint8Array> destination = script::Uint8Array::New(
        stub_window_.global_environment(), want.bytes.size());
    TextEncoderEncodeIntoResult got = text_encoder_->EncodeInto(
        stub_window_.environment_settings(), input, destination);

    // Verify the read / written values.
    ASSERT_TRUE(got.has_read());
    ASSERT_TRUE(got.has_written());
    EXPECT_EQ(want.read, got.read());
    EXPECT_EQ(want.written, got.written());

    // Verify the actual data.
    auto *array_got = static_cast<uint8 *>(destination->RawData());
    ASSERT_LE(got.written(), want.bytes.size());
    for (uint32 i = 0; i < want.bytes.size(); ++i) {
      EXPECT_EQ(array_got[i], want.bytes[i]);
    }
  }
}

TEST_F(TextEncoderTest, EncodeIntoExtraCapacity) {
  struct Result {
    std::vector<uint8> bytes;
    uint64_t read, written;
  };

  // Scenario III: Buffer with more capacity.
  std::vector<std::pair<std::string, Result>> tests = {
      {"Hello world!",
       {{72, 101, 108, 108, 111, 32, 119, 111, 114, 108, 100, 33, 0, 0},
        12,
        12}},
      {"Hej! Привет! 你好！",
       {{72,  101, 106, 33,  32,  208, 159, 209, 128, 208,
         184, 208, 178, 208, 181, 209, 130, 33,  32,  228,
         189, 160, 229, 165, 189, 239, 188, 129, 0,   0},
        16,
        28}},
      {"Да!", {{208, 148, 208, 176, 041, 0, 0}, 3, 5}},
  };

  Result want;
  std::string input;
  for (const auto &test : tests) {
    std::tie(input, want) = test;
    script::Handle<script::Uint8Array> destination = script::Uint8Array::New(
        stub_window_.global_environment(), want.bytes.size());
    TextEncoderEncodeIntoResult got = text_encoder_->EncodeInto(
        stub_window_.environment_settings(), input, destination);

    // Verify the read / written values.
    ASSERT_TRUE(got.has_read());
    ASSERT_TRUE(got.has_written());
    EXPECT_EQ(want.read, got.read());
    EXPECT_EQ(want.written, got.written());

    // Verify the actual data.
    auto *array_got = static_cast<uint8 *>(destination->RawData());
    ASSERT_LE(got.written(), want.bytes.size());
    for (uint32 i = 0; i < want.bytes.size(); ++i) {
      EXPECT_EQ(array_got[i], want.bytes[i]);
    }
  }
}

}  // namespace
}  // namespace encoding
}  // namespace cobalt
