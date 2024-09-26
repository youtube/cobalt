// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_mediator.h"

#import "base/test/scoped_feature_list.h"
#import "components/prefs/testing_pref_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_mediator+internal.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// List of item types.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSafeBrowsingStandardProtection = kItemTypeEnumZero,
  ItemTypeSafeBrowsingEnhancedProtection,
  ItemTypeSafeBrowsingNoProtection,
};

// Registers account preference that will be used for Safe Browsing. Default
// state of Safe Browsing should be Standard Protection.
std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
  auto prefs = std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  user_prefs::PrefRegistrySyncable* registry = prefs->registry();
  RegisterBrowserStatePrefs(registry);
  return prefs;
}

}  // anonymous namespace

class PrivacySafeBrowsingMediatorTest : public PlatformTest {
 public:
  PrivacySafeBrowsingMediatorTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPrefService(CreatePrefService());
    browser_state_ = test_cbs_builder.Build();

    mediator_ = [[PrivacySafeBrowsingMediator alloc]
        initWithUserPrefService:browser_state_->GetPrefs()];
    [mediator_ safeBrowsingItems];
  }

  void TearDown() override {
    [[mediator_ safeBrowsingEnhancedProtectionPreference] stop];
    [[mediator_ safeBrowsingStandardProtectionPreference] stop];
    PlatformTest::TearDown();
  }

  TableViewItem* itemWithItemType(ItemType type) {
    for (TableViewItem* item in mediator_.safeBrowsingItems) {
      ItemType itemType = static_cast<ItemType>(item.type);
      if (itemType == type)
        return item;
    }
    return nil;
  }

 protected:
  web::WebTaskEnvironment environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  PrivacySafeBrowsingMediator* mediator_;
};

TEST_F(PrivacySafeBrowsingMediatorTest, TurnOnEnhancedProtection) {
  TableViewItem* enhanced_safe_browsing_item =
      itemWithItemType(ItemTypeSafeBrowsingEnhancedProtection);
  [mediator_ didSelectItem:enhanced_safe_browsing_item];
  [mediator_ selectSettingItem:enhanced_safe_browsing_item];
  EXPECT_TRUE([mediator_
      shouldItemTypeHaveCheckmark:ItemTypeSafeBrowsingEnhancedProtection]);
  EXPECT_TRUE(
      safe_browsing::GetSafeBrowsingState(*browser_state_->GetPrefs()) ==
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
}

TEST_F(PrivacySafeBrowsingMediatorTest, TurnOnStandardProtection) {
  TableViewItem* standard_safe_browsing_item =
      itemWithItemType(ItemTypeSafeBrowsingStandardProtection);
  [mediator_ didSelectItem:standard_safe_browsing_item];
  [mediator_ selectSettingItem:standard_safe_browsing_item];
  EXPECT_TRUE([mediator_
      shouldItemTypeHaveCheckmark:ItemTypeSafeBrowsingStandardProtection]);
  EXPECT_TRUE(
      safe_browsing::GetSafeBrowsingState(*browser_state_->GetPrefs()) ==
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
}

TEST_F(PrivacySafeBrowsingMediatorTest, TurnOffSafeBrowsing) {
  id mock_handler =
      OCMProtocolMock(@protocol(PrivacySafeBrowsingNavigationCommands));
  OCMExpect([mock_handler showSafeBrowsingNoProtectionPopUp:[OCMArg any]]);
  mediator_.handler = mock_handler;

  // Simulate pressing the "No Protection" option in the Safe Browsing settings
  // page and turning off Safe Browsing.
  TableViewItem* no_safe_browsing_item =
      itemWithItemType(ItemTypeSafeBrowsingNoProtection);
  [mediator_ didSelectItem:no_safe_browsing_item];
  [mediator_ selectSettingItem:no_safe_browsing_item];
  EXPECT_TRUE(
      [mediator_ shouldItemTypeHaveCheckmark:ItemTypeSafeBrowsingNoProtection]);
  EXPECT_TRUE(
      safe_browsing::GetSafeBrowsingState(*browser_state_->GetPrefs()) ==
      safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);

  EXPECT_OCMOCK_VERIFY(mock_handler);
}

TEST_F(PrivacySafeBrowsingMediatorTest, CancelTurnOffSafeBrowsing) {
  id mock_handler =
      OCMProtocolMock(@protocol(PrivacySafeBrowsingNavigationCommands));
  OCMExpect([mock_handler showSafeBrowsingNoProtectionPopUp:[OCMArg any]]);
  mediator_.handler = mock_handler;

  // Simulate pressing the "No Protection" option in the Safe Browsing settings
  // page and pressing the cancel button in the No Protection pop up.
  TableViewItem* no_safe_browsing_item =
      itemWithItemType(ItemTypeSafeBrowsingNoProtection);
  [mediator_ didSelectItem:no_safe_browsing_item];
  EXPECT_FALSE(
      [mediator_ shouldItemTypeHaveCheckmark:ItemTypeSafeBrowsingNoProtection]);
  EXPECT_FALSE(
      safe_browsing::GetSafeBrowsingState(*browser_state_->GetPrefs()) ==
      safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);

  EXPECT_OCMOCK_VERIFY(mock_handler);
}
