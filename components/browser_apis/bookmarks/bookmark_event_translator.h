// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_EVENT_TRANSLATOR_H_
#define COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_EVENT_TRANSLATOR_H_

#include <map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/browser_apis/bookmarks/bookmarks_api.mojom.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
class ManagedBookmarkService;
}  // namespace bookmarks

namespace bookmarks_api {

// Translates between BookmarkModelObserver to mojo based event types.
class BookmarkEventTranslator : public bookmarks::BookmarkModelObserver {
 public:
  class Subscriber {
   public:
    virtual void OnBookmarkEvents(
        const std::vector<mojom::BookmarksEventPtr>& events) = 0;

   protected:
    virtual ~Subscriber() = default;
  };

  BookmarkEventTranslator(bookmarks::BookmarkModel* model,
                          bookmarks::ManagedBookmarkService* managed,
                          Subscriber* subscriber);
  BookmarkEventTranslator(const BookmarkEventTranslator&) = delete;
  BookmarkEventTranslator& operator=(const BookmarkEventTranslator&) = delete;
  ~BookmarkEventTranslator() override;

  static mojom::BookmarkNodePtr ConvertNode(
      bookmarks::BookmarkModel* model,
      bookmarks::ManagedBookmarkService* managed,
      const bookmarks::BookmarkNode* node);

  static mojom::RootNodePtr ConvertRootNode(
      bookmarks::BookmarkModel* model,
      bookmarks::ManagedBookmarkService* managed,
      const bookmarks::BookmarkNode* node);

  static mojom::FolderPtr ConvertFolderNode(
      bookmarks::BookmarkModel* model,
      bookmarks::ManagedBookmarkService* managed,
      const bookmarks::BookmarkNode* node);

  // bookmarks::BookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override {}
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& no_longer_bookmarked,
                           const base::Location& location) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;
  void ExtensiveBookmarkChangesBeginning() override;
  void ExtensiveBookmarkChangesEnded() override;

 private:
  void RefreshFoldersSnapshot();
  void PopulateFoldersSnapshot(const bookmarks::BookmarkNode* node);
  void Notify(std::vector<mojom::BookmarksEventPtr> events);

  raw_ptr<bookmarks::BookmarkModel> model_;
  raw_ptr<bookmarks::ManagedBookmarkService> managed_;
  raw_ptr<Subscriber> subscriber_;
  // A snapshot of the folder structure (mapping folder UUID to its children's
  // UUIDs) used to detect changes (adds, removes, moves) in the bookmark model.
  // This is necessary because bookmark model has a "reorder" event type, which
  // performs several move operations at once. We need to keep an old snapshot
  // to compute individual move events.
  std::map<base::Uuid, std::vector<base::Uuid>> folders_snapshot_;

  std::vector<mojom::BookmarksEventPtr> queued_events_;
};

}  // namespace bookmarks_api

#endif  // COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_EVENT_TRANSLATOR_H_
