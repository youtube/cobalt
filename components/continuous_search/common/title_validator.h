// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTINUOUS_SEARCH_COMMON_TITLE_VALIDATOR_H_
#define COMPONENTS_CONTINUOUS_SEARCH_COMMON_TITLE_VALIDATOR_H_

#include "base/strings/string_piece.h"

namespace continuous_search {

// These methods:
// 1. Replace control characters with whitespace.
// 2. Trim leading and trailing whitespace.
// 3. Collapse internal whitespace to a single space.
// 4. Cap the length of the output at `kMaxLength`.
std::string ValidateTitleAscii(base::StringPiece title);
std::u16string ValidateTitle(base::StringPiece16 title);

}  // namespace continuous_search

#endif  // COMPONENTS_CONTINUOUS_SEARCH_COMMON_TITLE_VALIDATOR_H_
