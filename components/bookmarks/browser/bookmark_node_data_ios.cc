// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node_data.h"

#include "base/notreached.h"

namespace bookmarks {

// static
bool BookmarkNodeData::ClipboardContainsBookmarks() {
  NOTREACHED();
  return false;
}

void BookmarkNodeData::WriteToClipboard() {
  NOTREACHED();
}

bool BookmarkNodeData::ReadFromClipboard(ui::ClipboardBuffer buffer) {
  NOTREACHED();
  return false;
}

}  // namespace bookmarks
