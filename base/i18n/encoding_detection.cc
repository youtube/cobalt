// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/encoding_detection.h"
#include "build/build_config.h"

#if defined(STARBOARD)
#include "base/strings/string_util.h"
#include "unicode/ucsdet.h"
#else

#include "third_party/ced/src/compact_enc_det/compact_enc_det.h"
#endif

// third_party/ced/src/util/encodings/encodings.h, which is included
// by the include above, undefs UNICODE because that is a macro used
// internally in ced. If we later in the same translation unit do
// anything related to Windows or Windows headers those will then use
// the ASCII versions which we do not want. To avoid that happening in
// jumbo builds, we redefine UNICODE again here.
#if defined(OS_WIN)
#define UNICODE 1
#endif  // OS_WIN

namespace base {

#if defined(STARBOARD)
bool DetectEncoding(const std::string& text, std::string* encoding) {
  if (IsStringASCII(text)) {
    *encoding = std::string();
    return true;
  }

  UErrorCode status = U_ZERO_ERROR;
  UCharsetDetector* detector = ucsdet_open(&status);
  ucsdet_setText(detector, text.data(), static_cast<int32_t>(text.length()),
                 &status);
  const UCharsetMatch* match = ucsdet_detect(detector, &status);
  if (match == NULL)
    return false;
  const char* detected_encoding = ucsdet_getName(match, &status);
  ucsdet_close(detector);

  if (U_FAILURE(status))
    return false;

  *encoding = detected_encoding;
  return true;
}
#else
bool DetectEncoding(const std::string& text, std::string* encoding) {
  int consumed_bytes;
  bool is_reliable;
  Encoding enc = CompactEncDet::DetectEncoding(
      text.c_str(), text.length(), nullptr, nullptr, nullptr,
      UNKNOWN_ENCODING,
      UNKNOWN_LANGUAGE,
      CompactEncDet::QUERY_CORPUS,  // plain text
      false,  // Include 7-bit encodings
      &consumed_bytes,
      &is_reliable);

  if (enc == UNKNOWN_ENCODING)
    return false;

  *encoding = MimeEncodingName(enc);
  return true;
}
#endif
}  // namespace base
