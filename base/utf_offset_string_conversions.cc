// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_offset_string_conversions.h"

#include <algorithm>

#include "base/scoped_ptr.h"
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
                    std::vector<size_t>* offsets_for_adjustment) {
  if (offsets_for_adjustment) {
    std::for_each(offsets_for_adjustment->begin(),
                  offsets_for_adjustment->end(),
                  LimitOffset<std::wstring>(src_len));
  }

  // ICU requires 32-bit numbers.
  bool success = true;
  AdjustOffset::Adjustments adjustments;
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
    if (offsets_for_adjustment) {
      // NOTE: ReadUnicodeCharacter() adjusts |i| to point _at_ the last
      // character read, not after it (so that incrementing it in the loop
      // increment will place it at the right location), so we need to account
      // for that in determining the amount that was read.
      adjustments.push_back(AdjustOffset::Adjustment(
          original_i, i - original_i + 1, chars_written));
    }
  }

  // Make offset adjustment.
  if (offsets_for_adjustment && !adjustments.empty()) {
    std::for_each(offsets_for_adjustment->begin(),
                  offsets_for_adjustment->end(),
                  AdjustOffset(adjustments));
  }

  return success;
}

// UTF-8 <-> Wide --------------------------------------------------------------

bool UTF8ToWideAndAdjustOffset(const char* src,
                               size_t src_len,
                               std::wstring* output,
                               size_t* offset_for_adjustment) {
  std::vector<size_t> offsets;
  if (offset_for_adjustment)
    offsets.push_back(*offset_for_adjustment);
  PrepareForUTF16Or32Output(src, src_len, output);
  bool ret = ConvertUnicode(src, src_len, output, &offsets);
  if (offset_for_adjustment)
    *offset_for_adjustment = offsets[0];
  return ret;
}

bool UTF8ToWideAndAdjustOffsets(const char* src,
                                size_t src_len,
                                std::wstring* output,
                                std::vector<size_t>* offsets_for_adjustment) {
  PrepareForUTF16Or32Output(src, src_len, output);
  return ConvertUnicode(src, src_len, output, offsets_for_adjustment);
}

std::wstring UTF8ToWideAndAdjustOffset(const base::StringPiece& utf8,
                                       size_t* offset_for_adjustment) {
  std::vector<size_t> offsets;
  if (offset_for_adjustment)
    offsets.push_back(*offset_for_adjustment);
  std::wstring result;
  UTF8ToWideAndAdjustOffsets(utf8.data(), utf8.length(), &result,
                             &offsets);
  if (offset_for_adjustment)
    *offset_for_adjustment = offsets[0];
  return result;
}

std::wstring UTF8ToWideAndAdjustOffsets(const base::StringPiece& utf8,
                                        std::vector<size_t>*
                                            offsets_for_adjustment) {
  std::wstring result;
  UTF8ToWideAndAdjustOffsets(utf8.data(), utf8.length(), &result,
                             offsets_for_adjustment);
  return result;
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

bool UTF16ToWideAndAdjustOffsets(const char16* src,
                                 size_t src_len,
                                 std::wstring* output,
                                 std::vector<size_t>* offsets_for_adjustment) {
  output->assign(src, src_len);
  if (offsets_for_adjustment) {
    std::for_each(offsets_for_adjustment->begin(),
                  offsets_for_adjustment->end(),
                  LimitOffset<std::wstring>(src_len));
  }
  return true;
}

std::wstring UTF16ToWideAndAdjustOffset(const string16& utf16,
                                        size_t* offset_for_adjustment) {
  if (offset_for_adjustment && (*offset_for_adjustment >= utf16.length()))
    *offset_for_adjustment = std::wstring::npos;
  return utf16;
}

std::wstring UTF16ToWideAndAdjustOffsets(
    const string16& utf16,
    std::vector<size_t>* offsets_for_adjustment) {
  if (offsets_for_adjustment) {
    std::for_each(offsets_for_adjustment->begin(),
                  offsets_for_adjustment->end(),
                  LimitOffset<std::wstring>(utf16.length()));
  }
  return utf16;
}

#elif defined(WCHAR_T_IS_UTF32)

bool UTF16ToWideAndAdjustOffset(const char16* src,
                                size_t src_len,
                                std::wstring* output,
                                size_t* offset_for_adjustment) {
  std::vector<size_t> offsets;
  if (offset_for_adjustment)
    offsets.push_back(*offset_for_adjustment);
  output->clear();
  // Assume that normally we won't have any non-BMP characters so the counts
  // will be the same.
  output->reserve(src_len);
  bool ret = ConvertUnicode(src, src_len, output, &offsets);
  if (offset_for_adjustment)
    *offset_for_adjustment = offsets[0];
  return ret;
}

bool UTF16ToWideAndAdjustOffsets(const char16* src,
                                 size_t src_len,
                                 std::wstring* output,
                                 std::vector<size_t>* offsets_for_adjustment) {
  output->clear();
  // Assume that normally we won't have any non-BMP characters so the counts
  // will be the same.
  output->reserve(src_len);
  return ConvertUnicode(src, src_len, output, offsets_for_adjustment);
}

std::wstring UTF16ToWideAndAdjustOffset(const string16& utf16,
                                        size_t* offset_for_adjustment) {
  std::vector<size_t> offsets;
  if (offset_for_adjustment)
    offsets.push_back(*offset_for_adjustment);
  std::wstring result;
  UTF16ToWideAndAdjustOffsets(utf16.data(), utf16.length(), &result,
                              &offsets);
  if (offset_for_adjustment)
    *offset_for_adjustment = offsets[0];
  return result;
}

std::wstring UTF16ToWideAndAdjustOffsets(
    const string16& utf16,
    std::vector<size_t>* offsets_for_adjustment) {
  std::wstring result;
  UTF16ToWideAndAdjustOffsets(utf16.data(), utf16.length(), &result,
                              offsets_for_adjustment);
  return result;
}

#endif  // defined(WCHAR_T_IS_UTF32)

AdjustOffset::Adjustment::Adjustment(size_t location,
                                     size_t old_length,
                                     size_t new_length)
  : location(location),
    old_length(old_length),
    new_length(new_length) {}

AdjustOffset::AdjustOffset(const Adjustments& adjustments)
    : adjustments_(adjustments) {}

void AdjustOffset::operator()(size_t& offset) {
  if (offset == std::wstring::npos)
    return;
  size_t adjustment = 0;
  for (Adjustments::const_iterator i = adjustments_.begin();
       i != adjustments_.end(); ++i) {
    size_t location = i->location;
    if (offset == location && i->new_length == 0) {
      offset = std::wstring::npos;
      return;
    }
    if (offset <= location)
      break;
    if (offset < (location + i->old_length)) {
      offset = std::wstring::npos;
      return;
    }
    adjustment += (i->old_length - i->new_length);
  }
  offset -= adjustment;
}
