// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

// Subset of Chromium's base/strings/string_util.h for Starboard targets that
// cannot depend on //base.

#ifndef STARBOARD_COMMON_STRING_UTIL_H_
#define STARBOARD_COMMON_STRING_UTIL_H_

#include <string_view>

namespace starboard {

// Returns true if `str` starts with `prefix`.
inline bool StartsWith(std::string_view str, std::string_view prefix) {
  return str.substr(0, prefix.size()) == prefix;
}

// Returns true if `str` ends with `suffix`.
inline bool EndsWith(std::string_view str, std::string_view suffix) {
  return str.size() >= suffix.size() &&
         str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Locale-independent ASCII character classification helpers. Unlike <cctype>
// functions, these are constexpr and safe to call with any signed or unsigned
// char value without casting.
constexpr bool IsAsciiWhitespace(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
         c == '\v';
}

constexpr bool IsAsciiUpper(char c) noexcept {
  return c >= 'A' && c <= 'Z';
}

constexpr bool IsAsciiLower(char c) noexcept {
  return c >= 'a' && c <= 'z';
}

constexpr bool IsAsciiAlpha(char c) noexcept {
  return IsAsciiUpper(c) || IsAsciiLower(c);
}

constexpr bool IsAsciiDigit(char c) noexcept {
  return c >= '0' && c <= '9';
}

constexpr bool IsHexDigit(char c) noexcept {
  return IsAsciiDigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

constexpr bool IsAsciiPrintable(char c) noexcept {
  return c >= ' ' && c <= '~';
}

constexpr char ToLowerASCII(char c) noexcept {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

}  // namespace starboard

#endif  // STARBOARD_COMMON_STRING_UTIL_H_
