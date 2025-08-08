// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_VIEW_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_VIEW_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkModelObserver;
}  // namespace bookmarks

namespace sync_bookmarks {

// A sync-specific abstraction mimic-ing the API in BookmarkModel that allows
// exposing the minimal API surface required for sync and customizing how local
// permanent folders map to server-side permanent folders.
class BookmarkModelView {
 public:
  // `bookmark_model` must not be null and must outlive this object.
  explicit BookmarkModelView(bookmarks::BookmarkModel* bookmark_model);
  BookmarkModelView(const BookmarkModelView&) = delete;
  virtual ~BookmarkModelView();

  BookmarkModelView& operator=(const BookmarkModelView&) = delete;

  // Returns whether `node` is actually relevant in the context of this view,
  // which allows filtering which subset of bookmarks should be sync-ed. Note
  // that some other APIs, such as traversing root(), can expose nodes that are
  // NOT meant to be sync-ed, hence the need for this predicate.
  bool IsNodeSyncable(const bookmarks::BookmarkNode* node) const;

  // Functions that allow influencing which bookmark tree is exposed to sync.
  virtual const bookmarks::BookmarkNode* bookmark_bar_node() const = 0;
  virtual const bookmarks::BookmarkNode* other_node() const = 0;
  virtual const bookmarks::BookmarkNode* mobile_node() const = 0;

  // See bookmarks::BookmarkModel for documentation, as all functions below
  // mimic the same API.
  bool loaded() const;
  const bookmarks::BookmarkNode* root_node() const;
  bool is_permanent_node(const bookmarks::BookmarkNode* node) const;
  void AddObserver(bookmarks::BookmarkModelObserver* observer);
  void RemoveObserver(bookmarks::BookmarkModelObserver* observer);
  void BeginExtensiveChanges();
  void EndExtensiveChanges();
  void Remove(const bookmarks::BookmarkNode* node);
  void RemoveAllUserBookmarks();
  void Move(const bookmarks::BookmarkNode* node,
            const bookmarks::BookmarkNode* new_parent,
            size_t index);
  const gfx::Image& GetFavicon(const bookmarks::BookmarkNode* node);
  void SetTitle(const bookmarks::BookmarkNode* node,
                const std::u16string& title);
  void SetURL(const bookmarks::BookmarkNode* node, const GURL& url);
  const bookmarks::BookmarkNode* GetNodeByUuid(const base::Uuid& uuid) const;
  const bookmarks::BookmarkNode* AddFolder(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const bookmarks::BookmarkNode::MetaInfoMap* meta_info,
      absl::optional<base::Time> creation_time,
      absl::optional<base::Uuid> uuid);
  const bookmarks::BookmarkNode* AddURL(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const GURL& url,
      const bookmarks::BookmarkNode::MetaInfoMap* meta_info,
      absl::optional<base::Time> creation_time,
      absl::optional<base::Uuid> uuid);
  void ReorderChildren(
      const bookmarks::BookmarkNode* parent,
      const std::vector<const bookmarks::BookmarkNode*>& ordered_nodes);
  void UpdateLastUsedTime(const bookmarks::BookmarkNode* node,
                          const base::Time time,
                          bool just_opened);
  void SetNodeMetaInfoMap(
      const bookmarks::BookmarkNode* node,
      const bookmarks::BookmarkNode::MetaInfoMap& meta_info_map);

 protected:
  bookmarks::BookmarkModel* underlying_model() { return bookmark_model_.get(); }
  const bookmarks::BookmarkModel* underlying_model() const {
    return bookmark_model_.get();
  }

 private:
  // Using WeakPtr here allows detecting violations of the constructor
  // precondition and CHECK fail if BookmarkModel is destroyed earlier.
  // This also simplifies unit-testing, because using raw_ptr otherwise
  // complicates the way to achieve a reasonable destruction order for
  // TestBookmarkModelView.
  const base::WeakPtr<bookmarks::BookmarkModel> bookmark_model_;
};

class BookmarkModelViewUsingLocalOrSyncableNodes : public BookmarkModelView {
 public:
  // `bookmark_model` must not be null and must outlive this object.
  explicit BookmarkModelViewUsingLocalOrSyncableNodes(
      bookmarks::BookmarkModel* bookmark_model);
  ~BookmarkModelViewUsingLocalOrSyncableNodes() override;

  // BookmarkModelView overrides.
  const bookmarks::BookmarkNode* bookmark_bar_node() const override;
  const bookmarks::BookmarkNode* other_node() const override;
  const bookmarks::BookmarkNode* mobile_node() const override;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_VIEW_H_
