// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARKS_SERVICE_IMPL_H_
#define COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARKS_SERVICE_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/browser_apis/bookmarks/bookmark_event_translator.h"
#include "components/browser_apis/bookmarks/bookmark_node_finder.h"
#include "components/browser_apis/bookmarks/bookmarks_service.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace bookmarks_api {

class BookmarksServiceImpl : public BookmarksService,
                             public BookmarkEventTranslator::Subscriber {
 public:
  explicit BookmarksServiceImpl(bookmarks::BookmarkModel* bookmark_model);
  BookmarksServiceImpl(const BookmarksServiceImpl&) = delete;
  BookmarksServiceImpl& operator=(const BookmarksServiceImpl&) = delete;
  ~BookmarksServiceImpl() override;

  // BookmarksService:
  void Accept(mojo::PendingReceiver<mojom::BookmarksService> receiver) override;

  // mojom::BookmarksServiceDirectReturnStub:
  mojom::BookmarksService::GetBookmarksResult GetBookmarks() override;
  mojom::BookmarksService::GetBookmarkResult GetBookmark(
      const base::Uuid& id) override;
  mojom::BookmarksService::CreateBookmarkNodeResult CreateBookmarkNode(
      const base::Uuid& parent_id,
      std::optional<int32_t> index,
      mojom::BookmarkNodePtr node) override;
  mojom::BookmarksService::UpdateBookmarkNodeResult UpdateBookmarkNode(
      mojom::BookmarkNodePtr node) override;
  mojom::BookmarksService::MoveBookmarkNodeResult MoveBookmarkNode(
      const base::Uuid& id,
      const base::Uuid& new_parent_id,
      std::optional<int32_t> index) override;
  mojom::BookmarksService::DeleteBookmarkNodeResult DeleteBookmarkNode(
      const base::Uuid& id) override;

 private:
  mojom::BookmarkNodePtr ConvertNode(const bookmarks::BookmarkNode* node);

  // BookmarkEventTranslator::Subscriber:
  void OnBookmarkEvents(
      const std::vector<mojom::BookmarksEventPtr>& events) override;

  void BroadcastEvents(const std::vector<mojom::BookmarksEventPtr>& events);

  mojom::BookmarksServiceBridge bridge_{this};

  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  BookmarkNodeFinder finder_;
  mojo::ReceiverSet<mojom::BookmarksService> receivers_;
  mojo::AssociatedRemoteSet<mojom::BookmarksObserver> observers_;

  std::unique_ptr<BookmarkEventTranslator> translator_;
};

}  // namespace bookmarks_api

#endif  // COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARKS_SERVICE_IMPL_H_
