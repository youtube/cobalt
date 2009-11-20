// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/number_formatting.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "unicode/numfmt.h"
#include "unicode/ustring.h"

namespace base {

namespace {

struct NumberFormatSingletonTraits
    : public DefaultSingletonTraits<icu::NumberFormat> {
  static icu::NumberFormat* New() {
    UErrorCode status = U_ZERO_ERROR;
    icu::NumberFormat* formatter = icu::NumberFormat::createInstance(status);
    DCHECK(U_SUCCESS(status));
    return formatter;
  }
  // There's no ICU call to destroy a NumberFormat object other than
  // operator delete, so use the default Delete, which calls operator delete.
  // This can cause problems if a different allocator is used by this file than
  // by ICU.
};

}  // namespace

string16 FormatNumber(int64 number) {
  icu::NumberFormat* number_format =
      Singleton<icu::NumberFormat, NumberFormatSingletonTraits>::get();

  if (!number_format) {
    // As a fallback, just return the raw number in a string.
    return UTF8ToUTF16(StringPrintf("%" PRId64, number));
  }
  icu::UnicodeString ustr;
  number_format->format(number, ustr);

  return string16(ustr.getBuffer(), static_cast<size_t>(ustr.length()));
}

}  // namespace base
