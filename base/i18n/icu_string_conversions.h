// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ICU_STRING_CONVERSIONS_H_
#define BASE_I18N_ICU_STRING_CONVERSIONS_H_

#include <string>

#include "base/string16.h"
#include "base/string_piece.h"

namespace base {

// Defines the error handling modes of UTF16ToCodepage, CodepageToUTF16,
// WideToCodepage and CodepageToWide.
class OnStringConversionError {
 public:
  enum Type {
    // The function will return failure. The output buffer will be empty.
    FAIL,

    // The offending characters are skipped and the conversion will proceed as
    // if they did not exist.
    SKIP,

    // When converting to Unicode, the offending byte sequences are substituted
    // by Unicode replacement character (U+FFFD). When converting from Unicode,
    // this is the same as SKIP.
    SUBSTITUTE,
  };

 private:
  OnStringConversionError();
};

// Names of codepages (charsets) understood by icu.
extern const char kCodepageLatin1[];  // a.k.a. ISO 8859-1
extern const char kCodepageUTF8[];
extern const char kCodepageUTF16BE[];
extern const char kCodepageUTF16LE[];

// Like CodepageToUTF16() (see below), but also takes an offset into |encoded|,
// which will be adjusted to point at the same logical place in |utf16|.  If
// this isn't possible because it points past the end of |encoded| or into the
// middle of a multibyte sequence, it will be set to std::string16::npos.
// |offset_for_adjustment| may be NULL.
bool CodepageToUTF16AndAdjustOffset(const std::string& encoded,
                                    const char* codepage_name,
                                    OnStringConversionError::Type on_error,
                                    string16* utf16,
                                    size_t* offset_for_adjustment);

// Converts between UTF-16 strings and the encoding specified.  If the
// encoding doesn't exist or the encoding fails (when on_error is FAIL),
// returns false.
bool UTF16ToCodepage(const string16& utf16,
                     const char* codepage_name,
                     OnStringConversionError::Type on_error,
                     std::string* encoded);
inline bool CodepageToUTF16(const std::string& encoded,
                            const char* codepage_name,
                            OnStringConversionError::Type on_error,
                            string16* utf16) {
  return CodepageToUTF16AndAdjustOffset(encoded, codepage_name, on_error, utf16,
                                        NULL);
}

// Like CodepageToWide() (see below), but also takes an offset into |encoded|,
// which will be adjusted to point at the same logical place in |wide|.  If
// this isn't possible because it points past the end of |encoded| or into the
// middle of a multibyte sequence, it will be set to std::wstring::npos.
// |offset_for_adjustment| may be NULL.
bool CodepageToWideAndAdjustOffset(const std::string& encoded,
                                   const char* codepage_name,
                                   OnStringConversionError::Type on_error,
                                   std::wstring* wide,
                                   size_t* offset_for_adjustment);

// Converts between wide strings and the encoding specified.  If the
// encoding doesn't exist or the encoding fails (when on_error is FAIL),
// returns false.
bool WideToCodepage(const std::wstring& wide,
                    const char* codepage_name,
                    OnStringConversionError::Type on_error,
                    std::string* encoded);
inline bool CodepageToWide(const std::string& encoded,
                           const char* codepage_name,
                           OnStringConversionError::Type on_error,
                           std::wstring* wide) {
  return CodepageToWideAndAdjustOffset(encoded, codepage_name, on_error, wide,
                                       NULL);
}

}  // namespace base

#endif  // BASE_I18N_ICU_STRING_CONVERSIONS_H_
