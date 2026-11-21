// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/bookmark_event_translator.h"

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks_api {

class BookmarkEventTranslatorTest : public testing::Test,
                                    public BookmarkEventTranslator::Subscriber {
 public:
  BookmarkEventTranslatorTest() {
    model_ = bookmarks::TestBookmarkClient::CreateModel();
    translator_ = std::make_unique<BookmarkEventTranslator>(
        model_.get(), /*managed=*/nullptr, this);
  }

  // BookmarkEventTranslator::Subscriber:
  void OnBookmarkEvents(
      const std::vector<mojom::BookmarksEventPtr>& events) override {
    for (const auto& event : events) {
      events_.push_back(event.Clone());
    }
  }

  void ClearEvents() { events_.clear(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  std::unique_ptr<BookmarkEventTranslator> translator_;
  std::vector<mojom::BookmarksEventPtr> events_;
};

TEST_F(BookmarkEventTranslatorTest, AddBookmark) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_added());
  const auto& added = events_[0]->get_added();
  ASSERT_EQ(added->parent_id, parent->uuid());
  ASSERT_EQ(added->index, 0);
  ASSERT_TRUE(added->node->is_url());
  ASSERT_TRUE(added->node->get_url()->id.has_value());
  ASSERT_EQ(added->node->get_url()->title, "Title");
  ASSERT_EQ(added->node->get_url()->url, GURL("http://example.com"));
}

TEST_F(BookmarkEventTranslatorTest, RemoveBookmark) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));
  base::Uuid node_uuid = node->uuid();
  ClearEvents();

  model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                 FROM_HERE);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_removed());
  ASSERT_EQ(events_[0]->get_removed()->id, node_uuid);
}

TEST_F(BookmarkEventTranslatorTest, MoveBookmark) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* other_parent = model_->other_node();
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));
  ClearEvents();

  model_->Move(node, other_parent, 0);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_moved());
  const auto& moved = events_[0]->get_moved();
  ASSERT_EQ(moved->old_parent_id, parent->uuid());
  ASSERT_EQ(moved->old_index, 0);
  ASSERT_EQ(moved->new_parent_id, other_parent->uuid());
  ASSERT_EQ(moved->new_index, 0);
}

TEST_F(BookmarkEventTranslatorTest, ChangeBookmark) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));
  base::Uuid node_uuid = node->uuid();
  ClearEvents();

  model_->SetTitle(node, u"New Title",
                   bookmarks::metrics::BookmarkEditSource::kUser);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_changed());
  const auto& changed = events_[0]->get_changed();
  ASSERT_TRUE(changed->node->is_url());
  ASSERT_EQ(changed->node->get_url()->id, node_uuid);
  ASSERT_EQ(changed->node->get_url()->title, "New Title");
}

TEST_F(BookmarkEventTranslatorTest, ReorderChildren) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* node1 =
      model_->AddURL(parent, 0, u"Title 1", GURL("http://example1.com"));
  const bookmarks::BookmarkNode* node2 =
      model_->AddURL(parent, 1, u"Title 2", GURL("http://example2.com"));
  ClearEvents();

  std::vector<const bookmarks::BookmarkNode*> new_order = {node2, node1};
  model_->ReorderChildren(parent, new_order);

  // Reordering [node1, node2] -> [node2, node1] should move node2 to index 0.
  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_moved());
  const auto& moved = events_[0]->get_moved();
  ASSERT_EQ(moved->old_parent_id, parent->uuid());
  ASSERT_EQ(moved->old_index, 1);
  ASSERT_EQ(moved->new_parent_id, parent->uuid());
  ASSERT_EQ(moved->new_index, 0);
}

TEST_F(BookmarkEventTranslatorTest, RemoveAllUserBookmarks) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* other_parent = model_->other_node();
  const bookmarks::BookmarkNode* node1 =
      model_->AddURL(parent, 0, u"Title 1", GURL("http://example1.com"));
  const bookmarks::BookmarkNode* node2 =
      model_->AddURL(other_parent, 0, u"Title 2", GURL("http://example2.com"));
  base::Uuid uuid1 = node1->uuid();
  base::Uuid uuid2 = node2->uuid();
  ClearEvents();

  model_->RemoveAllUserBookmarks(FROM_HERE);

  // Should get 2 removed events: node1 (bookmark_bar) and node2 (other).
  // Order should be bookmark_bar then other node.
  ASSERT_EQ(events_.size(), 2u);
  ASSERT_TRUE(events_[0]->is_removed());
  ASSERT_EQ(events_[0]->get_removed()->id, uuid1);
  ASSERT_TRUE(events_[1]->is_removed());
  ASSERT_EQ(events_[1]->get_removed()->id, uuid2);
}

}  // namespace bookmarks_api
