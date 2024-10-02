// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_
#define COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_

#include "base/feature_list.h"

namespace switches {

BASE_DECLARE_FEATURE(kSyncOmitLargeBookmarkFaviconUrl);

// TODO(crbug.com/1232951): remove the feature toggle once most of bookmarks
// have been reuploaded.
BASE_DECLARE_FEATURE(kSyncReuploadBookmarks);

}  // namespace switches

#endif  // COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_
