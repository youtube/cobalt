// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_view_controller.h"

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"

#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for testing SuggestedActionsViewController class.
class SuggestedActionsViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  SuggestedActionsViewControllerTest()
      : delegate_(OCMProtocolMock(
            @protocol(SuggestedActionsViewControllerDelegate))) {}

  ChromeTableViewController* InstantiateController() override {
    return [[SuggestedActionsViewController alloc] initWithDelegate:delegate_];
  }
  // Delegate mock conforming to SuggestedActionsViewControllerDelegate
  // protocol.
  id delegate_;
};

// Tests that the view controller has been initialized correctly.
TEST_F(SuggestedActionsViewControllerTest, Initialization) {
  CreateController();
  CheckController();

  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(3, NumberOfItemsInSection(0));

  // This is a static table it should have 3 items in that order: (SearchWeb,
  // SearchRecentTabs, SearchHistory).
  TableViewImageItem* item = GetTableViewItem(0, 0);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_TABS_SEARCH_SUGGESTED_ACTION_SEARCH_WEB),
      item.title);
  item = GetTableViewItem(0, 1);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_TABS_SEARCH_SUGGESTED_ACTION_SEARCH_RECENT_TABS),
              item.title);
  item = GetTableViewItem(0, 2);
  EXPECT_NSEQ(
      l10n_util::GetNSString(
          IDS_IOS_TABS_SEARCH_SUGGESTED_ACTION_SEARCH_HISTORY_UNKNOWN_RESULT_COUNT),
      item.title);
}

// Tests that selecting the search web item should call the relevant delegate.
TEST_F(SuggestedActionsViewControllerTest, SelectSearchWebSuggestedAction) {
  CreateController();
  CheckController();
  OCMExpect([delegate_ didSelectSearchWebInSuggestedActionsViewController:
                           (SuggestedActionsViewController*)controller()]);
  [controller() tableView:controller().tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
  EXPECT_OCMOCK_VERIFY(delegate_);
}

// Tests that selecting the search recent tabs item should call the relevant
// delegate.
TEST_F(SuggestedActionsViewControllerTest,
       SelectSearchRecentTabsSuggestedAction) {
  CreateController();
  CheckController();
  OCMExpect(
      [delegate_ didSelectSearchRecentTabsInSuggestedActionsViewController:
                     (SuggestedActionsViewController*)controller()]);
  [controller() tableView:controller().tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:1 inSection:0]];
  EXPECT_OCMOCK_VERIFY(delegate_);
}

// Tests that selecting the search history item should call the relevant
// delegate.
TEST_F(SuggestedActionsViewControllerTest, SelectSearchHistorySuggestedAction) {
  CreateController();
  CheckController();
  OCMExpect([delegate_ didSelectSearchHistoryInSuggestedActionsViewController:
                           (SuggestedActionsViewController*)controller()]);
  [controller() tableView:controller().tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:2 inSection:0]];
  EXPECT_OCMOCK_VERIFY(delegate_);
}
