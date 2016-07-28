// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/string_search.h"
#include "base/logging.h"

#include "unicode/usearch.h"

namespace {

bool CollationSensitiveStringSearch(const string16& find_this,
                                    const string16& in_this,
                                    UCollationStrength strength,
                                    size_t* match_index,
                                    size_t* match_length) {
  UErrorCode status = U_ZERO_ERROR;

  UStringSearch* search = usearch_open(find_this.data(), -1,
                                       in_this.data(), -1,
                                       uloc_getDefault(),
                                       NULL,  // breakiter
                                       &status);

  // Default to basic substring search if usearch fails. According to
  // http://icu-project.org/apiref/icu4c/usearch_8h.html, usearch_open will fail
  // if either |find_this| or |in_this| are empty. In either case basic
  // substring search will give the correct return value.
  if (!U_SUCCESS(status)) {
    size_t index = in_this.find(find_this);
    if (index == string16::npos) {
      return false;
    } else {
      if (match_index)
        *match_index = index;
      if (match_length)
        *match_length = find_this.size();
      return true;
    }
  }

  UCollator* collator = usearch_getCollator(search);
  ucol_setStrength(collator, strength);
  usearch_reset(search);

  int32_t index = usearch_first(search, &status);
  if (!U_SUCCESS(status) || index == USEARCH_DONE) {
    usearch_close(search);
    return false;
  }

  if (match_index)
    *match_index = static_cast<size_t>(index);
  if (match_length)
    *match_length = static_cast<size_t>(usearch_getMatchedLength(search));

  usearch_close(search);
  return true;
}

}  // namespace

namespace base {
namespace i18n {

bool StringSearchIgnoringCaseAndAccents(const string16& find_this,
                                        const string16& in_this,
                                        size_t* match_index,
                                        size_t* match_length) {
  return CollationSensitiveStringSearch(find_this,
                                        in_this,
                                        UCOL_PRIMARY,
                                        match_index,
                                        match_length);
}

}  // namespace i18n
}  // namespace base
