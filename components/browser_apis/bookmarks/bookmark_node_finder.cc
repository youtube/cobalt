// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/bookmark_node_finder.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace bookmarks_api {

BookmarkNodeFinder::BookmarkNodeFinder(bookmarks::BookmarkModel* model)
    : model_(model) {
  CHECK(model_);
}

BookmarkNodeFinder::~BookmarkNodeFinder() = default;

std::optional<const bookmarks::BookmarkNode*>
BookmarkNodeFinder::FindNodeByUuid(const base::Uuid& uuid) const {
  const bookmarks::BookmarkNode* node = model_->GetNodeByUuid(
      uuid, bookmarks::BookmarkModel::NodeTypeForUuidLookup::kAccountNodes);
  if (!node) {
    node = model_->GetNodeByUuid(
        uuid,
        bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);
  }

  if (!node) {
    return std::nullopt;
  }
  return node;
}

}  // namespace bookmarks_api
