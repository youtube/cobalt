// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_sub_data_source_impl.h"

#import <Foundation/Foundation.h>
#import <OCMock/OCMock.h>

#import "base/containers/contains.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/bookmarks/model/account_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_consumer.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

enum class TestParam {
  kLocalOrSyncableModel,
  kAccountModel,
};

}  // namespace

@interface FakeBookmarksFolderChooserParentDataSource
    : NSObject <BookmarksFolderChooserParentDataSource>

// The argument provided when `bookmarkNodeDeleted:` was called.
@property(nonatomic, assign) const BookmarkNode* bookmarkNodeDeletedArg;
// The argument provided when `bookmarkModelWillRemoveAllNodes:` was called.
@property(nonatomic, assign)
    const BookmarkModel* bookmarkModelWillRemoveAllNodesArg;

- (instancetype)initWithNodes:(const std::set<const BookmarkNode*>&)nodes;

@end

@implementation FakeBookmarksFolderChooserParentDataSource {
  std::set<const BookmarkNode*> _editedNodes;
}

- (instancetype)initWithNodes:(const std::set<const BookmarkNode*>&)nodes {
  if ((self = [super init])) {
    _editedNodes = nodes;
  }
  return self;
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)bookmarkNode {
  if (base::Contains(_editedNodes, bookmarkNode)) {
    _editedNodes.erase(bookmarkNode);
  }
  _bookmarkNodeDeletedArg = bookmarkNode;
}

- (void)bookmarkModelWillRemoveAllNodes:(const BookmarkModel*)bookmarkModel {
  _editedNodes.clear();
  _bookmarkModelWillRemoveAllNodesArg = bookmarkModel;
}

- (const std::set<const BookmarkNode*>&)editedNodes {
  return _editedNodes;
}

@end

class BookmarksFolderChooserSubDataSourceImplTest
    : public PlatformTest,
      public testing::WithParamInterface<TestParam> {
 protected:
  BookmarksFolderChooserSubDataSourceImplTest() {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        ios::LocalOrSyncableBookmarkModelFactory::GetInstance(),
        ios::LocalOrSyncableBookmarkModelFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        ios::AccountBookmarkModelFactory::GetInstance(),
        ios::AccountBookmarkModelFactory::GetDefaultFactory());

    browser_state_ = builder.Build();
    // `scoped_feature_list_` should be initialized before creating the
    // `model_`. Otherwise `AccountBookmarkModelFactory` will return `nullptr`.
    scoped_feature_list_.InitAndEnableFeature(
        syncer::kEnableBookmarksAccountStorage);
    model_ = GetParam() == TestParam::kAccountModel
                 ? ios::AccountBookmarkModelFactory::GetForBrowserState(
                       browser_state_.get())
                 : ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
                       browser_state_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(model_);
    mock_consumer_ =
        OCMStrictProtocolMock(@protocol(BookmarksFolderChooserConsumer));
    edited_nodes_.insert(AddURL(model_->mobile_node(), @"Test URL"));
  }

  ~BookmarksFolderChooserSubDataSourceImplTest() override {
    [sub_data_source_ disconnect];
    sub_data_source_.consumer = nil;
    sub_data_source_ = nil;
    mock_consumer_ = nil;
    fake_parent_data_source_ = nil;
    model_ = nullptr;
  }

  void CreateSubDataSource() {
    fake_parent_data_source_ =
        [[FakeBookmarksFolderChooserParentDataSource alloc]
            initWithNodes:edited_nodes_];
    sub_data_source_ = [[BookmarksFolderChooserSubDataSourceImpl alloc]
        initWithBookmarkModel:model_
             parentDataSource:fake_parent_data_source_];
    sub_data_source_.consumer = mock_consumer_;
  }

  const BookmarkNode* AddURL(const BookmarkNode* parent, NSString* title) {
    std::u16string c_title = base::SysNSStringToUTF16(title);
    GURL url(base::SysNSStringToUTF16(@"http://example.com/bookmark") +
             c_title);
    return model_->AddURL(parent, parent->children().size(), c_title, url);
  }

  const BookmarkNode* AddFolder(const BookmarkNode* parent, NSString* title) {
    std::u16string c_title = base::SysNSStringToUTF16(title);
    return model_->AddFolder(parent, parent->children().size(), c_title);
  }

  void ChangeTitle(const BookmarkNode* node, NSString* title) {
    std::u16string c_title = base::SysNSStringToUTF16(title);
    model_->SetTitle(node, c_title,
                     bookmarks::metrics::BookmarkEditSource::kUser);
  }

  void RemoveNode(const BookmarkNode* node) {
    model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser);
  }

  void RemoveAllNodes() { model_->RemoveAllUserBookmarks(); }

  void MoveNode(const BookmarkNode* node, const BookmarkNode* new_parent) {
    model_->Move(node, new_parent, new_parent->children().size());
  }

  IOSChromeScopedTestingLocalState local_state_;
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  BookmarkModel* model_;
  BookmarksFolderChooserSubDataSourceImpl* sub_data_source_;
  id mock_consumer_;
  FakeBookmarksFolderChooserParentDataSource* fake_parent_data_source_;
  NSString* test_folder_title_1 = @"Test Folder 1";
  NSString* test_folder_title_2 = @"Test Folder 2";
  std::set<const BookmarkNode*> edited_nodes_;
};

// Tests that the sub data source correctly fetches visible folders.
TEST_P(BookmarksFolderChooserSubDataSourceImplTest, TestVisibleFolderNodes) {
  const BookmarkNode* test_folder_node_1 =
      AddFolder(model_->mobile_node(), test_folder_title_1);
  const BookmarkNode* test_folder_node_2 =
      AddFolder(test_folder_node_1, test_folder_title_2);
  edited_nodes_.insert(test_folder_node_2);
  CreateSubDataSource();

  std::vector<const BookmarkNode*> visible_folder_nodes =
      [sub_data_source_ visibleFolderNodes];
  ASSERT_EQ(2u, visible_folder_nodes.size());
  EXPECT_NSEQ(base::SysUTF16ToNSString(visible_folder_nodes[0]->GetTitle()),
              @"Mobile Bookmarks");
  EXPECT_NSEQ(base::SysUTF16ToNSString(visible_folder_nodes[1]->GetTitle()),
              test_folder_title_1);
}

// Tests that changing title of bookmarked folder node updates the UI.
TEST_P(BookmarksFolderChooserSubDataSourceImplTest, TestFolderTitleChange) {
  const BookmarkNode* test_folder_node =
      AddFolder(model_->mobile_node(), test_folder_title_1);
  CreateSubDataSource();

  [[mock_consumer_ expect] notifyModelUpdated];
  ChangeTitle(test_folder_node, test_folder_title_2);

  [mock_consumer_ verify];
  std::vector<const BookmarkNode*> visible_folder_nodes =
      [sub_data_source_ visibleFolderNodes];
  ASSERT_EQ(2u, visible_folder_nodes.size());
  EXPECT_NSEQ(base::SysUTF16ToNSString(visible_folder_nodes[0]->GetTitle()),
              @"Mobile Bookmarks");
  EXPECT_NSEQ(base::SysUTF16ToNSString(visible_folder_nodes[1]->GetTitle()),
              test_folder_title_2);
}

// Tests that adding a folder node in the bookmark model updates the UI.
TEST_P(BookmarksFolderChooserSubDataSourceImplTest, TestFolderAdded) {
  const BookmarkNode* test_folder_node_1 =
      AddFolder(model_->mobile_node(), test_folder_title_1);
  CreateSubDataSource();

  [[mock_consumer_ expect] notifyModelUpdated];
  AddFolder(test_folder_node_1, test_folder_title_2);

  [mock_consumer_ verify];
  std::vector<const BookmarkNode*> visible_folder_nodes =
      [sub_data_source_ visibleFolderNodes];
  ASSERT_EQ(3u, visible_folder_nodes.size());
  EXPECT_NSEQ(base::SysUTF16ToNSString(visible_folder_nodes[0]->GetTitle()),
              @"Mobile Bookmarks");
  EXPECT_NSEQ(base::SysUTF16ToNSString(visible_folder_nodes[1]->GetTitle()),
              test_folder_title_1);
  EXPECT_NSEQ(base::SysUTF16ToNSString(visible_folder_nodes[2]->GetTitle()),
              test_folder_title_2);
}

// Tests that removing a folder node from the bookmark model updates the UI.
TEST_P(BookmarksFolderChooserSubDataSourceImplTest, TestFolderRemoved) {
  const BookmarkNode* test_folder_node_1 =
      AddFolder(model_->mobile_node(), test_folder_title_1);
  const BookmarkNode* test_folder_node_2 =
      AddFolder(test_folder_node_1, test_folder_title_2);
  CreateSubDataSource();

  // `mock_consumer_` gets notified twice in `bookmarkNodeChildrenChanged:` and
  // `bookmarkNodeDeleted:fromFolder:` method calls from `BookmarkModel`
  // observer.
  [[mock_consumer_ expect] notifyModelUpdated];
  [[mock_consumer_ expect] notifyModelUpdated];
  RemoveNode(test_folder_node_2);

  [mock_consumer_ verify];
  ASSERT_EQ(test_folder_node_2,
            fake_parent_data_source_.bookmarkNodeDeletedArg);
  std::vector<const BookmarkNode*> visible_folder_nodes =
      [sub_data_source_ visibleFolderNodes];
  ASSERT_EQ(2u, visible_folder_nodes.size());
  EXPECT_NSEQ(base::SysUTF16ToNSString(visible_folder_nodes[0]->GetTitle()),
              @"Mobile Bookmarks");
  EXPECT_NSEQ(base::SysUTF16ToNSString(visible_folder_nodes[1]->GetTitle()),
              test_folder_title_1);
}

// Tests that removing all nodes in the bookmark model updates the UI.
TEST_P(BookmarksFolderChooserSubDataSourceImplTest, TestAllFoldersRemoved) {
  const BookmarkNode* test_folder_node_1 =
      AddFolder(model_->mobile_node(), test_folder_title_1);
  AddFolder(test_folder_node_1, test_folder_title_2);
  CreateSubDataSource();

  [[mock_consumer_ expect] notifyModelUpdated];
  RemoveAllNodes();

  [mock_consumer_ verify];
  ASSERT_EQ(model_,
            fake_parent_data_source_.bookmarkModelWillRemoveAllNodesArg);
  std::vector<const BookmarkNode*> visible_folder_nodes =
      [sub_data_source_ visibleFolderNodes];
  ASSERT_EQ(1u, visible_folder_nodes.size());
  // "Mobile Bookmarks" is a permanent node and thus always exists.
  EXPECT_NSEQ(base::SysUTF16ToNSString(visible_folder_nodes[0]->GetTitle()),
              @"Mobile Bookmarks");
}

// Tests that moving a node in the bookmark model updates the UI.
TEST_P(BookmarksFolderChooserSubDataSourceImplTest, TestFolderMoved) {
  const BookmarkNode* test_folder_node_1 =
      AddFolder(model_->mobile_node(), test_folder_title_1);
  const BookmarkNode* test_folder_node_2 =
      AddFolder(test_folder_node_1, test_folder_title_2);
  CreateSubDataSource();

  [[mock_consumer_ expect] notifyModelUpdated];
  MoveNode(test_folder_node_2, model_->mobile_node());

  [mock_consumer_ verify];
  std::vector<const BookmarkNode*> visible_folder_nodes =
      [sub_data_source_ visibleFolderNodes];
  ASSERT_EQ(3u, visible_folder_nodes.size());
  EXPECT_NSEQ(base::SysUTF16ToNSString(visible_folder_nodes[0]->GetTitle()),
              @"Mobile Bookmarks");
  EXPECT_NSEQ(base::SysUTF16ToNSString(visible_folder_nodes[1]->GetTitle()),
              test_folder_title_1);
  EXPECT_NSEQ(base::SysUTF16ToNSString(visible_folder_nodes[2]->GetTitle()),
              test_folder_title_2);
}

INSTANTIATE_TEST_SUITE_P(/* No InstantionName*/,
                         BookmarksFolderChooserSubDataSourceImplTest,
                         testing::Values(TestParam::kAccountModel,
                                         TestParam::kLocalOrSyncableModel));
