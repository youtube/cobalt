// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icu_encoding_detection.h"

#include "base/string_util.h"
#include "unicode/ucsdet.h"

namespace base {

// TODO(jungshik): We can apply more heuristics here (e.g. using various hints
// like TLD, the UI language/default encoding of a client, etc).
bool DetectEncoding(const std::string& text, std::string* encoding) {
  if (IsStringASCII(text)) {
    *encoding = std::string();
    return true;
  }

  UErrorCode status = U_ZERO_ERROR;
  UCharsetDetector* detector = ucsdet_open(&status);
  ucsdet_setText(detector, text.data(), static_cast<int32_t>(text.length()),
                 &status);
  // TODO(jungshik): Should we check the quality of the match? A rather
  // arbitrary number is assigned by ICU and it's hard to come up with
  // a lower limit.
  const UCharsetMatch* match = ucsdet_detect(detector, &status);
  const char* detected_encoding = ucsdet_getName(match, &status);
  ucsdet_close(detector);

  if (U_FAILURE(status))
    return false;

  *encoding = detected_encoding;
  return true;
}

}  // namespace base
