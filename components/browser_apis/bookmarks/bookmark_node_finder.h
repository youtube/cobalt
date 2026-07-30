// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_NODE_FINDER_H_
#define COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_NODE_FINDER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace bookmarks_api {

class BookmarkNodeFinder {
 public:
  explicit BookmarkNodeFinder(bookmarks::BookmarkModel* model);
  ~BookmarkNodeFinder();

  BookmarkNodeFinder(const BookmarkNodeFinder&) = delete;
  BookmarkNodeFinder& operator=(const BookmarkNodeFinder&) = delete;

  // Finds a bookmark node by UUID.
  // Returns std::nullopt if the node is not found.
  std::optional<const bookmarks::BookmarkNode*> FindNodeByUuid(
      const base::Uuid& uuid) const;

 private:
  raw_ptr<bookmarks::BookmarkModel> model_;
};

}  // namespace bookmarks_api

#endif  // COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_NODE_FINDER_H_
