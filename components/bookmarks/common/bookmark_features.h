// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_FEATURES_H_
#define COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace bookmarks {

BASE_DECLARE_FEATURE(kRollbackBookmarksAccountStorage);
BASE_DECLARE_FEATURE(kAllBookmarksBaselineFolderVisibility);

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_FEATURES_H_
