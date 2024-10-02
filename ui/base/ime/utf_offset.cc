// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/utf_offset.h"

#include <string>

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"

namespace ui {

absl::optional<size_t> Utf16OffsetFromUtf8Offset(base::StringPiece text,
                                                 size_t utf8_offset) {
  if (utf8_offset > text.length())
    return absl::nullopt;

  // TODO(hidehiko): Update not to depend on UTF8ToUTF16 to avoid
  // unnecessary memory allocation.
  std::u16string converted;
  if (!base::UTF8ToUTF16(text.data(), utf8_offset, &converted))
    return absl::nullopt;
  return converted.length();
}

absl::optional<size_t> Utf8OffsetFromUtf16Offset(base::StringPiece16 text,
                                                 size_t utf16_offset) {
  if (utf16_offset > text.length())
    return absl::nullopt;

  // TODO(hidehiko): Update not to depend on UTF16ToUTF8 to avoid
  // unnecessary memory allocation.
  std::string converted;
  if (!base::UTF16ToUTF8(text.data(), utf16_offset, &converted))
    return absl::nullopt;
  return converted.length();
}

}  // namespace ui
