// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/bookmark_event_translator.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace bookmarks_api {

BookmarkEventTranslator::BookmarkEventTranslator(
    bookmarks::BookmarkModel* model,
    Subscriber* subscriber)
    : model_(model), subscriber_(subscriber) {
  CHECK(model_);
  CHECK(subscriber_);
  CHECK(model_->loaded());
  model_->AddObserver(this);
  RefreshFoldersSnapshot();
}

BookmarkEventTranslator::~BookmarkEventTranslator() {
  model_->RemoveObserver(this);
}

// static
mojom::BookmarkNodePtr BookmarkEventTranslator::ConvertNode(
    const bookmarks::BookmarkNode* node) {
  switch (node->type()) {
    case bookmarks::BookmarkNode::URL: {
      auto url_node = mojom::Url::New();
      url_node->id = node->uuid();
      url_node->title = base::UTF16ToUTF8(node->GetTitle());
      url_node->url = node->url();
      if (node->icon_url()) {
        url_node->favicon_url = *node->icon_url();
      }
      return mojom::BookmarkNode::NewUrl(std::move(url_node));
    }
    case bookmarks::BookmarkNode::FOLDER:
    case bookmarks::BookmarkNode::BOOKMARK_BAR:
    case bookmarks::BookmarkNode::OTHER_NODE:
    case bookmarks::BookmarkNode::MOBILE: {
      auto folder_node = mojom::Folder::New();
      folder_node->id = node->uuid();
      folder_node->title = base::UTF16ToUTF8(node->GetTitle());
      for (const auto& child : node->children()) {
        folder_node->children.push_back(ConvertNode(child.get()));
      }
      return mojom::BookmarkNode::NewFolder(std::move(folder_node));
    }
  }
}

void BookmarkEventTranslator::BookmarkNodeMoved(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  auto moved_event = mojom::BookmarkNodeMoved::New(
      old_parent->uuid(), static_cast<int32_t>(old_index), new_parent->uuid(),
      static_cast<int32_t>(new_index));

  std::vector<mojom::BookmarksEventPtr> events;
  events.push_back(mojom::BookmarksEvent::NewMoved(std::move(moved_event)));

  RefreshFoldersSnapshot();

  Notify(events);
}

void BookmarkEventTranslator::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  const bookmarks::BookmarkNode* node = parent->children()[index].get();
  auto added_event = mojom::BookmarkNodeCreated::New(
      parent->uuid(), static_cast<int32_t>(index), ConvertNode(node));

  std::vector<mojom::BookmarksEventPtr> events;
  events.push_back(mojom::BookmarksEvent::NewAdded(std::move(added_event)));

  RefreshFoldersSnapshot();

  Notify(events);
}

void BookmarkEventTranslator::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& no_longer_bookmarked,
    const base::Location& location) {
  auto removed_event = mojom::BookmarkNodeRemoved::New(node->uuid());

  std::vector<mojom::BookmarksEventPtr> events;
  events.push_back(mojom::BookmarksEvent::NewRemoved(std::move(removed_event)));

  RefreshFoldersSnapshot();

  Notify(events);
}

void BookmarkEventTranslator::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  auto changed_event = mojom::BookmarkNodeChanged::New(ConvertNode(node));

  std::vector<mojom::BookmarksEventPtr> events;
  events.push_back(mojom::BookmarksEvent::NewChanged(std::move(changed_event)));
  Notify(events);
}

void BookmarkEventTranslator::BookmarkNodeChildrenReordered(
    const bookmarks::BookmarkNode* node) {
  std::vector<mojom::BookmarksEventPtr> events;

  // Shuffle forward algorithm. We start at index 0 then progress forward. For
  // any node that isn't where it's supposed to be in the new order, we produce
  // a "shuffle forward" event. This prevents clients from needing to reorder
  // elements that have already been moved.
  const auto& new_ordering = node->children();
  auto current_view = folders_snapshot_[node->uuid()];
  for (size_t i = 0; i < new_ordering.size(); ++i) {
    const auto& target = new_ordering[i];
    auto it =
        std::find(current_view.begin(), current_view.end(), target->uuid());

    CHECK(it != current_view.end())
        << "a reordered node could not be found in the current folder "
           "snapshot, this should never happen";

    size_t current_index = std::distance(current_view.begin(), it);
    // If the index does not match the expected index.
    if (current_index != i) {
      // Shuffle forward.
      current_view.erase(it);
      current_view.insert(current_view.begin() + i, target->uuid());

      auto moved_event = mojom::BookmarkNodeMoved::New(
          node->uuid(), static_cast<int32_t>(current_index), node->uuid(),
          static_cast<int32_t>(i));
      events.push_back(mojom::BookmarksEvent::NewMoved(std::move(moved_event)));
    }
  }

  RefreshFoldersSnapshot();

  Notify(events);
}

void BookmarkEventTranslator::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  std::vector<mojom::BookmarksEventPtr> events;

  std::vector<const bookmarks::BookmarkNode*> permanent_nodes = {
      model_->bookmark_bar_node(), model_->other_node(), model_->mobile_node()};

  for (const auto* permanent_node : permanent_nodes) {
    if (!permanent_node) {
      continue;
    }
    auto it = folders_snapshot_.find(permanent_node->uuid());
    if (it != folders_snapshot_.end()) {
      for (const auto& child_uuid : it->second) {
        auto removed_event = mojom::BookmarkNodeRemoved::New(child_uuid);
        events.push_back(
            mojom::BookmarksEvent::NewRemoved(std::move(removed_event)));
      }
    }
  }

  // Reset snapshot.
  RefreshFoldersSnapshot();

  Notify(events);
}

void BookmarkEventTranslator::RefreshFoldersSnapshot() {
  folders_snapshot_.clear();
  PopulateFoldersSnapshot(model_->root_node());
}

void BookmarkEventTranslator::PopulateFoldersSnapshot(
    const bookmarks::BookmarkNode* node) {
  if (node->is_folder()) {
    std::vector<base::Uuid> children;
    for (const auto& child : node->children()) {
      children.push_back(child->uuid());
      PopulateFoldersSnapshot(child.get());
    }
    folders_snapshot_[node->uuid()] = std::move(children);
  }
}

void BookmarkEventTranslator::Notify(
    const std::vector<mojom::BookmarksEventPtr>& events) {
  if (events.empty()) {
    return;
  }
  subscriber_->OnBookmarkEvents(events);
}

}  // namespace bookmarks_api
