// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/case_conversion.h"

#include "unicode/unistr.h"

namespace base {
namespace i18n {

string16 ToLower(const string16& string) {
  icu::UnicodeString unicode_string(string.c_str(), string.size());
  unicode_string.toLower();
  return string16(unicode_string.getBuffer(), unicode_string.length());
}

string16 ToUpper(const string16& string) {
  icu::UnicodeString unicode_string(string.c_str(), string.size());
  unicode_string.toUpper();
  return string16(unicode_string.getBuffer(), unicode_string.length());
}

}  // namespace i18n
}  // namespace base
