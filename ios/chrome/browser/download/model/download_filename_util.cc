// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/model/download_filename_util.h"

#include "third_party/icu/source/common/unicode/normalizer2.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"

namespace download_model {

std::string NormalizeFileName(std::string_view file_name) {
  if (file_name.empty()) {
    return std::string();
  }
  icu::UnicodeString u16 = icu::UnicodeString::fromUTF8(file_name);
  UErrorCode status = U_ZERO_ERROR;
  const icu::Normalizer2* nfd = icu::Normalizer2::getNFDInstance(status);
  if (U_SUCCESS(status) && nfd) {
    icu::UnicodeString decomposed = nfd->normalize(u16, status);
    if (U_SUCCESS(status)) {
      u16 = decomposed;
    }
  }
  // Strip Non-Spacing Marks (Mn) in place.
  icu::UnicodeString stripped;
  for (int32_t i = 0; i < u16.length();) {
    UChar32 cp = u16.char32At(i);
    int32_t cp_len = U16_LENGTH(cp);
    if (u_charType(cp) != U_NON_SPACING_MARK) {
      stripped.append(cp);
    }
    i += cp_len;
  }
  // Case-fold in place via ICU directly to avoid extra UTF-16/UTF-8
  // round-trips through `base::i18n::FoldCase`.
  stripped.foldCase();
  std::string utf8;
  stripped.toUTF8String(utf8);
  return utf8;
}

}  // namespace download_model
