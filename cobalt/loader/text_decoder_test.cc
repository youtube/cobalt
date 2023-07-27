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

#include "cobalt/loader/text_decoder.h"

#include <memory>

#include "base/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace loader {
namespace {

struct TextDecoderCallback {
  void Callback(const Origin& last_url_origin,
                std::unique_ptr<std::string> value) {
    text = *value;
    last_url_origin_ = last_url_origin;
  }
  std::string text;
  Origin last_url_origin_;
};

}  // namespace

TEST(TextDecoderTest, EmptyString) {
  TextDecoderCallback text_decoder_result;
  std::unique_ptr<Decoder> text_decoder = TextDecoder::Create(base::Bind(
      &TextDecoderCallback::Callback, base::Unretained(&text_decoder_result)));

  EXPECT_EQ("", text_decoder_result.text);
  text_decoder->DecodeChunk("abc", 0);
  text_decoder->DecodeChunk("", 0);
  text_decoder->Finish();
  EXPECT_EQ("", text_decoder_result.text);
}

TEST(TextDecoderTest, NonEmptyString) {
  TextDecoderCallback text_decoder_result;
  std::unique_ptr<Decoder> text_decoder = TextDecoder::Create(base::Bind(
      &TextDecoderCallback::Callback, base::Unretained(&text_decoder_result)));

  EXPECT_EQ("", text_decoder_result.text);
  text_decoder->DecodeChunk("abc", 3);
  text_decoder->DecodeChunk("defghi", 3);
  text_decoder->Finish();
  EXPECT_EQ("abcdef", text_decoder_result.text);
}

}  // namespace loader
}  // namespace cobalt
