
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


#include "cobalt/encoding/text_decoder.h"

#include <string>
#include <utility>
#include <vector>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/testing/stub_window.h"
#include "cobalt/script/array_buffer_view.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "cobalt/script/typed_arrays.h"


#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace cobalt {
namespace encoding {
namespace {

//////////////////////////////////////////////////////////////////////////
// TextDecoderTest
//////////////////////////////////////////////////////////////////////////

class TextDecoderTest : public ::testing::Test {
 protected:
  TextDecoderTest();
  ~TextDecoderTest();

  cobalt::dom::testing::StubWindow stub_window_;
  script::testing::MockExceptionState exception_state_;
};

TextDecoderTest::TextDecoderTest() {}
TextDecoderTest::~TextDecoderTest() {}

//////////////////////////////////////////////////////////////////////////
// Test cases
//////////////////////////////////////////////////////////////////////////

TEST_F(TextDecoderTest, Constructors) {
  scoped_refptr<TextDecoder> text_decoder;

  EXPECT_CALL(exception_state_, SetSimpleExceptionVA(script::kRangeError, _, _))
      .Times(0);
  text_decoder = new TextDecoder(&exception_state_);
  EXPECT_EQ("utf-8", text_decoder->encoding());
  text_decoder.reset();

  EXPECT_CALL(exception_state_, SetSimpleExceptionVA(script::kRangeError, _, _))
      .Times(0);
  text_decoder = new TextDecoder("utf-16", &exception_state_);
  // It seems default is little endian.
  EXPECT_EQ("utf-16le", text_decoder->encoding());
  text_decoder.reset();

  EXPECT_CALL(exception_state_, SetSimpleExceptionVA(script::kRangeError, _, _))
      .Times(0);
  text_decoder = new TextDecoder("utf-16be", &exception_state_);
  EXPECT_EQ("utf-16be", text_decoder->encoding());
  text_decoder.reset();

  EXPECT_CALL(exception_state_,
              SetSimpleExceptionVA(script::kRangeError, _, _));
  text_decoder = new TextDecoder("foo-encoding", &exception_state_);
  EXPECT_EQ("", text_decoder->encoding());
  text_decoder.reset();
}

TEST_F(TextDecoderTest, DecodeUTF8) {
  EXPECT_CALL(exception_state_, SetSimpleExceptionVA(script::kRangeError, _, _))
      .Times(0);
  scoped_refptr<TextDecoder> text_decoder_ = new TextDecoder(&exception_state_);
  std::vector<std::pair<std::vector<uint8>, std::string>> tests = {
      {{72, 101, 108, 108, 111, 32, 119, 111, 114, 108, 100, 33},
       "Hello world!"},
      {{72,  101, 106, 33, 32, 208, 159, 209, 128, 208, 184, 208, 178, 208,
        181, 209, 130, 33, 32, 228, 189, 160, 229, 165, 189, 239, 188, 129},
       "Hej! Привет! 你好！"},
      {{208, 148, 208, 176, 041}, "Да!"},
  };

  for (const auto &test : tests) {
    const std::string &expected = test.second;
    const std::vector<uint8> &raw_data = test.first;
    script::Handle<script::ArrayBuffer> array_buffer = script::ArrayBuffer::New(
        stub_window_.global_environment(), raw_data.data(), raw_data.size());
    EXPECT_CALL(exception_state_,
                SetSimpleExceptionVA(script::kRangeError, _, _))
        .Times(0);
    std::string result = text_decoder_->Decode(dom::BufferSource(array_buffer),
                                               &exception_state_);
    EXPECT_EQ(result, expected);
  }
}

TEST_F(TextDecoderTest, DecodeUTF16) {
  EXPECT_CALL(exception_state_, SetSimpleExceptionVA(script::kRangeError, _, _))
      .Times(0);
  scoped_refptr<TextDecoder> text_decoder_ =
      new TextDecoder("utf-16", &exception_state_);
  std::vector<std::pair<std::vector<uint8>, std::string>> tests = {
      {{0x14, 0x04, 0x30, 0x04, 0x21, 0x00}, "Да!"},
  };

  for (const auto &test : tests) {
    const std::string &expected = test.second;
    const std::vector<uint8> &raw_data = test.first;
    script::Handle<script::ArrayBuffer> array_buffer = script::ArrayBuffer::New(
        stub_window_.global_environment(), raw_data.data(), raw_data.size());
    EXPECT_CALL(exception_state_,
                SetSimpleExceptionVA(script::kRangeError, _, _))
        .Times(0);
    std::string result = text_decoder_->Decode(dom::BufferSource(array_buffer),
                                               &exception_state_);
    LOG(WARNING) << result << "\n";
    EXPECT_EQ(result, expected);
  }
}

TEST_F(TextDecoderTest, DecodeUTF16BE) {
  EXPECT_CALL(exception_state_, SetSimpleExceptionVA(script::kRangeError, _, _))
      .Times(0);
  scoped_refptr<TextDecoder> text_decoder_ =
      new TextDecoder("utf-16be", &exception_state_);
  std::vector<std::pair<std::vector<uint8>, std::string>> tests = {
      {{0x04, 0x14, 0x04, 0x30, 0x00, 0x21}, "Да!"},
  };

  for (const auto &test : tests) {
    const std::string &expected = test.second;
    const std::vector<uint8> &raw_data = test.first;
    script::Handle<script::ArrayBuffer> array_buffer = script::ArrayBuffer::New(
        stub_window_.global_environment(), raw_data.data(), raw_data.size());
    EXPECT_CALL(exception_state_,
                SetSimpleExceptionVA(script::kRangeError, _, _))
        .Times(0);
    std::string result = text_decoder_->Decode(dom::BufferSource(array_buffer),
                                               &exception_state_);
    LOG(WARNING) << result << "\n";
    EXPECT_EQ(result, expected);
  }
}

}  // namespace
}  // namespace encoding
}  // namespace cobalt
