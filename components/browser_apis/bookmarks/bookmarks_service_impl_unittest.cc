// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/bookmarks_service_impl.h"

#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks_api {

class BookmarksServiceImplTest : public testing::Test {
 protected:
  BookmarksServiceImplTest() {
    model_ = std::make_unique<bookmarks::BookmarkModel>(
        std::make_unique<bookmarks::TestBookmarkClient>());
    model_->LoadEmptyForTest();
    service_ = std::make_unique<BookmarksServiceImpl>(model_.get());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  std::unique_ptr<BookmarksServiceImpl> service_;
};

TEST_F(BookmarksServiceImplTest, GetBookmarks_Empty) {
  auto result = service_->GetBookmarks();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_folder());
  const auto& root_folder = result.value()->get_folder();
  EXPECT_EQ(root_folder->id, model_->root_node()->uuid());

  ASSERT_EQ(root_folder->children.size(), 3u);
  ASSERT_TRUE(root_folder->children[0]->is_folder());
  EXPECT_TRUE(root_folder->children[0]->get_folder()->children.empty());
  ASSERT_TRUE(root_folder->children[1]->is_folder());
  EXPECT_TRUE(root_folder->children[1]->get_folder()->children.empty());
  ASSERT_TRUE(root_folder->children[2]->is_folder());
  EXPECT_TRUE(root_folder->children[2]->get_folder()->children.empty());
}

TEST_F(BookmarksServiceImplTest, GetBookmark_NotFound) {
  auto result = service_->GetBookmark(base::Uuid::GenerateRandomV4());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kNotFound);
}

TEST_F(BookmarksServiceImplTest, GetBookmark_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  auto result = service_->GetBookmark(node->uuid());
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_url());
  const auto& url_node = result.value()->get_url();
  EXPECT_EQ(url_node->id, node->uuid());
  EXPECT_EQ(url_node->title, "Title");
  EXPECT_EQ(url_node->url, GURL("http://example.com"));
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_Bookmark_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);

  auto url_node = mojom::Url::New();
  url_node->title = "New Bookmark";
  url_node->url = GURL("http://new-example.com");
  auto node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result =
      service_->CreateBookmarkNode(parent->uuid(), 0, std::move(node));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_url());
  const auto& created_url_node = result.value()->get_url();
  EXPECT_EQ(created_url_node->title, "New Bookmark");
  EXPECT_EQ(created_url_node->url, GURL("http://new-example.com"));

  // Verify it was actually added to the model.
  const bookmarks::BookmarkNode* model_node = model_->GetNodeByUuid(
      created_url_node->id.value(),
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);
  ASSERT_TRUE(model_node);
  EXPECT_EQ(model_node->GetTitle(), u"New Bookmark");
  EXPECT_EQ(model_node->url(), GURL("http://new-example.com"));
  EXPECT_EQ(parent->children()[0].get(), model_node);
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_Folder_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);

  auto folder_node = mojom::Folder::New();
  folder_node->title = "New Folder";
  auto node = mojom::BookmarkNode::NewFolder(std::move(folder_node));

  auto result =
      service_->CreateBookmarkNode(parent->uuid(), 0, std::move(node));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_folder());
  const auto& created_folder_node = result.value()->get_folder();
  EXPECT_EQ(created_folder_node->title, "New Folder");
  EXPECT_TRUE(created_folder_node->children.empty());

  // Verify it was actually added to the model.
  const bookmarks::BookmarkNode* model_node = model_->GetNodeByUuid(
      created_folder_node->id.value(),
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);
  ASSERT_TRUE(model_node);
  EXPECT_EQ(model_node->GetTitle(), u"New Folder");
  EXPECT_TRUE(model_node->is_folder());
  EXPECT_EQ(parent->children()[0].get(), model_node);
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_Append_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  model_->AddURL(parent, 0, u"Existing", GURL("http://existing.com"));

  auto url_node = mojom::Url::New();
  url_node->title = "New Bookmark";
  url_node->url = GURL("http://new-example.com");
  auto node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result = service_->CreateBookmarkNode(parent->uuid(), std::nullopt,
                                             std::move(node));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_url());
  const auto& created_url_node = result.value()->get_url();

  ASSERT_EQ(parent->children().size(), 2u);
  EXPECT_EQ(parent->children()[1]->GetTitle(), u"New Bookmark");
  EXPECT_EQ(parent->children()[1]->uuid(), created_url_node->id);
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_NegativeIndex_Error) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);

  auto url_node = mojom::Url::New();
  url_node->title = "New Bookmark";
  url_node->url = GURL("http://new-example.com");
  auto node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result =
      service_->CreateBookmarkNode(parent->uuid(), -1, std::move(node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_IndexOutOfRange_Error) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);

  auto url_node = mojom::Url::New();
  url_node->title = "New Bookmark";
  url_node->url = GURL("http://new-example.com");
  auto node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  // Parent has 0 children, so index 1 is out of range.
  auto result =
      service_->CreateBookmarkNode(parent->uuid(), 1, std::move(node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_InvalidParent) {
  auto url_node = mojom::Url::New();
  url_node->title = "New Bookmark";
  url_node->url = GURL("http://new-example.com");
  auto node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result = service_->CreateBookmarkNode(base::Uuid::GenerateRandomV4(), 0,
                                             std::move(node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kNotFound);
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_ParentNotFolder) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* url_node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  auto new_url_node = mojom::Url::New();
  new_url_node->title = "New Bookmark";
  new_url_node->url = GURL("http://new-example.com");
  auto node = mojom::BookmarkNode::NewUrl(std::move(new_url_node));

  auto result =
      service_->CreateBookmarkNode(url_node->uuid(), 0, std::move(node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_NullNode_Error) {
  auto result = service_->CreateBookmarkNode(
      model_->bookmark_bar_node()->uuid(), 0, nullptr);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, CreateBookmarkNode_InvalidUrl) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);

  auto url_node = mojom::Url::New();
  url_node->title = "New Bookmark";
  url_node->url = GURL("invalid-url");
  auto node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result =
      service_->CreateBookmarkNode(parent->uuid(), 0, std::move(node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_Title_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  auto url_node = mojom::Url::New();
  url_node->id = node->uuid();
  url_node->title = "Updated Title";
  url_node->url = GURL("http://example.com");
  auto update_node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_url());
  EXPECT_EQ(result.value()->get_url()->title, "Updated Title");
  EXPECT_EQ(result.value()->get_url()->url, GURL("http://example.com"));

  EXPECT_EQ(node->GetTitle(), u"Updated Title");
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_Url_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  auto url_node = mojom::Url::New();
  url_node->id = node->uuid();
  url_node->title = "Title";
  url_node->url = GURL("http://updated-example.com");
  auto update_node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_url());
  EXPECT_EQ(result.value()->get_url()->title, "Title");
  EXPECT_EQ(result.value()->get_url()->url, GURL("http://updated-example.com"));

  EXPECT_EQ(node->url(), GURL("http://updated-example.com"));
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_AllFields_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  auto url_node = mojom::Url::New();
  url_node->id = node->uuid();
  url_node->title = "Updated Title";
  url_node->url = GURL("http://updated-example.com");
  auto update_node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_url());
  EXPECT_EQ(result.value()->get_url()->title, "Updated Title");
  EXPECT_EQ(result.value()->get_url()->url, GURL("http://updated-example.com"));

  EXPECT_EQ(node->GetTitle(), u"Updated Title");
  EXPECT_EQ(node->url(), GURL("http://updated-example.com"));
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_Folder_Title_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node = model_->AddFolder(parent, 0, u"Folder");

  auto folder_node = mojom::Folder::New();
  folder_node->id = node->uuid();
  folder_node->title = "Updated Folder";
  auto update_node = mojom::BookmarkNode::NewFolder(std::move(folder_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_folder());
  EXPECT_EQ(result.value()->get_folder()->title, "Updated Folder");

  EXPECT_EQ(node->GetTitle(), u"Updated Folder");
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_Folder_Url_Error) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node = model_->AddFolder(parent, 0, u"Folder");

  // We try to update a folder node with URL data.
  auto url_node = mojom::Url::New();
  url_node->id = node->uuid();
  url_node->title = "Updated Folder";
  url_node->url = GURL("http://example.com");
  auto update_node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_UrlNode_WithFolder_Error) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  // We try to update a URL node with Folder data.
  auto folder_node = mojom::Folder::New();
  folder_node->id = node->uuid();
  folder_node->title = "New Folder Title";
  auto update_node = mojom::BookmarkNode::NewFolder(std::move(folder_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_InvalidUrl_Error) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  auto url_node = mojom::Url::New();
  url_node->id = node->uuid();
  url_node->title = "Title";
  url_node->url = GURL("invalid-url");
  auto update_node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_NotFound) {
  auto url_node = mojom::Url::New();
  url_node->id = base::Uuid::GenerateRandomV4();
  url_node->title = "Updated Title";
  url_node->url = GURL("http://example.com");
  auto update_node = mojom::BookmarkNode::NewUrl(std::move(url_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kNotFound);
}

TEST_F(BookmarksServiceImplTest, UpdateBookmarkNode_PermanentNode_Error) {
  const bookmarks::BookmarkNode* node = model_->bookmark_bar_node();
  ASSERT_TRUE(node);

  auto folder_node = mojom::Folder::New();
  folder_node->id = node->uuid();
  folder_node->title = "Updated Title";
  auto update_node = mojom::BookmarkNode::NewFolder(std::move(folder_node));

  auto result = service_->UpdateBookmarkNode(std::move(update_node));
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

TEST_F(BookmarksServiceImplTest, DeleteBookmarkNode_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  ASSERT_TRUE(parent);
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  base::Uuid uuid = node->uuid();
  auto result = service_->DeleteBookmarkNode(uuid);
  EXPECT_TRUE(result.has_value());

  // Verify it was removed from the model.
  EXPECT_FALSE(model_->GetNodeByUuid(
      uuid,
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_TRUE(parent->children().empty());
}

TEST_F(BookmarksServiceImplTest, DeleteBookmarkNode_NotFound) {
  auto result = service_->DeleteBookmarkNode(base::Uuid::GenerateRandomV4());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kNotFound);
}

TEST_F(BookmarksServiceImplTest, DeleteBookmarkNode_PermanentNode_Error) {
  const bookmarks::BookmarkNode* node = model_->bookmark_bar_node();
  ASSERT_TRUE(node);

  auto result = service_->DeleteBookmarkNode(node->uuid());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kInvalidArgument);
}

}  // namespace bookmarks_api
