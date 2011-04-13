// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UTF_OFFSET_STRING_CONVERSIONS_H_
#define BASE_UTF_OFFSET_STRING_CONVERSIONS_H_
#pragma once

#include <string>
#include <vector>

#include "base/base_api.h"
#include "base/string16.h"

namespace base {
class StringPiece;
}

// Like the conversions in utf_string_conversions.h, but also takes one or more
// offsets (|offset[s]_for_adjustment|) into the source strings, each offset
// will be adjusted to point at the same logical place in the result strings.
// If this isn't possible because an offset points past the end of the source
// strings or into the middle of a multibyte sequence, the offending offset will
// be set to std::wstring::npos. |offset[s]_for_adjustment| may be NULL.
BASE_API bool UTF8ToWideAndAdjustOffset(const char* src,
                                        size_t src_len,
                                        std::wstring* output,
                                        size_t* offset_for_adjustment);
BASE_API bool UTF8ToWideAndAdjustOffsets(
    const char* src,
    size_t src_len,
    std::wstring* output,
    std::vector<size_t>* offsets_for_adjustment);

BASE_API std::wstring UTF8ToWideAndAdjustOffset(const base::StringPiece& utf8,
                                                size_t* offset_for_adjustment);
BASE_API std::wstring UTF8ToWideAndAdjustOffsets(
    const base::StringPiece& utf8,
    std::vector<size_t>* offsets_for_adjustment);

BASE_API bool UTF16ToWideAndAdjustOffset(const char16* src,
                                         size_t src_len,
                                         std::wstring* output,
                                         size_t* offset_for_adjustment);
BASE_API bool UTF16ToWideAndAdjustOffsets(
    const char16* src,
    size_t src_len,
    std::wstring* output,
    std::vector<size_t>* offsets_for_adjustment);

BASE_API std::wstring UTF16ToWideAndAdjustOffset(const string16& utf16,
                                                 size_t* offset_for_adjustment);
BASE_API std::wstring UTF16ToWideAndAdjustOffsets(
    const string16& utf16,
    std::vector<size_t>* offsets_for_adjustment);

// Limiting function callable by std::for_each which will replace any value
// which is equal to or greater than |limit| with npos.
template <typename T>
struct LimitOffset {
  explicit LimitOffset(size_t limit)
    : limit_(limit) {}

  void operator()(size_t& offset) {
    if (offset >= limit_)
      offset = T::npos;
  }

  size_t limit_;
};

// Adjustment function called by std::transform which will adjust any offset
// that occurs after one or more modified substrings. To use, create any
// number of AdjustOffset::Adjustments, drop them into a vector, then call
// std::transform with the transform function being something similar to
// AdjustOffset(adjustments). Each Adjustment gives the original |location|
// of the encoded section and the |old_length| and |new_length| of the section
// before and after decoding.
struct AdjustOffset {
  // Helper structure which indicates where an encoded character occurred
  // and how long that encoding was.
  struct Adjustment {
    Adjustment(size_t location, size_t old_length, size_t new_length);

    size_t location;
    size_t old_length;
    size_t new_length;
  };

  typedef std::vector<Adjustment> Adjustments;

  explicit AdjustOffset(const Adjustments& adjustments);
  void operator()(size_t& offset);

  const Adjustments& adjustments_;
};

#endif  // BASE_UTF_OFFSET_STRING_CONVERSIONS_H_
