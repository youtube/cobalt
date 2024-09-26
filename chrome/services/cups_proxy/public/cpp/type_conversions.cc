// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/public/cpp/type_conversions.h"

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"

namespace ipp_converter {

// Implicit conversion is safe since the conversion preserves memory layout.
std::vector<uint8_t> ConvertToByteBuffer(base::StringPiece char_buffer) {
  std::vector<uint8_t> byte_buffer;
  byte_buffer.resize(char_buffer.size());

  base::ranges::copy(char_buffer, byte_buffer.begin());
  return byte_buffer;
}

// Implicit conversion is safe since the conversion preserves memory layout.
std::vector<char> ConvertToCharBuffer(base::span<const uint8_t> byte_buffer) {
  std::vector<char> char_buffer;
  char_buffer.resize(byte_buffer.size());

  base::ranges::copy(byte_buffer, char_buffer.begin());
  return char_buffer;
}

std::string ConvertToString(base::span<const uint8_t> byte_buffer) {
  std::vector<char> char_buffer = ConvertToCharBuffer(byte_buffer);
  return std::string(char_buffer.begin(), char_buffer.end());
}

}  // namespace ipp_converter
