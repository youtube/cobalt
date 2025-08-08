// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/switches.h"

#include "base/feature_list.h"

namespace switches {

BASE_FEATURE(kSyncOmitLargeBookmarkFaviconUrl,
             "SyncOmitLargeBookmarkFaviconUrl",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/1232951): remove the feature toggle once most of bookmarks
// have been reuploaded.
BASE_FEATURE(kSyncReuploadBookmarks,
             "SyncReuploadBookmarks",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace switches
