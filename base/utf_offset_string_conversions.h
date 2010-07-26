// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UTF_OFFSET_STRING_CONVERSIONS_H_
#define BASE_UTF_OFFSET_STRING_CONVERSIONS_H_
#pragma once

#include <string>

#include "base/string16.h"

namespace base {
class StringPiece;
}

// Like the conversions in utf_string_conversions.h, but also take offsets into
// the source strings, which will be adjusted to point at the same logical place
// in the result strings.  If this isn't possible because the offsets point past
// the end of the source strings or into the middle of multibyte sequences, they
// will be set to std::wstring::npos.  |offset_for_adjustment| may be NULL.
bool UTF8ToWideAndAdjustOffset(const char* src,
                               size_t src_len,
                               std::wstring* output,
                               size_t* offset_for_adjustment);
std::wstring UTF8ToWideAndAdjustOffset(const base::StringPiece& utf8,
                                       size_t* offset_for_adjustment);

bool UTF16ToWideAndAdjustOffset(const char16* src,
                                size_t src_len,
                                std::wstring* output,
                                size_t* offset_for_adjustment);
std::wstring UTF16ToWideAndAdjustOffset(const string16& utf16,
                                        size_t* offset_for_adjustment);

#endif  // BASE_UTF_OFFSET_STRING_CONVERSIONS_H_
