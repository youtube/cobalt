// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/string.h"

namespace starboard {

std::string HexEncode(const void* data, int size, const char* delimiter) {
  const char kDecToHex[] = "0123456789abcdef";

  std::string result;
  auto delimiter_size = delimiter ? std::strlen(delimiter) : 0;
  result.reserve((delimiter_size + 2) * size);

  const uint8_t* data_in_uint8 = static_cast<const uint8_t*>(data);

  for (int i = 0; i < size; ++i) {
    result += kDecToHex[data_in_uint8[i] / 16];
    result += kDecToHex[data_in_uint8[i] % 16];
    if (i != size - 1 && delimiter != nullptr) {
      result += delimiter;
    }
  }

  return result;
}

std::string FormatWithDigitSeparators(int64_t number) {
  if (number == 0) {
    return "0";
  }

  std::string str = std::to_string(number);
  std::string reversed_result;
  reversed_result.reserve(str.length() + (str.length() - 1) / 3);
  int digit_count = 0;
  for (int i = str.length() - 1; i >= 0; --i) {
    if (str[i] == '-') {
      continue;
    }
    if (digit_count > 0 && digit_count % 3 == 0) {
      reversed_result.push_back('\'');
    }
    reversed_result.push_back(str[i]);
    digit_count++;
  }
  if (number < 0) {
    reversed_result.push_back('-');
  }
  return {reversed_result.rbegin(), reversed_result.rend()};
}

}  // namespace starboard
