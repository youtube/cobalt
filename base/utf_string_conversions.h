// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UTF_STRING_CONVERSIONS_H_
#define BASE_UTF_STRING_CONVERSIONS_H_

#include <string>

#include "base/string16.h"
#include "base/string_piece.h"

// Like the conversions below, but also takes an offset into the source string,
// which will be adjusted to point at the same logical place in the result
// string.  If this isn't possible because it points past the end of the source
// string or into the middle of a multibyte sequence, it will be set to
// std::wstring::npos.  |offset_for_adjustment| may be NULL.
bool WideToUTF8AndAdjustOffset(const wchar_t* src,
                               size_t src_len,
                               std::string* output,
                               size_t* offset_for_adjustment);
std::string WideToUTF8AndAdjustOffset(const std::wstring& wide,
                                      size_t* offset_for_adjustment);
bool UTF8ToWideAndAdjustOffset(const char* src,
                               size_t src_len,
                               std::wstring* output,
                               size_t* offset_for_adjustment);
std::wstring UTF8ToWideAndAdjustOffset(const base::StringPiece& utf8,
                                       size_t* offset_for_adjustment);

bool WideToUTF16AndAdjustOffset(const wchar_t* src,
                                size_t src_len,
                                string16* output,
                                size_t* offset_for_adjustment);
string16 WideToUTF16AndAdjustOffset(const std::wstring& wide,
                                    size_t* offset_for_adjustment);
bool UTF16ToWideAndAdjustOffset(const char16* src,
                                size_t src_len,
                                std::wstring* output,
                                size_t* offset_for_adjustment);
std::wstring UTF16ToWideAndAdjustOffset(const string16& utf16,
                                        size_t* offset_for_adjustment);

// These convert between UTF-8, -16, and -32 strings. They are potentially slow,
// so avoid unnecessary conversions. The low-level versions return a boolean
// indicating whether the conversion was 100% valid. In this case, it will still
// do the best it can and put the result in the output buffer. The versions that
// return strings ignore this error and just return the best conversion
// possible.
//
// Note that only the structural validity is checked and non-character
// codepoints and unassigned are regarded as valid.
// TODO(jungshik): Consider replacing an invalid input sequence with
// the Unicode replacement character or adding |replacement_char| parameter.
// Currently, it's skipped in the ouput, which could be problematic in
// some situations.
inline bool WideToUTF8(const wchar_t* src,
                       size_t src_len,
                       std::string* output) {
  return WideToUTF8AndAdjustOffset(src, src_len, output, NULL);
}
inline std::string WideToUTF8(const std::wstring& wide) {
  return WideToUTF8AndAdjustOffset(wide, NULL);
}
inline bool UTF8ToWide(const char* src, size_t src_len, std::wstring* output) {
  return UTF8ToWideAndAdjustOffset(src, src_len, output, NULL);
}
inline std::wstring UTF8ToWide(const base::StringPiece& utf8) {
  return UTF8ToWideAndAdjustOffset(utf8, NULL);
}

inline bool WideToUTF16(const wchar_t* src, size_t src_len, string16* output) {
  return WideToUTF16AndAdjustOffset(src, src_len, output, NULL);
}
inline string16 WideToUTF16(const std::wstring& wide) {
  return WideToUTF16AndAdjustOffset(wide, NULL);
}
inline bool UTF16ToWide(const char16* src, size_t src_len,
                        std::wstring* output) {
  return UTF16ToWideAndAdjustOffset(src, src_len, output, NULL);
}
inline std::wstring UTF16ToWide(const string16& utf16) {
  return UTF16ToWideAndAdjustOffset(utf16, NULL);
}

bool UTF8ToUTF16(const char* src, size_t src_len, string16* output);
string16 UTF8ToUTF16(const std::string& utf8);
bool UTF16ToUTF8(const char16* src, size_t src_len, std::string* output);
std::string UTF16ToUTF8(const string16& utf16);

// We are trying to get rid of wstring as much as possible, but it's too big
// a mess to do it all at once.  These conversions should be used when we
// really should just be passing a string16 around, but we haven't finished
// porting whatever module uses wstring and the conversion is being used as a
// stopcock.  This makes it easy to grep for the ones that should be removed.
#if defined(OS_WIN)
# define WideToUTF16Hack
# define UTF16ToWideHack
#else
# define WideToUTF16Hack WideToUTF16
# define UTF16ToWideHack UTF16ToWide
#endif

#endif  // BASE_UTF_STRING_CONVERSIONS_H_
