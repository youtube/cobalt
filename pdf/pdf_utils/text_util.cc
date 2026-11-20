// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_utils/text_util.h"

#include <stdint.h>

#include "base/numerics/byte_conversions.h"
#include "base/strings/string_util.h"
#include "pdf/pdfium/pdfium_range.h"

namespace chrome_pdf {

bool IsWordBoundary(uint32_t ch) {
  // Deal with ASCII characters.
  if (base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch) || ch == '_') {
    return false;
  }
  return ch < 128 || ch == kZeroWidthSpace;
}

std::vector<unsigned char> ToUTF16BEBlob(const std::u16string& u16str) {
  std::vector<unsigned char> out(2 + 2 * u16str.size());
  // Insert the BOM
  out[0] = 0xFE;
  out[1] = 0xFF;
  size_t i = 2;
  for (char16_t c : u16str) {
    auto [c1, c2] = base::U16ToBigEndian(c);
    out[i++] = c1;
    out[i++] = c2;
  }
  return out;
}

}  // namespace chrome_pdf
