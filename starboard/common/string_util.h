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
#include <type_traits>

namespace starboard {

// Returns true if `str` starts with `prefix`.
constexpr bool StartsWith(std::string_view str,
                          std::string_view prefix) noexcept {
  return str.size() >= prefix.size() &&
         str.compare(0, prefix.size(), prefix) == 0;
}

// Returns true if `str` ends with `suffix`.
constexpr bool EndsWith(std::string_view str,
                        std::string_view suffix) noexcept {
  return str.size() >= suffix.size() &&
         str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Locale-independent ASCII character classification helpers. Unlike <cctype>
// functions, these are constexpr, safe to call with any signed or unsigned
// char value without casting, and do not narrow wider integer/character types.
template <typename Char,
          std::enable_if_t<std::is_integral_v<Char> &&
                               !std::is_same_v<std::remove_cv_t<Char>, bool>,
                           int> = 0>
constexpr bool IsAsciiWhitespace(Char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
         c == '\v';
}

template <typename Char,
          std::enable_if_t<std::is_integral_v<Char> &&
                               !std::is_same_v<std::remove_cv_t<Char>, bool>,
                           int> = 0>
constexpr bool IsAsciiUpper(Char c) noexcept {
  return c >= 'A' && c <= 'Z';
}

template <typename Char,
          std::enable_if_t<std::is_integral_v<Char> &&
                               !std::is_same_v<std::remove_cv_t<Char>, bool>,
                           int> = 0>
constexpr bool IsAsciiLower(Char c) noexcept {
  return c >= 'a' && c <= 'z';
}

template <typename Char,
          std::enable_if_t<std::is_integral_v<Char> &&
                               !std::is_same_v<std::remove_cv_t<Char>, bool>,
                           int> = 0>
constexpr bool IsAsciiAlpha(Char c) noexcept {
  return IsAsciiUpper(c) || IsAsciiLower(c);
}

template <typename Char,
          std::enable_if_t<std::is_integral_v<Char> &&
                               !std::is_same_v<std::remove_cv_t<Char>, bool>,
                           int> = 0>
constexpr bool IsAsciiDigit(Char c) noexcept {
  return c >= '0' && c <= '9';
}

template <typename Char,
          std::enable_if_t<std::is_integral_v<Char> &&
                               !std::is_same_v<std::remove_cv_t<Char>, bool>,
                           int> = 0>
constexpr bool IsHexDigit(Char c) noexcept {
  return IsAsciiDigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

template <typename Char,
          std::enable_if_t<std::is_integral_v<Char> &&
                               !std::is_same_v<std::remove_cv_t<Char>, bool>,
                           int> = 0>
constexpr bool IsAsciiPrintable(Char c) noexcept {
  return c >= ' ' && c <= '~';
}

template <typename Char,
          std::enable_if_t<std::is_integral_v<Char> &&
                               !std::is_same_v<std::remove_cv_t<Char>, bool>,
                           int> = 0>
constexpr Char ToLowerASCII(Char c) noexcept {
  return (c >= 'A' && c <= 'Z') ? static_cast<Char>(c + ('a' - 'A')) : c;
}

}  // namespace starboard

#endif  // STARBOARD_COMMON_STRING_UTIL_H_
