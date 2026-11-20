// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/cells/bookmark_parent_folder_item.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using BookmarkParentFolderItemTest = PlatformTest;

TEST_F(BookmarkParentFolderItemTest, LabelGetsTitle) {
  BookmarkParentFolderItem* item =
      [[BookmarkParentFolderItem alloc] initWithType:0];
  BookmarkParentFolderCell* cell =
      [[BookmarkParentFolderCell alloc] initWithFrame:CGRectZero];
  item.title = @"Foo";
  [item configureCell:cell];
  EXPECT_NSEQ(@"Foo", cell.parentFolderNameLabel.text);
}

}  // namespace
