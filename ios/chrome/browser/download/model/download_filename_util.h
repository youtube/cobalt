// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_FILENAME_UTIL_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_FILENAME_UTIL_H_

#include <string>
#include <string_view>

namespace download_model {

// Returns the search-friendly normalized form of a UTF-8 file name. The
// transform is a performance pre-filter only: a substring match on the
// normalized form returns a superset of what
// `base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents` would match.
// Every caller that uses this helper to short-circuit a match MUST also
// re-verify each survivor with `FixedPatternStringSearchIgnoringCaseAndAccents`
// to guarantee semantic parity (no false positives, no behavior drift vs. the
// ICU search if this NFD+stripMn+FoldCase approximation diverges from it).
//
// Pipeline:
//   1. NFD-decompose so accented base letters split into (letter, combining
//      mark) pairs.
//   2. Drop characters in category Mn (Non-Spacing Mark), i.e. combining
//      diacritics.
//   3. FoldCase for locale-independent case-insensitive comparison.
// Both the stored DB column (`file_name_normalized`) and the user's query
// string are run through this helper so they meet on the same normalized
// form.
std::string NormalizeFileName(std::string_view file_name);

}  // namespace download_model

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_FILENAME_UTIL_H_
