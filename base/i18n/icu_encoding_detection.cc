// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icu_encoding_detection.h"

#include "base/string_util.h"
#include "unicode/ucsdet.h"

namespace base {

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
  const char* detected_encoding = ucsdet_getName(match, &status);
  ucsdet_close(detector);

  if (U_FAILURE(status))
    return false;

  *encoding = detected_encoding;
  return true;
}

bool DetectAllEncodings(const std::string& text,
                        std::vector<std::string>* encodings) {
  UErrorCode status = U_ZERO_ERROR;
  UCharsetDetector* detector = ucsdet_open(&status);
  ucsdet_setText(detector, text.data(), static_cast<int32_t>(text.length()),
                 &status);
  int matches_count = 0;
  const UCharsetMatch** matches = ucsdet_detectAll(detector,
                                                   &matches_count,
                                                   &status);
  if (U_FAILURE(status)) {
    ucsdet_close(detector);
    return false;
  }

  encodings->clear();
  for (int i = 0; i < matches_count; i++) {
    UErrorCode get_name_status = U_ZERO_ERROR;
    const char* encoding_name = ucsdet_getName(matches[i], &get_name_status);

    // If we failed to get the encoding's name, ignore the error.
    if (U_FAILURE(get_name_status))
      continue;

    encodings->push_back(encoding_name);
  }

  ucsdet_close(detector);
  return !encodings->empty();
}

}  // namespace base
