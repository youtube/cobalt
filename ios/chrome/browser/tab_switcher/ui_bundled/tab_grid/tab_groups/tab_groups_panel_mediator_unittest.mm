// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/user_action_tester.h"
#import "components/collaboration/public/messaging/messaging_backend_service.h"
#import "components/collaboration/public/messaging/util.h"
#import "components/collaboration/test_support/mock_collaboration_service.h"
#import "components/collaboration/test_support/mock_messaging_backend_service.h"
#import "components/data_sharing/test_support/mock_data_sharing_service.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/types.h"
#import "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#import "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#import "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/share_kit/model/test_share_kit_service.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_sync_service_observer_bridge.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/test/fake_tab_grid_toolbars_mediator.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using collaboration::MockCollaborationService;
using collaboration::messaging::CollaborationEvent;
using collaboration::messaging::MessagingBackendService;
using collaboration::messaging::MockMessagingBackendService;
using collaboration::messaging::PersistentMessage;
using collaboration::messaging::PersistentNotificationType;
using collaboration::messaging::TabGroupMessageMetadata;

using tab_groups::MockTabGroupSyncService;
using tab_groups::SharingState;
using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

const char* kSelectTabGroupsUMA = "MobileTabGridSelectTabGroups";

// Creates a message about a shared tab group that is no longer shared with the
// user.
PersistentMessage CreateUnavailableTabGroupMessage(
    const std::string& last_known_title) {
  PersistentMessage persistent_message;
  persistent_message.type = PersistentNotificationType::TOMBSTONED;
  persistent_message.collaboration_event =
      CollaborationEvent::TAB_GROUP_REMOVED;
  TabGroupMessageMetadata tab_group_metadata;
  tab_group_metadata.last_known_title = last_known_title;
  persistent_message.attribution.tab_group_metadata = tab_group_metadata;
  return persistent_message;
}

// Returns the summary from GetRemovedCollaborationsSummary as an NSString. If
// the summary was std::nullopt, this returns nil.
NSString* SummaryFromMessages(const std::vector<PersistentMessage>& messages) {
  std::optional<std::string> summary =
      collaboration::messaging::GetRemovedCollaborationsSummary(messages);
  if (summary.has_value()) {
    return base::SysUTF8ToNSString(summary.value());
  }
  return nil;
}

}  // namespace

@interface FakeTabGroupsPanelConsumer : NSObject <TabGroupsPanelConsumer>
@property(nonatomic, readonly) TabGroupsPanelItem* notificationItem;
@property(nonatomic, readonly, copy)
    NSArray<TabGroupsPanelItem*>* tabGroupItems;
@property(nonatomic, readonly) NSUInteger populateItemsCallCount;
@property(nonatomic, readonly) NSUInteger reconfigureItemCallCount;
@end

@implementation FakeTabGroupsPanelConsumer

#pragma mark TabGroupsPanelConsumer

- (void)populateNotificationItem:(TabGroupsPanelItem*)notificationItem
                   tabGroupItems:(NSArray<TabGroupsPanelItem*>*)tabGroupItems {
  _notificationItem = notificationItem;
  _tabGroupItems = [tabGroupItems copy];
  _populateItemsCallCount++;
}

- (void)reconfigureItem:(TabGroupsPanelItem*)item {
  _reconfigureItemCallCount++;
}

- (void)dismissModals {
}

@end

class TabGroupsPanelMediatorTest : public PlatformTest {
 protected:
  TabGroupsPanelMediatorTest() : web_state_list_(&web_state_list_delegate_) {
    profile_ = TestProfileIOS::Builder().Build();
    // Create a regular browser.
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());
    browser_list_->AddBrowser(browser_.get());
    mode_holder_ = [[TabGridModeHolder alloc] init];
    share_kit_service_ =
        std::make_unique<TestShareKitService>(nullptr, nullptr, nullptr);
    collaboration_service_ = std::make_unique<MockCollaborationService>();
    messaging_service_ = std::make_unique<MockMessagingBackendService>();
    data_sharing_service_ = std::make_unique<
        ::testing::NiceMock<data_sharing::MockDataSharingService>>();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<BrowserList> browser_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  ::testing::NiceMock<MockTabGroupSyncService> tab_group_sync_service_;
  TabGridModeHolder* mode_holder_;
  std::unique_ptr<ShareKitService> share_kit_service_;
  std::unique_ptr<MockCollaborationService> collaboration_service_;
  std::unique_ptr<MockMessagingBackendService> messaging_service_;
  std::unique_ptr<data_sharing::MockDataSharingService> data_sharing_service_;
};

// Tests that the service observation starts and stops when the mediator is
// released.
TEST_F(TabGroupsPanelMediatorTest, StartStopObserving_Released) {
  // Use a strict mock.
  tab_groups::MockTabGroupSyncService strict_tab_group_sync_service;
  // Expect the observation start.
  EXPECT_CALL(strict_tab_group_sync_service, AddObserver(_)).Times(1);
  __unused TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&strict_tab_group_sync_service
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];

  // Expect the observation end when the mediator is released.
  EXPECT_CALL(strict_tab_group_sync_service, RemoveObserver(_)).Times(1);
  mediator = nil;
}

// Tests that the service observation starts and stops when the mediator is
// released.
TEST_F(TabGroupsPanelMediatorTest, StartStopObserving_Disconnect) {
  // Use a strict mock.
  tab_groups::MockTabGroupSyncService strict_tab_group_sync_service;
  // Expect the observation start.
  EXPECT_CALL(strict_tab_group_sync_service, AddObserver(_)).Times(1);
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&strict_tab_group_sync_service
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];

  {
    EXPECT_CALL(strict_tab_group_sync_service, RemoveObserver(_)).Times(1);
    [mediator disconnect];
  }
}

// Tests that the UMA for selecting the Tab Groups panel is correctly recorded.
TEST_F(TabGroupsPanelMediatorTest, RecordUMAWhenSelected) {
  base::UserActionTester user_action_tester;
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];

  EXPECT_EQ(0, user_action_tester.GetActionCount(kSelectTabGroupsUMA));

  // Select a different page.
  [mediator currentlySelectedGrid:NO];
  EXPECT_EQ(0, user_action_tester.GetActionCount(kSelectTabGroupsUMA));

  // Select Tab Groups.
  [mediator currentlySelectedGrid:YES];
  EXPECT_EQ(1, user_action_tester.GetActionCount(kSelectTabGroupsUMA));

  // Unselect Tab Groups.
  [mediator currentlySelectedGrid:NO];
  EXPECT_EQ(1, user_action_tester.GetActionCount(kSelectTabGroupsUMA));
}

// Tests that when the panel not the selected one, no toolbar delegate is set,
// no toolbar config is returned.
TEST_F(TabGroupsPanelMediatorTest, NotSelected_NoToolbarsDelegateOrConfig) {
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  FakeTabGridToolbarsMediator* toolbars_mutator =
      [[FakeTabGridToolbarsMediator alloc] init];
  mediator.toolbarsMutator = toolbars_mutator;

  [mediator currentlySelectedGrid:NO];

  EXPECT_EQ(toolbars_mutator.delegate, nil);
  EXPECT_EQ(toolbars_mutator.configuration, nil);
}

// Tests that when the panel is disabled by policy, the toolbars config is the
// disabled one.
TEST_F(TabGroupsPanelMediatorTest, DisabledByPolicy_DisabledToolbarsConfig) {
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:YES
                      browserList:browser_list_];
  FakeTabGridToolbarsMediator* toolbars_mutator =
      [[FakeTabGridToolbarsMediator alloc] init];
  mediator.toolbarsMutator = toolbars_mutator;

  [mediator currentlySelectedGrid:YES];

  EXPECT_EQ(toolbars_mutator.delegate,
            static_cast<id<TabGridToolbarsGridDelegate>>(mediator));
  EXPECT_NE(toolbars_mutator.configuration, nil);
  EXPECT_EQ(TabGridPageTabGroups, toolbars_mutator.configuration.page);

  // All buttons are disabled.
  EXPECT_FALSE(toolbars_mutator.configuration.doneButton);

  EXPECT_FALSE(toolbars_mutator.configuration.selectAllButton);
  EXPECT_EQ(0u, toolbars_mutator.configuration.selectedItemsCount);
  EXPECT_FALSE(toolbars_mutator.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.shareButton);
  EXPECT_FALSE(toolbars_mutator.configuration.addToButton);

  EXPECT_FALSE(toolbars_mutator.configuration.closeAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.newTabButton);
  EXPECT_FALSE(toolbars_mutator.configuration.searchButton);
  EXPECT_FALSE(toolbars_mutator.configuration.selectTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.undoButton);
  EXPECT_FALSE(toolbars_mutator.configuration.deselectAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.cancelSearchButton);
}

// Tests that when the panel is selected and not disabled by policy, the
// toolbars delegate and config are set accordingly. Since there are no tab in
// the Regular Tabs page, the Done button is still disabled.
TEST_F(TabGroupsPanelMediatorTest,
       EnabledByPolicyAndSelectedButNoRegularTab_DoneButtonDisabled) {
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  FakeTabGridToolbarsMediator* toolbars_mutator =
      [[FakeTabGridToolbarsMediator alloc] init];
  mediator.toolbarsMutator = toolbars_mutator;

  [mediator currentlySelectedGrid:YES];

  EXPECT_EQ(toolbars_mutator.delegate,
            static_cast<id<TabGridToolbarsGridDelegate>>(mediator));
  EXPECT_NE(toolbars_mutator.configuration, nil);
  EXPECT_EQ(TabGridPageTabGroups, toolbars_mutator.configuration.page);

  // Done button is disabled.
  EXPECT_FALSE(toolbars_mutator.configuration.doneButton);

  // All other buttons are disabled.
  EXPECT_FALSE(toolbars_mutator.configuration.selectAllButton);
  EXPECT_EQ(0u, toolbars_mutator.configuration.selectedItemsCount);
  EXPECT_FALSE(toolbars_mutator.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.shareButton);
  EXPECT_FALSE(toolbars_mutator.configuration.addToButton);

  EXPECT_FALSE(toolbars_mutator.configuration.closeAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.newTabButton);
  EXPECT_FALSE(toolbars_mutator.configuration.searchButton);
  EXPECT_FALSE(toolbars_mutator.configuration.selectTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.undoButton);
  EXPECT_FALSE(toolbars_mutator.configuration.deselectAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.cancelSearchButton);
}

// Tests that when the panel is selected and not disabled by policy, the
// toolbars delegate and config are set accordingly. Since there is a Regular
// tab, the Done button is finally enabled.
TEST_F(TabGroupsPanelMediatorTest,
       EnabledByPolicyAndSelectedWithRegularTab_DoneButtonEnabled) {
  // Add a web state in a local scope. Otherwise, the NiceMock for the sync
  // service reports an uninteresting call to RemoveObserver (??).
  {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_list_.InsertWebState(std::move(web_state));
  }
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  FakeTabGridToolbarsMediator* toolbars_mutator =
      [[FakeTabGridToolbarsMediator alloc] init];
  mediator.toolbarsMutator = toolbars_mutator;

  [mediator currentlySelectedGrid:YES];

  EXPECT_EQ(toolbars_mutator.delegate,
            static_cast<id<TabGridToolbarsGridDelegate>>(mediator));
  EXPECT_NE(toolbars_mutator.configuration, nil);
  EXPECT_EQ(TabGridPageTabGroups, toolbars_mutator.configuration.page);

  // Done button is enabled.
  EXPECT_TRUE(toolbars_mutator.configuration.doneButton);

  // All other buttons are disabled.
  EXPECT_FALSE(toolbars_mutator.configuration.selectAllButton);
  EXPECT_EQ(0u, toolbars_mutator.configuration.selectedItemsCount);
  EXPECT_FALSE(toolbars_mutator.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.shareButton);
  EXPECT_FALSE(toolbars_mutator.configuration.addToButton);

  EXPECT_FALSE(toolbars_mutator.configuration.closeAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.newTabButton);
  EXPECT_FALSE(toolbars_mutator.configuration.searchButton);
  EXPECT_FALSE(toolbars_mutator.configuration.selectTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.undoButton);
  EXPECT_FALSE(toolbars_mutator.configuration.deselectAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.cancelSearchButton);
}

// Tests that setting a consumer before the service initialization doesn't
// populate the consumer
TEST_F(TabGroupsPanelMediatorTest,
       SetConsumerDoesntPopulateFromUninitializedService) {
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  // Prepare a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  EXPECT_NSEQ(consumer.notificationItem, nil);
  EXPECT_NSEQ(consumer.tabGroupItems, nil);
  EXPECT_EQ(consumer.populateItemsCallCount, 0u);
  // Set a saved tab group.
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  std::vector<tab_groups::SavedTabGroup> groups = {group};
  EXPECT_CALL(tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(groups));

  mediator.consumer = consumer;

  EXPECT_NSEQ(consumer.notificationItem, nil);
  EXPECT_NSEQ(consumer.tabGroupItems, nil);
  EXPECT_EQ(consumer.populateItemsCallCount, 0u);
}

// Tests that setting a consumer after the service initialization populates it
// with data from the service's GetAllGroups API.
TEST_F(TabGroupsPanelMediatorTest,
       SetConsumerPopulatesFromInitializedAndNonEmptyService) {
  tab_groups::TabGroupSyncService::Observer* observer = nullptr;
  EXPECT_CALL(tab_group_sync_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&observer));
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  EXPECT_NE(observer, nullptr);
  // Prepare a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  EXPECT_NSEQ(consumer.notificationItem, nil);
  EXPECT_NSEQ(consumer.tabGroupItems, nil);
  EXPECT_EQ(consumer.populateItemsCallCount, 0u);
  // Set a saved tab group.
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  std::vector<tab_groups::SavedTabGroup> groups = {group};
  EXPECT_CALL(tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(groups));
  // Initialize the service.
  observer->OnInitialized();

  // Set the consumer.
  mediator.consumer = consumer;

  EXPECT_NSEQ(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 1u);
  EXPECT_EQ(consumer.populateItemsCallCount, 1u);
}

// Tests that setting a consumer populates it with data from the service's
// GetAllGroups API.
TEST_F(TabGroupsPanelMediatorTest,
       SetConsumerPopulatesFromInitializedEmptyService) {
  tab_groups::TabGroupSyncService::Observer* observer = nullptr;
  EXPECT_CALL(tab_group_sync_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&observer));
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  EXPECT_NE(observer, nullptr);
  // Prepare a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  EXPECT_NSEQ(consumer.notificationItem, nil);
  EXPECT_NSEQ(consumer.tabGroupItems, nil);
  EXPECT_EQ(consumer.populateItemsCallCount, 0u);
  // Set no saved tab group.
  std::vector<tab_groups::SavedTabGroup> groups = {};
  EXPECT_CALL(tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(groups));
  // Initialize the service.
  observer->OnInitialized();

  // Set the consumer.
  mediator.consumer = consumer;

  EXPECT_NSEQ(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 0u);
  EXPECT_EQ(consumer.populateItemsCallCount, 1u);
}

// Tests that when the sync service is initialized, the consumer is repopulated
// with data from the service's GetAllGroups API.
TEST_F(TabGroupsPanelMediatorTest,
       ServiceInitialized_RequeriesGroupsAndPopulates) {
  tab_groups::TabGroupSyncService::Observer* observer = nullptr;
  EXPECT_CALL(tab_group_sync_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&observer));
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  EXPECT_NE(observer, nullptr);
  // Set no saved tab group.
  std::vector<tab_groups::SavedTabGroup> groups = {};
  EXPECT_CALL(tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(groups));
  // Set a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  mediator.consumer = consumer;
  EXPECT_NSEQ(consumer.notificationItem, nil);
  EXPECT_NSEQ(consumer.tabGroupItems, nil);
  EXPECT_EQ(consumer.populateItemsCallCount, 0u);
  // Set a saved tab group.
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  groups = {group};
  EXPECT_CALL(tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(groups));

  observer->OnInitialized();

  EXPECT_NSEQ(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 1u);
  EXPECT_EQ(consumer.populateItemsCallCount, 1u);
}

// Tests that the groups pushed to the consumer are sorted with the most recent
// first.
TEST_F(TabGroupsPanelMediatorTest, PopulatesSortedGroups) {
  tab_groups::TabGroupSyncService::Observer* observer = nullptr;
  EXPECT_CALL(tab_group_sync_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&observer));
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  EXPECT_NE(observer, nullptr);
  // Set a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  mediator.consumer = consumer;
  // Set 2 saved tab group with the oldest first.
  tab_groups::SavedTabGroup group_1 = tab_groups::test::CreateTestSavedTabGroup(
      base::Time::Now() - base::Days(2));
  tab_groups::SavedTabGroup group_2 =
      tab_groups::test::CreateTestSavedTabGroup(base::Time::Now());
  std::vector<tab_groups::SavedTabGroup> groups = {group_1, group_2};
  EXPECT_CALL(tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(groups));
  observer->OnInitialized();

  EXPECT_NSEQ(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 2u);
  EXPECT_EQ(consumer.tabGroupItems[0].savedTabGroupID, group_2.saved_guid());
  EXPECT_EQ(consumer.tabGroupItems[1].savedTabGroupID, group_1.saved_guid());
}

// Tests that the consumer is asked to reconfigure an item when the observer
// notifies a group update.
TEST_F(TabGroupsPanelMediatorTest, UpdateGroup) {
  tab_groups::TabGroupSyncService::Observer* observer = nullptr;
  EXPECT_CALL(tab_group_sync_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&observer));
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:&tab_group_sync_service_
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  EXPECT_NE(observer, nullptr);
  // Set a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  mediator.consumer = consumer;
  // Set a saved tab group.
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  std::vector<tab_groups::SavedTabGroup> groups = {group};
  EXPECT_CALL(tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(groups));
  observer->OnInitialized();
  EXPECT_NSEQ(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 1u);
  EXPECT_EQ(consumer.reconfigureItemCallCount, 0u);

  observer->OnTabGroupUpdated(group, tab_groups::TriggerSource::REMOTE);

  EXPECT_NSEQ(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 1u);
  EXPECT_EQ(consumer.reconfigureItemCallCount, 1u);
}

// Tests that a group which doesn't exist locally is deleted from the tab group
// sync service.
TEST_F(TabGroupsPanelMediatorTest, DeleteRemoteGroup) {
  auto sync_service = std::make_unique<tab_groups::FakeTabGroupSyncService>();
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:sync_service.get()
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];

  // Set a saved tab group.
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  sync_service->AddGroup(group);
  EXPECT_TRUE(sync_service->GetGroup(group.saved_guid()).has_value());

  TabGroupsPanelItem* item = [[TabGroupsPanelItem alloc]
      initWithSavedTabGroupID:group.saved_guid()
                 sharingState:SharingState::kSharedAndOwned];
  [mediator deleteSyncedTabGroup:item.savedTabGroupID];

  EXPECT_FALSE(sync_service->GetGroup(group.saved_guid()).has_value());
}

// Tests that a group which exists locally is deleted from the tab group sync
// service.
TEST_F(TabGroupsPanelMediatorTest, DeleteLocalGroup) {
  auto sync_service = std::make_unique<tab_groups::FakeTabGroupSyncService>();
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:sync_service.get()
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];

  // Create a web state.
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  browser_->GetWebStateList()->InsertWebState(
      std::move(fake_web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  // Create a tab group.
  tab_groups::TabGroupId tab_group_id = tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data = tab_groups::TabGroupVisualData(
      u"Test Group", tab_groups::TabGroupColorId::kGrey);
  const TabGroup* tab_group =
      browser_->GetWebStateList()->CreateGroup({0}, visual_data, tab_group_id);

  // Set a saved tab group with a local ID of the tab group created above.
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  group.SetLocalGroupId(tab_group->tab_group_id());
  sync_service->AddGroup(group);
  EXPECT_EQ(1u, browser_->GetWebStateList()->GetGroups().size());
  EXPECT_EQ(1, browser_->GetWebStateList()->count());

  TabGroupsPanelItem* item = [[TabGroupsPanelItem alloc]
      initWithSavedTabGroupID:group.saved_guid()
                 sharingState:SharingState::kNotShared];
  [mediator deleteSyncedTabGroup:item.savedTabGroupID];

  // Check if the number of groups and tabs is 0.
  EXPECT_EQ(0u, browser_->GetWebStateList()->GetGroups().size());
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
}

// Tests that `facePileViewControllerForItem` returns an UIViewController when
// the group is shared.
TEST_F(TabGroupsPanelMediatorTest, FacePileViewControllerForItem) {
  auto sync_service = std::make_unique<tab_groups::FakeTabGroupSyncService>();
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:sync_service.get()
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];

  // Set a saved tab group.
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  group.SetLocalGroupId(tab_groups::TabGroupId::GenerateNew());
  sync_service->AddGroup(group);
  EXPECT_TRUE(sync_service->GetGroup(group.saved_guid()).has_value());
  TabGroupsPanelItem* item = [[TabGroupsPanelItem alloc]
      initWithSavedTabGroupID:group.saved_guid()
                 sharingState:SharingState::kSharedAndOwned];

  EXPECT_FALSE([mediator facePileViewControllerForItem:item]);

  // Share the group.
  sync_service->MakeTabGroupShared(
      group.local_group_id().value(), "collaboration",
      tab_groups::TabGroupSyncService::TabGroupSharingCallback());

  EXPECT_TRUE([mediator facePileViewControllerForItem:item]);
}

// Tests that a persistent message about a shared tab group that is no longer
// shared pushes a notification item to the consumer.
TEST_F(TabGroupsPanelMediatorTest, DisplayNotificationItem) {
  // Initially the messaging service is not initialized, and has no messages.
  EXPECT_CALL(*messaging_service_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*messaging_service_, GetMessages(_))
      .WillRepeatedly(Return(std::vector<PersistentMessage>{}));
  MessagingBackendService::PersistentMessageObserver* observer = nullptr;
  EXPECT_CALL(*messaging_service_, AddPersistentMessageObserver(_))
      .WillOnce(SaveArg<0>(&observer));

  // Create the mediator.
  auto sync_service = std::make_unique<tab_groups::FakeTabGroupSyncService>();
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:sync_service.get()
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  EXPECT_NE(observer, nullptr);
  // Prepare a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  mediator.consumer = consumer;
  EXPECT_CALL(*messaging_service_, IsInitialized())
      .WillRepeatedly(Return(true));
  EXPECT_NSEQ(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 0u);

  // Simulate the unsharing of a group communicated by the messaging backend
  // service.
  PersistentMessage persistent_message =
      CreateUnavailableTabGroupMessage("Vacation");
  const std::vector<PersistentMessage> messages = {persistent_message};
  EXPECT_CALL(*messaging_service_, GetMessages(_))
      .WillRepeatedly(Return(std::vector<PersistentMessage>{messages}));
  observer->DisplayPersistentMessage(persistent_message);

  EXPECT_NSNE(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 0u);
  EXPECT_EQ(consumer.notificationItem.type,
            TabGroupsPanelItemType::kNotification);
  EXPECT_NSEQ(consumer.notificationItem.notificationText,
              SummaryFromMessages(messages));
}

// Tests that a persistent message about a shared tab group that is no longer
// shared is removed from the consumer when asked to be hidden.
TEST_F(TabGroupsPanelMediatorTest, HideNotificationItem) {
  // Initially the messaging service is not initialized, and has no messages.
  EXPECT_CALL(*messaging_service_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*messaging_service_, GetMessages(_))
      .WillRepeatedly(Return(std::vector<PersistentMessage>{}));
  MessagingBackendService::PersistentMessageObserver* observer = nullptr;
  EXPECT_CALL(*messaging_service_, AddPersistentMessageObserver(_))
      .WillOnce(SaveArg<0>(&observer));

  // Create the mediator.
  auto sync_service = std::make_unique<tab_groups::FakeTabGroupSyncService>();
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:sync_service.get()
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  EXPECT_NE(observer, nullptr);
  // Prepare a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  mediator.consumer = consumer;
  EXPECT_CALL(*messaging_service_, IsInitialized())
      .WillRepeatedly(Return(true));
  EXPECT_NSEQ(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 0u);
  // Simulate the unsharing of two groups communicated by the messaging backend
  // service.
  PersistentMessage persistent_message_1 =
      CreateUnavailableTabGroupMessage("Vacation");
  PersistentMessage persistent_message_2 =
      CreateUnavailableTabGroupMessage("Birthday gift");
  std::vector<PersistentMessage> messages = {persistent_message_1,
                                             persistent_message_2};
  EXPECT_CALL(*messaging_service_, GetMessages(_))
      .WillRepeatedly(Return(std::vector<PersistentMessage>{
          persistent_message_1, persistent_message_2}));
  observer->DisplayPersistentMessage(persistent_message_1);
  observer->DisplayPersistentMessage(persistent_message_2);
  EXPECT_NSNE(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 0u);
  EXPECT_EQ(consumer.notificationItem.type,
            TabGroupsPanelItemType::kNotification);
  EXPECT_NSEQ(consumer.notificationItem.notificationText,
              SummaryFromMessages(messages));

  // Hide it now.
  messages = {persistent_message_2};
  EXPECT_CALL(*messaging_service_, GetMessages(_))
      .WillRepeatedly(Return(messages));
  observer->HidePersistentMessage(persistent_message_1);

  EXPECT_NSNE(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 0u);
  EXPECT_EQ(consumer.notificationItem.type,
            TabGroupsPanelItemType::kNotification);
  EXPECT_NSEQ(consumer.notificationItem.notificationText,
              SummaryFromMessages(messages));
}

// Tests that two persistent messages about shared tab groups that are no
// longer shared push two notification items to the consumer.
TEST_F(TabGroupsPanelMediatorTest, DisplayNotificationItemForTwoGroups) {
  // Initially the messaging service is not initialized, and has no messages.
  EXPECT_CALL(*messaging_service_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*messaging_service_, GetMessages(_))
      .WillRepeatedly(Return(std::vector<PersistentMessage>{}));
  MessagingBackendService::PersistentMessageObserver* observer = nullptr;
  EXPECT_CALL(*messaging_service_, AddPersistentMessageObserver(_))
      .WillOnce(SaveArg<0>(&observer));

  // Create the mediator.
  auto sync_service = std::make_unique<tab_groups::FakeTabGroupSyncService>();
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:sync_service.get()
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  EXPECT_NE(observer, nullptr);
  // Prepare a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  mediator.consumer = consumer;
  EXPECT_CALL(*messaging_service_, IsInitialized())
      .WillRepeatedly(Return(true));
  EXPECT_NSEQ(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 0u);

  // Simulate the unsharing of two groups communicated by the messaging backend
  // service.
  PersistentMessage persistent_message_1 =
      CreateUnavailableTabGroupMessage("Vacation");
  PersistentMessage persistent_message_2 =
      CreateUnavailableTabGroupMessage("Birthday gift");
  const std::vector<PersistentMessage> messages = {persistent_message_1,
                                                   persistent_message_2};
  EXPECT_CALL(*messaging_service_, GetMessages(_))
      .WillRepeatedly(Return(messages));
  observer->DisplayPersistentMessage(persistent_message_1);
  observer->DisplayPersistentMessage(persistent_message_2);

  EXPECT_NSNE(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 0u);
  EXPECT_EQ(consumer.notificationItem.type,
            TabGroupsPanelItemType::kNotification);
  EXPECT_NSEQ(consumer.notificationItem.notificationText,
              SummaryFromMessages(messages));
}

// Tests that three persistent messages about shared tab groups that are no
// longer shared push one aggregate notification item to the consumer.
TEST_F(TabGroupsPanelMediatorTest, DisplayAggregateNotificationItem) {
  // Initially the messaging service is not initialized, and has no messages.
  EXPECT_CALL(*messaging_service_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*messaging_service_, GetMessages(_))
      .WillRepeatedly(Return(std::vector<PersistentMessage>{}));
  MessagingBackendService::PersistentMessageObserver* observer = nullptr;
  EXPECT_CALL(*messaging_service_, AddPersistentMessageObserver(_))
      .WillOnce(SaveArg<0>(&observer));

  // Create the mediator.
  auto sync_service = std::make_unique<tab_groups::FakeTabGroupSyncService>();
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:sync_service.get()
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  EXPECT_NE(observer, nullptr);
  // Prepare a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  mediator.consumer = consumer;
  EXPECT_CALL(*messaging_service_, IsInitialized())
      .WillRepeatedly(Return(true));
  EXPECT_NSEQ(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 0u);

  // Simulate the unsharing of three groups communicated by the messaging
  // backend service.
  PersistentMessage persistent_message_1 =
      CreateUnavailableTabGroupMessage("Vacation");
  PersistentMessage persistent_message_2 =
      CreateUnavailableTabGroupMessage("Birthday gift");
  PersistentMessage persistent_message_3 =
      CreateUnavailableTabGroupMessage("Project Foo");
  const std::vector<PersistentMessage> messages = {
      persistent_message_1, persistent_message_2, persistent_message_3};
  EXPECT_CALL(*messaging_service_, GetMessages(_))
      .WillRepeatedly(Return(messages));
  observer->DisplayPersistentMessage(persistent_message_1);
  observer->DisplayPersistentMessage(persistent_message_2);
  observer->DisplayPersistentMessage(persistent_message_3);

  EXPECT_NSNE(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 0u);
  EXPECT_EQ(consumer.notificationItem.type,
            TabGroupsPanelItemType::kNotification);
  EXPECT_NSEQ(consumer.notificationItem.notificationText,
              SummaryFromMessages(messages));
}

// Tests that the aggregate notification item turns back to a notification item
// with two named groups when it goes down to 2 remaining messages.
TEST_F(TabGroupsPanelMediatorTest,
       ReturnToNotificationItemWithGroupNamesAfterAggregateNotificationItem) {
  // Initially the messaging service is not initialized, and has no messages.
  EXPECT_CALL(*messaging_service_, IsInitialized())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*messaging_service_, GetMessages(_))
      .WillRepeatedly(Return(std::vector<PersistentMessage>{}));
  MessagingBackendService::PersistentMessageObserver* observer = nullptr;
  EXPECT_CALL(*messaging_service_, AddPersistentMessageObserver(_))
      .WillOnce(SaveArg<0>(&observer));

  // Create the mediator.
  auto sync_service = std::make_unique<tab_groups::FakeTabGroupSyncService>();
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithTabGroupSyncService:sync_service.get()
                  shareKitService:share_kit_service_.get()
             collaborationService:collaboration_service_.get()
                 messagingService:messaging_service_.get()
               dataSharingService:data_sharing_service_.get()
              regularWebStateList:&web_state_list_
                    faviconLoader:nullptr
                 disabledByPolicy:NO
                      browserList:browser_list_];
  EXPECT_NE(observer, nullptr);
  // Prepare a consumer.
  FakeTabGroupsPanelConsumer* consumer =
      [[FakeTabGroupsPanelConsumer alloc] init];
  mediator.consumer = consumer;
  EXPECT_CALL(*messaging_service_, IsInitialized())
      .WillRepeatedly(Return(true));
  EXPECT_NSEQ(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 0u);

  // Simulate the unsharing of three groups communicated by the messaging
  // backend service.
  PersistentMessage persistent_message_1 =
      CreateUnavailableTabGroupMessage("Vacation");
  PersistentMessage persistent_message_2 =
      CreateUnavailableTabGroupMessage("Birthday gift");
  PersistentMessage persistent_message_3 =
      CreateUnavailableTabGroupMessage("Project Foo");
  EXPECT_CALL(*messaging_service_, GetMessages(_))
      .WillRepeatedly(Return(std::vector<PersistentMessage>{
          persistent_message_1, persistent_message_2, persistent_message_3}));
  observer->DisplayPersistentMessage(persistent_message_1);
  observer->DisplayPersistentMessage(persistent_message_2);
  observer->DisplayPersistentMessage(persistent_message_3);

  EXPECT_NSNE(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 0u);
  EXPECT_EQ(consumer.notificationItem.type,
            TabGroupsPanelItemType::kNotification);
  EXPECT_NSEQ(consumer.notificationItem.notificationText,
              l10n_util::GetNSStringF(
                  IDS_COLLABORATION_SEVERAL_GROUPS_REMOVED_NOTIFICATION, u"3"));

  // Hide one of the messages.
  const std::vector<PersistentMessage> messages = {persistent_message_1,
                                                   persistent_message_3};
  EXPECT_CALL(*messaging_service_, GetMessages(_))
      .WillRepeatedly(Return(messages));
  observer->HidePersistentMessage(persistent_message_2);

  EXPECT_NSNE(consumer.notificationItem, nil);
  EXPECT_EQ(consumer.tabGroupItems.count, 0u);
  EXPECT_EQ(consumer.notificationItem.type,
            TabGroupsPanelItemType::kNotification);
  EXPECT_NSEQ(consumer.notificationItem.notificationText,
              SummaryFromMessages(messages));
}
