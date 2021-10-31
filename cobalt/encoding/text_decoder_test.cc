
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

  std::string want;
  std::vector<uint8> data;
  for (const auto &test : tests) {
    std::tie(data, want) = test;
    script::Handle<script::ArrayBuffer> array_buffer = script::ArrayBuffer::New(
        stub_window_.global_environment(), data.data(), data.size());
    EXPECT_CALL(exception_state_,
                SetSimpleExceptionVA(script::kRangeError, _, _))
        .Times(0);
    std::string got = text_decoder_->Decode(dom::BufferSource(array_buffer),
                                            &exception_state_);
    EXPECT_EQ(got, want);
  }
}

TEST_F(TextDecoderTest, DecodeUTF8Surrogates) {
  EXPECT_CALL(exception_state_, SetSimpleExceptionVA(script::kRangeError, _, _))
      .Times(0);
  scoped_refptr<TextDecoder> text_decoder_ = new TextDecoder(&exception_state_);
  std::vector<std::pair<std::vector<uint8>, std::string>> tests = {
      {{0x61, 0x62, 0x63, 0x31, 0x32, 0x33}, "abc123"},  // Sanity check
      {{0xef, 0xbf, 0xbd}, "\xEF\xBF\xBD"},              // Surrogate half (low)
      {{0x61, 0x62, 0x63, 0xef, 0xbf, 0xbd, 0x31, 0x32, 0x33},
       "abc\xEF\xBF\xBD"
       "123"},  // Surrogate half (high)
      {{0xef, 0xbf, 0xbd, 0xef, 0xbf, 0xbd},
       "\xEF\xBF\xBD\xEF\xBF\xBD"},     // Wrong order
      {{0xf0, 0x90, 0x80, 0x80}, "𐀀"},  // Right order
  };

  std::string want;
  std::vector<uint8> data;
  for (const auto &test : tests) {
    std::tie(data, want) = test;
    script::Handle<script::ArrayBuffer> array_buffer = script::ArrayBuffer::New(
        stub_window_.global_environment(), data.data(), data.size());
    EXPECT_CALL(exception_state_,
                SetSimpleExceptionVA(script::kRangeError, _, _))
        .Times(0);
    std::string got = text_decoder_->Decode(dom::BufferSource(array_buffer),
                                            &exception_state_);
    EXPECT_EQ(got, want);
  }
}

TEST_F(TextDecoderTest, DecodeUTF8Fatal) {
  std::vector<std::pair<std::string, std::vector<uint8>>> tests = {
      {"utf-8", {0xFF}},                                // Invalid code
      {"utf-8", {0xC0}},                                // Ends early
      {"utf-8", {0xE0}},                                // Ends early (2)
      {"utf-8", {0xC0, 0x00}},                          // Invalid trail
      {"utf-8", {0xC0, 0xC0}},                          // Invalid trail (2)
      {"utf-8", {0xE0, 0x00}},                          // Invalid trail (3)
      {"utf-8", {0xE0, 0xC0}},                          // Invalid trail (4)
      {"utf-8", {0xE0, 0x80, 0x00}},                    // Invalid trail (5)
      {"utf-8", {0xE0, 0x80, 0xC0}},                    // Invalid trail (6)
      {"utf-8", {0xFC, 0x80, 0x80, 0x80, 0x80, 0x80}},  // > 0x10FFFF
      {"utf-8", {0xFE, 0x80, 0x80, 0x80, 0x80, 0x80}},  // Obsolete lead byte
      {"utf-8", {0xC0, 0x80}},                    // Overlong U+0000 - 2 bytes
      {"utf-8", {0xE0, 0x80, 0x80}},              // overlong U+0000 - 3 bytes
      {"utf-8", {0xF0, 0x80, 0x80, 0x80}},        // Overlong U+0000 - 4 bytes
      {"utf-8", {0xF8, 0x80, 0x80, 0x80, 0x80}},  // Overlong U+0000 - 5 bytes
      {"utf-8",
       {0xFC, 0x80, 0x80, 0x80, 0x80, 0x80}},     // Overlong U+0000 - 6 bytes
      {"utf-8", {0xC1, 0xBF}},                    // Overlong U+007F - 2 bytes
      {"utf-8", {0xE0, 0x81, 0xBF}},              // Overlong U+007F - 3 bytes
      {"utf-8", {0xF0, 0x80, 0x81, 0xBF}},        // Overlong U+007F - 4 bytes
      {"utf-8", {0xF8, 0x80, 0x80, 0x81, 0xBF}},  // Overlong U+007F - 5 bytes
      {"utf-8",
       {0xFC, 0x80, 0x80, 0x80, 0x81, 0xBF}},     // Overlong U+007F - 6 bytes
      {"utf-8", {0xE0, 0x9F, 0xBF}},              // Overlong U+07FF - 3 bytes
      {"utf-8", {0xF0, 0x80, 0x9F, 0xBF}},        // Overlong U+07FF - 4 bytes
      {"utf-8", {0xF8, 0x80, 0x80, 0x9F, 0xBF}},  // Overlong U+07FF - 5 bytes
      {"utf-8",
       {0xFC, 0x80, 0x80, 0x80, 0x9F, 0xBF}},     // Overlong U+07FF - 6 bytes
      {"utf-8", {0xF0, 0x8F, 0xBF, 0xBF}},        // Overlong U+FFFF - 4 bytes
      {"utf-8", {0xF8, 0x80, 0x8F, 0xBF, 0xBF}},  // Overlong U+FFFF - 5 bytes
      {"utf-8",
       {0xFC, 0x80, 0x80, 0x8F, 0xBF, 0xBF}},     // Overlong U+FFFF - 6 bytes
      {"utf-8", {0xF8, 0x84, 0x8F, 0xBF, 0xBF}},  // Overlong U+10FFFF - 5 bytes
      {"utf-8",
       {0xFC, 0x80, 0x84, 0x8F, 0xBF, 0xBF}},  // Overlong U+10FFFF - 6 bytes
      {"utf-8", {0xED, 0xA0, 0x80}},           // Lead surrogate
      {"utf-8", {0xED, 0xB0, 0x80}},           // Trail surrogate
      {"utf-8", {0xED, 0xA0, 0x80, 0xED, 0xB0, 0x80}},  // Surrogate pair
      {"utf-16le", {0x00}},                             // Truncated code unit
  };

  std::string label, want = "";
  std::vector<uint8> input;
  TextDecoderOptions options;
  options.set_fatal(true);
  for (const auto &test : tests) {
    std::tie(label, input) = test;
    // No errors expected while constructing the object.
    EXPECT_CALL(exception_state_,
                SetSimpleExceptionVA(script::kRangeError, _, _))
        .Times(0);
    scoped_refptr<TextDecoder> text_decoder_ =
        new TextDecoder(label, options, &exception_state_);
    script::Handle<script::ArrayBuffer> array_buffer = script::ArrayBuffer::New(
        stub_window_.global_environment(), input.data(), input.size());
    // Malformed strings must race an exception when fatal is set to true.
    EXPECT_CALL(exception_state_,
                SetSimpleExceptionVA(script::kTypeError, _, _))
        .Times(1);
    std::string got = text_decoder_->Decode(dom::BufferSource(array_buffer),
                                            &exception_state_);
    EXPECT_EQ(got, want);
  }
}

TEST_F(TextDecoderTest, DecodeIgnoreBOM) {
  std::vector<std::pair<std::string, std::vector<uint8>>> tests = {
      {"utf-8", {0xEF, 0xBB, 0xBF, 0x61, 0x62, 0x63}},
      {"utf-16le", {0xFF, 0xFE, 0x61, 0x00, 0x62, 0x00, 0x63, 0x00}},
      {"utf-16be", {0xFE, 0xFF, 0x00, 0x61, 0x00, 0x62, 0x00, 0x63}},
  };

  // u8 prefix needed for compilation on Windows.
  const std::string kBOM = u8"\uFEFF";
  const std::string kABC = "abc";
  std::string label;
  std::vector<uint8> data;
  TextDecoderOptions options;

  for (const auto &test : tests) {
    std::tie(label, data) = test;
    {  // BOM should be present in decoded string if ignored.
      options.set_ignore_bom(true);
      EXPECT_CALL(exception_state_,
                  SetSimpleExceptionVA(script::kRangeError, _, _))
          .Times(0);
      scoped_refptr<TextDecoder> text_decoder_ =
          new TextDecoder(label, options, &exception_state_);

      script::Handle<script::ArrayBuffer> array_buffer =
          script::ArrayBuffer::New(stub_window_.global_environment(),
                                   data.data(), data.size());
      EXPECT_CALL(exception_state_,
                  SetSimpleExceptionVA(script::kRangeError, _, _))
          .Times(0);
      std::string got = text_decoder_->Decode(dom::BufferSource(array_buffer),
                                              &exception_state_);
      std::string want = kBOM + kABC;
      EXPECT_EQ(got, want);
    }
    {  // BOM should be absent from decoded string if not ignored.
      options.set_ignore_bom(false);
      EXPECT_CALL(exception_state_,
                  SetSimpleExceptionVA(script::kRangeError, _, _))
          .Times(0);
      scoped_refptr<TextDecoder> text_decoder_ =
          new TextDecoder(label, options, &exception_state_);

      script::Handle<script::ArrayBuffer> array_buffer =
          script::ArrayBuffer::New(stub_window_.global_environment(),
                                   data.data(), data.size());
      EXPECT_CALL(exception_state_,
                  SetSimpleExceptionVA(script::kRangeError, _, _))
          .Times(0);
      std::string got = text_decoder_->Decode(dom::BufferSource(array_buffer),
                                              &exception_state_);
      std::string want = kABC;
      EXPECT_EQ(got, want);
    }
    {  // BOM should be absent from decoded string by default.
      EXPECT_CALL(exception_state_,
                  SetSimpleExceptionVA(script::kRangeError, _, _))
          .Times(0);
      scoped_refptr<TextDecoder> text_decoder_ =
          new TextDecoder(label, &exception_state_);

      script::Handle<script::ArrayBuffer> array_buffer =
          script::ArrayBuffer::New(stub_window_.global_environment(),
                                   data.data(), data.size());
      EXPECT_CALL(exception_state_,
                  SetSimpleExceptionVA(script::kRangeError, _, _))
          .Times(0);
      std::string got = text_decoder_->Decode(dom::BufferSource(array_buffer),
                                              &exception_state_);
      std::string want = kABC;
      EXPECT_EQ(got, want);
    }
  }
}

TEST_F(TextDecoderTest, DecodeUTFStreamSimple) {
  std::string want = "abc123Да!𐀀";
  std::vector<std::pair<std::string, std::vector<uint8>>> tests = {
      {
          "utf-8",
          {0x61, 0x62, 0x63, 0x31, 0x32, 0x33, 0xd0, 0x94, 0xd0, 0xb0, 0x21,
           0xf0, 0x90, 0x80, 0x80},
      },
      {
          "utf-16be",
          {0x00, 0x61, 0x00, 0x62, 0x00, 0x63, 0x00, 0x31, 0x00, 0x32, 0x00,
           0x33, 0x04, 0x14, 0x04, 0x30, 0x00, 0x21, 0xd8, 0x00, 0xdc, 0x00},
      },
      {
          "utf-16le",
          {0x61, 0x00, 0x62, 0x00, 0x63, 0x00, 0x31, 0x00, 0x32, 0x00, 0x33,
           0x00, 0x14, 0x04, 0x30, 0x04, 0x21, 0x00, 0x00, 0xd8, 0x00, 0xdc},
      },
  };
  std::string encoding_label;
  std::vector<uint8> data;
  TextDecodeOptions stream_option;
  stream_option.set_stream(true);
  for (int chunk_size = 1; chunk_size <= 1; ++chunk_size) {
    for (const auto &test : tests) {
      std::tie(encoding_label, data) = test;
      EXPECT_CALL(exception_state_,
                  SetSimpleExceptionVA(script::kRangeError, _, _))
          .Times(0);
      scoped_refptr<TextDecoder> text_decoder_ =
          new TextDecoder(encoding_label, &exception_state_);
      std::string got;
      for (std::size_t offset = 0; offset < data.size(); offset += chunk_size) {
        const auto start = data.begin() + offset;
        const auto end = (offset + chunk_size >= data.size())
                             ? data.end()
                             : data.begin() + (offset + chunk_size);
        std::vector<uint8> byte_chunk(start, end);
        script::Handle<script::ArrayBuffer> chunk =
            script::ArrayBuffer::New(stub_window_.global_environment(),
                                     byte_chunk.data(), byte_chunk.size());
        EXPECT_CALL(exception_state_,
                    SetSimpleExceptionVA(script::kRangeError, _, _))
            .Times(0);
        // decoding with {stream: true}
        got += text_decoder_->Decode(dom::BufferSource(chunk), stream_option,
                                     &exception_state_);
      }
      EXPECT_CALL(exception_state_,
                  SetSimpleExceptionVA(script::kRangeError, _, _))
          .Times(0);
      got += text_decoder_->Decode(&exception_state_);
      EXPECT_EQ(got, want);
    }
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

  std::string want;
  std::vector<uint8> data;
  for (const auto &test : tests) {
    std::tie(data, want) = test;
    script::Handle<script::ArrayBuffer> array_buffer = script::ArrayBuffer::New(
        stub_window_.global_environment(), data.data(), data.size());
    EXPECT_CALL(exception_state_,
                SetSimpleExceptionVA(script::kRangeError, _, _))
        .Times(0);
    std::string got = text_decoder_->Decode(dom::BufferSource(array_buffer),
                                            &exception_state_);
    EXPECT_EQ(got, want);
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

  std::string want;
  std::vector<uint8> data;
  for (const auto &test : tests) {
    std::tie(data, want) = test;
    script::Handle<script::ArrayBuffer> array_buffer = script::ArrayBuffer::New(
        stub_window_.global_environment(), data.data(), data.size());
    EXPECT_CALL(exception_state_,
                SetSimpleExceptionVA(script::kRangeError, _, _))
        .Times(0);
    std::string got = text_decoder_->Decode(dom::BufferSource(array_buffer),
                                            &exception_state_);
    EXPECT_EQ(got, want);
  }
}

}  // namespace
}  // namespace encoding
}  // namespace cobalt
