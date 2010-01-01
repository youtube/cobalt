// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_offset_string_conversions.h"

#include "base/string_piece.h"
#include "base/utf_string_conversion_utils.h"

using base::PrepareForUTF16Or32Output;
using base::ReadUnicodeCharacter;
using base::WriteUnicodeCharacter;

// Generalized Unicode converter -----------------------------------------------

// Converts the given source Unicode character type to the given destination
// Unicode character type as a STL string. The given input buffer and size
// determine the source, and the given output STL string will be replaced by
// the result.
template<typename SRC_CHAR>
bool ConvertUnicode(const SRC_CHAR* src,
                    size_t src_len,
                    std::wstring* output,
                    size_t* offset_for_adjustment) {
  size_t output_offset =
      (offset_for_adjustment && *offset_for_adjustment < src_len) ?
          *offset_for_adjustment : std::wstring::npos;

  // ICU requires 32-bit numbers.
  bool success = true;
  int32 src_len32 = static_cast<int32>(src_len);
  for (int32 i = 0; i < src_len32; i++) {
    uint32 code_point;
    size_t original_i = i;
    size_t chars_written = 0;
    if (ReadUnicodeCharacter(src, src_len32, &i, &code_point)) {
      chars_written = WriteUnicodeCharacter(code_point, output);
    } else {
      chars_written = WriteUnicodeCharacter(0xFFFD, output);
      success = false;
    }
    if ((output_offset != std::wstring::npos) &&
        (*offset_for_adjustment > original_i)) {
      // NOTE: ReadUnicodeCharacter() adjusts |i| to point _at_ the last
      // character read, not after it (so that incrementing it in the loop
      // increment will place it at the right location), so we need to account
      // for that in determining the amount that was read.
      if (*offset_for_adjustment <= static_cast<size_t>(i))
        output_offset = std::wstring::npos;
      else
        output_offset += chars_written - (i - original_i + 1);
    }
  }

  if (offset_for_adjustment)
    *offset_for_adjustment = output_offset;
  return success;
}

// UTF-8 <-> Wide --------------------------------------------------------------

bool UTF8ToWideAndAdjustOffset(const char* src,
                               size_t src_len,
                               std::wstring* output,
                               size_t* offset_for_adjustment) {
  PrepareForUTF16Or32Output(src, src_len, output);
  return ConvertUnicode(src, src_len, output, offset_for_adjustment);
}

std::wstring UTF8ToWideAndAdjustOffset(const base::StringPiece& utf8,
                                       size_t* offset_for_adjustment) {
  std::wstring ret;
  UTF8ToWideAndAdjustOffset(utf8.data(), utf8.length(), &ret,
                            offset_for_adjustment);
  return ret;
}

// UTF-16 <-> Wide -------------------------------------------------------------

#if defined(WCHAR_T_IS_UTF16)

// When wide == UTF-16, then conversions are a NOP.
bool UTF16ToWideAndAdjustOffset(const char16* src,
                                size_t src_len,
                                std::wstring* output,
                                size_t* offset_for_adjustment) {
  output->assign(src, src_len);
  if (offset_for_adjustment && (*offset_for_adjustment >= src_len))
    *offset_for_adjustment = std::wstring::npos;
  return true;
}

std::wstring UTF16ToWideAndAdjustOffset(const string16& utf16,
                                        size_t* offset_for_adjustment) {
  if (offset_for_adjustment && (*offset_for_adjustment >= utf16.length()))
    *offset_for_adjustment = std::wstring::npos;
  return utf16;
}

#elif defined(WCHAR_T_IS_UTF32)

bool UTF16ToWideAndAdjustOffset(const char16* src,
                                size_t src_len,
                                std::wstring* output,
                                size_t* offset_for_adjustment) {
  output->clear();
  // Assume that normally we won't have any non-BMP characters so the counts
  // will be the same.
  output->reserve(src_len);
  return ConvertUnicode(src, src_len, output, offset_for_adjustment);
}

std::wstring UTF16ToWideAndAdjustOffset(const string16& utf16,
                                        size_t* offset_for_adjustment) {
  std::wstring ret;
  UTF16ToWideAndAdjustOffset(utf16.data(), utf16.length(), &ret,
                             offset_for_adjustment);
  return ret;
}

#endif  // defined(WCHAR_T_IS_UTF32)
