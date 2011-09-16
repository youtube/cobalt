// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unicode/usearch.h>

#include "base/i18n/string_search.h"

namespace {

bool CollationSensitiveStringSearch(const string16& find_this,
                                    const string16& in_this,
                                    UCollationStrength strength) {
  UErrorCode status = U_ZERO_ERROR;

  UStringSearch* search = usearch_open(find_this.data(), -1, in_this.data(), -1,
                                       uloc_getDefault(), NULL, &status);

  // Default to basic substring search if usearch fails. According to
  // http://icu-project.org/apiref/icu4c/usearch_8h.html, usearch_open will fail
  // if either |find_this| or |in_this| are empty. In either case basic
  // substring search will give the correct return value.
  if (!U_SUCCESS(status))
    return in_this.find(find_this) != string16::npos;

  UCollator* collator = usearch_getCollator(search);
  ucol_setStrength(collator, strength);
  usearch_reset(search);

  return usearch_first(search, &status) != USEARCH_DONE;
}

}  // namespace

namespace base {
namespace i18n {

bool StringSearchIgnoringCaseAndAccents(const string16& find_this,
                                        const string16& in_this) {
  return CollationSensitiveStringSearch(find_this, in_this, UCOL_PRIMARY);
}

}  // namespace i18n
}  // namespace base

