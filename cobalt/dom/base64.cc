// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/base64.h"

#include "base/base64.h"

namespace cobalt {
namespace dom {

namespace {
// https://infra.spec.whatwg.org/#ascii-whitespace
const char kAsciiWhitespace[] = {0x09, 0x0a, 0x0c, 0x0d, 0x20};

bool IsAtobAllowedChar(char c) {
  // atob() allows '+', '/', and ASCII alphanumeric characters.
  // https://infra.spec.whatwg.org/#forgiving-base64-decode
  if (c == '+') {
    return true;
  }
  if (c < '/') {
    return false;
  }
  if (c <= '9') {
    return true;
  }
  if (c < 'A') {
    return false;
  }
  if (c <= 'Z') {
    return true;
  }
  if (c < 'a') {
    return false;
  }
  if (c <= 'z') {
    return true;
  }
  return false;
}

bool IsNotAsciiWhitespace(char c) {
  for (char sp_char : kAsciiWhitespace) {
    if (c == sp_char) {
      return false;
    }
  }
  return true;
}

base::optional<std::string> GetAtobAllowedStr(const std::string& input_str) {
  // Our base64 decoding method does not check for forbidden characters. We use
  // the following method to reject string containing disallowed character or
  // format in atob().
  // Step 1-4 of https://infra.spec.whatwg.org/#forgiving-base64-decode
  std::string output_str;
  // Step 1: Remove all ASCII whitespace from data.
  std::copy_if(input_str.begin(), input_str.end(),
               std::back_inserter(output_str), IsNotAsciiWhitespace);
  // Step 2: If data's length divides by 4 leaving no remainder, then:
  //        1. if data ends with one or two U+003D (=) code points, then
  //           remove them from data.
  if (!output_str.empty() && output_str.length() % 4 == 0 &&
      output_str.back() == '=') {
    output_str.pop_back();
    if (output_str.back() == '=') {
      output_str.pop_back();
    }
  }
  // Step 3: If data's length divides by 4 leaving a remainder of 1, return
  // failure.
  if (output_str.length() % 4 == 1) {
    return base::nullopt;
  }
  // Step 4: If data contains a code point that is not allowed, return failure.
  for (char current_char : output_str) {
    if (!IsAtobAllowedChar(current_char)) {
      return base::nullopt;
    }
  }
  // Customized step: add padding to string less than 4 character.
  for (size_t i = output_str.length() % 4; i > 1 && i < 4; i++) {
    output_str.push_back('=');
  }
  return output_str;
}

base::optional<std::string> Utf8ToLatin1(const std::string& input) {
  std::string output;
  unsigned char current_char_remainder = 0x00;
  for (unsigned char c : input) {
    if (c <= 0x7f && !current_char_remainder) {
      output.push_back(c);
    } else if (c <= 0xbf && current_char_remainder) {
      // This is the only byte or second byte of one character.
      output.push_back(current_char_remainder | (c & 0x3f));
      current_char_remainder = 0x00;
    } else if (c == 0xc2 && !current_char_remainder) {
      current_char_remainder = c & 0x80;
    } else if (c == 0xc3 && !current_char_remainder) {
      current_char_remainder = 0xc0;
    } else {
      return base::nullopt;
    }
  }
  return output;
}
}  // namespace

base::optional<std::string> ForgivingBase64Encode(
    const std::string& string_to_encode) {
  // https://infra.spec.whatwg.org/#forgiving-base64-encode
  auto maybe_string_to_encode_in_latin1 = Utf8ToLatin1(string_to_encode);
  std::string output;
  if (!maybe_string_to_encode_in_latin1 ||
      !base::Base64Encode(*maybe_string_to_encode_in_latin1, &output)) {
    return base::nullopt;
  }
  return output;
}

base::optional<std::vector<uint8_t>> ForgivingBase64Decode(
    const std::string& encoded_string) {
  // https://infra.spec.whatwg.org/#forgiving-base64-decode
  // Step 1-4:
  auto maybe_encoded_string_no_whitespace = GetAtobAllowedStr(encoded_string);
  // Step 5-10:
  std::vector<uint8_t> output;
  // If input string format is not allowed or base64 encoding failed, return
  // nullopt to signal failure.
  if (!maybe_encoded_string_no_whitespace ||
      !base::Base64Decode(*maybe_encoded_string_no_whitespace, &output)) {
    return base::nullopt;
  }
  return output;
}

}  // namespace dom
}  // namespace cobalt
