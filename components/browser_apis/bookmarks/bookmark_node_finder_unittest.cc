// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/bookmark_node_finder.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks_api {

class BookmarkNodeFinderTest : public testing::Test {
 protected:
  BookmarkNodeFinderTest() {
    model_ = std::make_unique<bookmarks::BookmarkModel>(
        std::make_unique<bookmarks::TestBookmarkClient>());
    model_->LoadEmptyForTest();
    finder_ = std::make_unique<BookmarkNodeFinder>(model_.get());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  std::unique_ptr<BookmarkNodeFinder> finder_;
};

TEST_F(BookmarkNodeFinderTest, FindNodeByUuid_Success) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  auto result = finder_->FindNodeByUuid(node->uuid());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), node);
}

TEST_F(BookmarkNodeFinderTest, FindNodeByUuid_NotFound) {
  auto result = finder_->FindNodeByUuid(base::Uuid::GenerateRandomV4());
  EXPECT_FALSE(result.has_value());
}

}  // namespace bookmarks_api
