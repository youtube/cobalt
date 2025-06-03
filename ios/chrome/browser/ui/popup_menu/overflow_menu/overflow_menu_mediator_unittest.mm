// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_mediator.h"

#import "base/files/scoped_temp_dir.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/default_clock.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_utils.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "components/language/ios/browser/language_detection_java_script_feature.h"
#import "components/password_manager/core/browser/mock_password_store_interface.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/policy/core/common/mock_configuration_policy_provider.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/supervised_user/core/browser/supervised_user_preferences.h"
#import "components/supervised_user/core/common/pref_names.h"
#import "components/sync/base/features.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/test/mock_sync_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "components/translate/core/language_detection/language_detection_model.h"
#import "ios/chrome/browser/bookmarks/model/account_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_alert_dialog_overlay.h"
#import "ios/chrome/browser/overlays/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/policy/cloud/user_policy_constants.h"
#import "ios/chrome/browser/policy/enterprise_policy_test_helper.h"
#import "ios/chrome/browser/promos_manager/mock_promos_manager.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_test_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/signin/system_identity_manager.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#import "ios/chrome/browser/ui/popup_menu//overflow_menu/overflow_menu_orderer.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#import "ios/chrome/browser/ui/whats_new/constants.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "ios/chrome/browser/web/font_size/font_size_java_script_feature.h"
#import "ios/chrome/browser/web/font_size/font_size_tab_helper.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

using bookmarks::BookmarkModel;
using sync_preferences::PrefServiceMockFactory;
using sync_preferences::PrefServiceSyncable;
using testing::Return;
using user_prefs::PrefRegistrySyncable;

namespace {

const int kNumberOfWebStates = 3;

// Turns on Sync.
void SetupSyncServiceEnabledExpectations(
    syncer::MockSyncService* sync_service) {
  ON_CALL(*sync_service, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
  ON_CALL(*sync_service->GetMockUserSettings(),
          IsInitialSyncFeatureSetupComplete())
      .WillByDefault(Return(true));
  ON_CALL(*sync_service->GetMockUserSettings(), GetSelectedTypes())
      .WillByDefault(Return(syncer::UserSelectableTypeSet::All()));
  ON_CALL(*sync_service, HasSyncConsent()).WillByDefault(Return(true));
}

// Sync Service error that is eligble to be indicated as an Identity error when
// Sync is turned OFF.
constexpr syncer::SyncService::UserActionableError
    kEligibleIdentityErrorWhenSyncOff =
        syncer::SyncService::UserActionableError::kNeedsPassphrase;

// Sync Service error that is ineligble to be indicated as an Identity error
// when Sync is turned OFF.
constexpr syncer::SyncService::UserActionableError
    kIneligibleIdentityErrorWhenSyncOff =
        syncer::SyncService::UserActionableError::kGenericUnrecoverableError;

void CleanupNSUserDefaults() {
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kWhatsNewUsageEntryKey];
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kWhatsNewM116UsageEntryKey];
}

// Creates a PrefService that can be used by the browser state.
std::unique_ptr<PrefServiceSyncable> CreatePrefServiceForBrowserState() {
  PrefServiceMockFactory factory;
  scoped_refptr<PrefRegistrySyncable> registry(new PrefRegistrySyncable);
  std::unique_ptr<PrefServiceSyncable> prefs =
      factory.CreateSyncable(registry.get());
  RegisterBrowserStatePrefs(registry.get());
  return prefs;
}

}  // namespace

class OverflowMenuMediatorTest : public PlatformTest {
 public:
  OverflowMenuMediatorTest() {
    pref_service_.registry()->RegisterBooleanPref(
        translate::prefs::kOfferTranslateEnabled, true);
    pref_service_.registry()->RegisterStringPref(prefs::kSupervisedUserId,
                                                 std::string());
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kChildAccountStatusKnown, false);
  }

  void SetUp() override {
    PlatformTest::SetUp();

    // TODO(crbug.com/1425657): Removed this once the other test suites properly
    // clean up their NSUserDefaults on teardown.
    CleanupNSUserDefaults();

    TestChromeBrowserState::Builder builder;
    // Set a pref service for the ChromeBrowserState that is needed by some
    // factories (e.g. AuthenticationServiceFactory). The browser prefs for
    // testing the mediator are usually hosted in `browserStatePrefs_`.
    builder.SetPrefService(CreatePrefServiceForBrowserState());
    builder.AddTestingFactory(
        ios::LocalOrSyncableBookmarkModelFactory::GetInstance(),
        ios::LocalOrSyncableBookmarkModelFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                            web::BrowserState,
                            password_manager::MockPasswordStoreInterface>));
    builder.AddTestingFactory(
        ReadingListModelFactory::GetInstance(),
        base::BindRepeating(&BuildReadingListModelWithFakeStorage,
                            std::vector<scoped_refptr<ReadingListEntry>>()));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());

    browser_state_ = builder.Build();

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());

    web::test::OverrideJavaScriptFeatures(
        browser_state_.get(),
        {language::LanguageDetectionJavaScriptFeature::GetInstance()});

    // Set up the TestBrowser.
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    // Set up the WebStateList.
    auto navigation_manager = std::make_unique<ToolbarTestNavigationManager>();

    navigation_item_ = web::NavigationItem::Create();
    GURL url = GURL("http://chromium.org");
    navigation_item_->SetURL(url);
    navigation_manager->SetVisibleItem(navigation_item_.get());

    std::unique_ptr<web::FakeWebState> test_web_state =
        std::make_unique<web::FakeWebState>();
    test_web_state->SetNavigationManager(std::move(navigation_manager));
    test_web_state->SetLoading(true);
    test_web_state->SetBrowserState(browser_state_.get());
    web_state_ = test_web_state.get();

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    auto main_frame = web::FakeWebFrame::CreateMainWebFrame(
        /*security_origin=*/url);
    main_frame->set_browser_state(browser_state_.get());
    frames_manager->AddWebFrame(std::move(main_frame));
    web::ContentWorld content_world =
        language::LanguageDetectionJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld();
    web_state_->SetWebFramesManager(content_world, std::move(frames_manager));

    browser_->GetWebStateList()->InsertWebState(
        0, std::move(test_web_state), WebStateList::INSERT_FORCE_INDEX,
        WebStateOpener());
    for (int i = 1; i < kNumberOfWebStates; i++) {
      InsertNewWebState(i);
    }

    // Set up the OverlayPresenter.
    OverlayPresenter::FromBrowser(browser_.get(),
                                  OverlayModality::kWebContentArea)
        ->SetPresentationContext(&presentation_context_);

    baseViewController_ = [[UIViewController alloc] init];

    model_ = [[OverflowMenuModel alloc] initWithDestinations:@[]
                                                actionGroups:@[]];
  }

  void TearDown() override {
    // Explicitly disconnect the mediator so there won't be any WebStateList
    // observers when browser_ gets destroyed.
    [mediator_ disconnect];
    browser_.reset();

    CleanupNSUserDefaults();

    PlatformTest::TearDown();
  }

 protected:
  OverflowMenuMediator* CreateMediator(BOOL is_incognito) {
    orderer_ = [[OverflowMenuOrderer alloc] initWithIsIncognito:is_incognito];
    orderer_.model = model_;

    mediator_ = [[OverflowMenuMediator alloc] init];
    mediator_.isIncognito = is_incognito;
    mediator_.menuOrderer = orderer_;
    mediator_.baseViewController = baseViewController_;
    mediator_.supervisedUserService =
        SupervisedUserServiceFactory::GetForBrowserState(browser_state_.get());
    SetUpReadingList();
    return mediator_;
  }

  OverflowMenuMediator* CreateMediatorWithBrowserPolicyConnector(
      BOOL is_incognito,
      BrowserPolicyConnectorIOS* browser_policy_connector) {
    CreateMediator(is_incognito);
    mediator_.browserPolicyConnector = browser_policy_connector;
    return mediator_;
  }

  void CreateBrowserStatePrefs() {
    browserStatePrefs_ = std::make_unique<TestingPrefServiceSimple>();
    browserStatePrefs_->registry()->RegisterBooleanPref(
        bookmarks::prefs::kEditBookmarksEnabled,
        /*default_value=*/true);
  }

  void CreateLocalStatePrefs() {
    localStatePrefs_ = std::make_unique<TestingPrefServiceSimple>();
    localStatePrefs_->registry()->RegisterListPref(
        prefs::kOverflowMenuNewDestinations, PrefRegistry::LOSSY_PREF);
    localStatePrefs_->registry()->RegisterDictionaryPref(
        prefs::kOverflowMenuDestinationUsageHistory, PrefRegistry::LOSSY_PREF);
    localStatePrefs_->registry()->RegisterListPref(
        prefs::kOverflowMenuDestinationsOrder);
    localStatePrefs_->registry()->RegisterDictionaryPref(
        prefs::kOverflowMenuActionsOrder);
  }

  void SetUpBookmarks() {
    local_or_syncable_bookmark_model_ =
        ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
            browser_state_.get());
    DCHECK(local_or_syncable_bookmark_model_);
    account_bookmark_model_ =
        ios::AccountBookmarkModelFactory::GetForBrowserState(
            browser_state_.get());

    // TODO(crbug.com/1448010): Use two-model `WaitForBookmarkModelToLoad`.
    bookmarks::test::WaitForBookmarkModelToLoad(
        local_or_syncable_bookmark_model_);
    if (account_bookmark_model_) {
      bookmarks::test::WaitForBookmarkModelToLoad(account_bookmark_model_);
    }
    mediator_.localOrSyncableBookmarkModel = local_or_syncable_bookmark_model_;
    mediator_.accountBookmarkModel = account_bookmark_model_;
  }

  void SetUpReadingList() {
    reading_list_model_ =
        ReadingListModelFactory::GetForBrowserState(browser_state_.get());
    DCHECK(reading_list_model_);
    ASSERT_TRUE(
        base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(5), ^{
          return reading_list_model_->loaded();
        }));
    mediator_.readingListModel = reading_list_model_;
  }

  void InsertNewWebState(int index) {
    auto web_state = std::make_unique<web::FakeWebState>();
    GURL url("http://test/" + base::NumberToString(index));
    web_state->SetCurrentURL(url);

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    auto main_frame = web::FakeWebFrame::CreateMainWebFrame(
        /*security_origin=*/url);
    main_frame->set_browser_state(browser_state_.get());
    frames_manager->AddWebFrame(std::move(main_frame));
    web::ContentWorld content_world =
        language::LanguageDetectionJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld();
    web_state->SetWebFramesManager(content_world, std::move(frames_manager));

    browser_->GetWebStateList()->InsertWebState(
        index, std::move(web_state), WebStateList::INSERT_FORCE_INDEX,
        WebStateOpener());
  }

  void SetUpActiveWebState() {
    // OverflowMenuMediator expects an language::IOSLanguageDetectionTabHelper
    // for the currently active WebState.
    language::IOSLanguageDetectionTabHelper::CreateForWebState(
        browser_->GetWebStateList()->GetWebStateAt(0),
        /*url_language_histogram=*/nullptr, &language_detection_model_,
        &pref_service_);

    browser_->GetWebStateList()->ActivateWebStateAt(0);
  }

  // Checks that the overflowMenuModel is receiving a number of destination
  // items corresponding to `destination_items` and the action group number
  // corresponding to `action_items` content.
  void CheckMediatorSetItems(NSUInteger destination_items,
                             NSArray<NSNumber*>* action_items) {
    SetUpActiveWebState();
    mediator_.webStateList = browser_->GetWebStateList();
    OverflowMenuModel* model = mediator_.model;

    EXPECT_EQ(destination_items, model.destinations.count);
    EXPECT_EQ(action_items.count, model.actionGroups.count);

    for (NSUInteger index = 0; index < action_items.count; index++) {
      NSNumber* expected_count = action_items[index];
      EXPECT_EQ(expected_count.unsignedIntValue,
                model.actionGroups[index].actions.count);
    }
  }

  bool HasItem(NSString* accessibility_identifier, BOOL enabled) {
    for (OverflowMenuDestination* destination in mediator_.model.destinations) {
      if (destination.accessibilityIdentifier == accessibility_identifier)
        return YES;
    }
    for (OverflowMenuActionGroup* group in mediator_.model.actionGroups) {
      for (OverflowMenuAction* action in group.actions) {
        if (action.accessibilityIdentifier == accessibility_identifier)
          return action.enabled == enabled;
      }
    }
    return NO;
  }

  bool HasEnterpriseInfoItem() {
    for (OverflowMenuActionGroup* group in mediator_.model.actionGroups) {
      if (group.footer.accessibilityIdentifier == kTextMenuEnterpriseInfo)
        return YES;
    }
    return NO;
  }

  bool HasFamilyLinkInfoItem() {
    for (OverflowMenuActionGroup* group in mediator_.model.actionGroups) {
      if (group.footer.accessibilityIdentifier == kTextMenuFamilyLinkInfo) {
        return YES;
      }
    }
    return NO;
  }

  OverflowMenuDestination* GetDestination(NSString* accessibility_identifier) {
    OverflowMenuDestination* found_destination = nil;
    for (OverflowMenuDestination* destination in mediator_.model.destinations) {
      if (destination.accessibilityIdentifier == accessibility_identifier) {
        EXPECT_EQ(nil, found_destination)
            << "there shouldn't be more than one destination with the \""
            << accessibility_identifier << "\" accessibility identifier";
        found_destination = destination;
      }
    }

    return found_destination;
  }

  web::WebTaskEnvironment task_env_;
  // Set a local state for the test ApplicationContext that is scoped to the
  // test (cleaned up on teardown). This is needed for certains factories that
  // gets the local state from the ApplicationContext directly, e.g. the
  // AuthenticationServiceFactory. Valid local state prefs for testing the
  // mediator are usually hosted in `localStatePrefs_`.
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;

  FakeOverlayPresentationContext presentation_context_;
  OverflowMenuModel* model_;
  OverflowMenuMediator* mediator_;
  OverflowMenuOrderer* orderer_;
  BookmarkModel* local_or_syncable_bookmark_model_;
  BookmarkModel* account_bookmark_model_;
  ReadingListModel* reading_list_model_;
  std::unique_ptr<TestingPrefServiceSimple> browserStatePrefs_;
  std::unique_ptr<TestingPrefServiceSimple> localStatePrefs_;
  web::FakeWebState* web_state_;
  std::unique_ptr<web::NavigationItem> navigation_item_;
  UIViewController* baseViewController_;
  translate::LanguageDetectionModel language_detection_model_;
  TestingPrefServiceSimple pref_service_;
};

// Tests that the feature engagement tracker get notified when the mediator is
// disconnected and the tracker wants the notification badge displayed.
TEST_F(OverflowMenuMediatorTest, TestFeatureEngagementDisconnect) {
  CreateMediator(/*is_incognito=*/NO);
  feature_engagement::test::MockTracker tracker;
  EXPECT_CALL(tracker, ShouldTriggerHelpUI(testing::_))
      .WillRepeatedly(Return(true));
  mediator_.engagementTracker = &tracker;

  // Force model update.
  mediator_.model = model_;

  // There may be one or more Tools Menu items that use engagement trackers.
  EXPECT_CALL(tracker, Dismissed(testing::_)).Times(testing::AtLeast(1));
  [mediator_ disconnect];
}

// Tests that the mediator is returning the right number of items and sections
// for the Tools Menu type.
TEST_F(OverflowMenuMediatorTest, TestMenuItemsCount) {
  CreateLocalStatePrefs();
  CreateMediator(/*is_incognito=*/NO);
  mediator_.localStatePrefs = localStatePrefs_.get();
  mediator_.model = model_;

  NSUInteger number_of_action_items = 6;

  if (ios::provider::IsTextZoomEnabled()) {
    number_of_action_items++;
  }

  // New Tab, New Incognito Tab.
  NSUInteger number_of_tab_actions = 2;
  if (IsSplitToolbarMode(mediator_.baseViewController)) {
    // Stop/Reload only shows in split toolbar mode.
    number_of_tab_actions++;
  }
  if (base::ios::IsMultipleScenesSupported()) {
    // New Window option is added in this case.
    number_of_tab_actions++;
  }

  NSUInteger number_of_help_items = 2;

  if (ios::provider::IsUserFeedbackSupported()) {
    number_of_help_items++;
  }

  // Checks that Tools Menu has the right number of items in each section.
  CheckMediatorSetItems(9, @[
    @(number_of_tab_actions),
    // Other actions, depending on configuration.
    @(number_of_action_items),
    // Feedback/help actions.
    @(number_of_help_items),
  ]);
}

// Tests that the items returned by the mediator are correctly enabled on a
// WebPage.
TEST_F(OverflowMenuMediatorTest, TestItemsStatusOnWebPage) {
  CreateLocalStatePrefs();
  CreateMediator(/*is_incognito=*/NO);
  SetUpActiveWebState();
  mediator_.webStateList = browser_->GetWebStateList();
  mediator_.localStatePrefs = localStatePrefs_.get();

  // Force model update.
  mediator_.model = model_;

  web::FakeNavigationContext context;
  web_state_->OnNavigationFinished(&context);

  EXPECT_TRUE(HasItem(kToolsMenuNewTabId, /*enabled=*/YES));
  EXPECT_TRUE(HasItem(kToolsMenuSiteInformation, /*enabled=*/YES));
}

// Tests that the items returned by the mediator are correctly enabled on the
// NTP.
TEST_F(OverflowMenuMediatorTest, TestItemsStatusOnNTP) {
  CreateLocalStatePrefs();
  CreateMediator(/*is_incognito=*/NO);
  SetUpActiveWebState();
  mediator_.webStateList = browser_->GetWebStateList();
  mediator_.localStatePrefs = localStatePrefs_.get();

  // Force model update.
  mediator_.model = model_;

  navigation_item_->SetURL(GURL("chrome://newtab"));
  web::FakeNavigationContext context;
  web_state_->OnNavigationFinished(&context);

  EXPECT_TRUE(HasItem(kToolsMenuNewTabId, /*enabled=*/YES));
  EXPECT_FALSE(HasItem(kToolsMenuSiteInformation, /*enabled=*/YES));
}

// Tests that the "Add to Reading List" button is disabled while overlay UI is
// displayed in OverlayModality::kWebContentArea.
TEST_F(OverflowMenuMediatorTest, TestReadLaterDisabled) {
  const GURL kUrl("https://chromium.test");
  web_state_->SetCurrentURL(kUrl);
  CreateBrowserStatePrefs();
  CreateMediator(/*is_incognito=*/NO);
  SetUpActiveWebState();
  mediator_.webStateList = browser_->GetWebStateList();
  mediator_.webContentAreaOverlayPresenter = OverlayPresenter::FromBrowser(
      browser_.get(), OverlayModality::kWebContentArea);
  mediator_.browserStatePrefs = browserStatePrefs_.get();

  // Force model update.
  mediator_.model = model_;

  ASSERT_TRUE(HasItem(kToolsMenuReadLater, /*enabled=*/YES));

  // Present a JavaScript alert over the WebState and verify that the page is no
  // longer shareable.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state_, OverlayModality::kWebContentArea);
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<JavaScriptAlertDialogRequest>(
          web_state_, kUrl,
          /*is_main_frame=*/true, @"message"));
  EXPECT_TRUE(HasItem(kToolsMenuReadLater, /*enabled=*/NO));

  // Cancel the request and verify that the "Add to Reading List" button is
  // enabled.
  queue->CancelAllRequests();
  EXPECT_TRUE(HasItem(kToolsMenuReadLater, /*enabled=*/YES));
}

// Tests that the "Text Zoom..." button is disabled on non-HTML pages.
TEST_F(OverflowMenuMediatorTest, TestTextZoomDisabled) {
  CreateMediator(/*is_incognito=*/NO);
  SetUpActiveWebState();
  mediator_.webStateList = browser_->GetWebStateList();

  // FontSizeTabHelper requires a web frames manager.
  web_state_->SetWebFramesManager(
      FontSizeJavaScriptFeature::GetInstance()->GetSupportedContentWorld(),
      std::make_unique<web::FakeWebFramesManager>());
  FontSizeTabHelper::CreateForWebState(
      browser_->GetWebStateList()->GetWebStateAt(0));

  // Force model update.
  mediator_.model = model_;

  EXPECT_TRUE(HasItem(kToolsMenuTextZoom, /*enabled=*/YES));

  web_state_->SetContentIsHTML(false);
  // Fake a navigationFinished to force the popup menu items to update.
  web::FakeNavigationContext context;
  web_state_->OnNavigationFinished(&context);
  EXPECT_TRUE(HasItem(kToolsMenuTextZoom, /*enabled=*/NO));
}

// Tests that the "Managed by..." item is hidden when none of the policies is
// set.
TEST_F(OverflowMenuMediatorTest, TestEnterpriseInfoHidden) {
  CreateMediator(/*is_incognito=*/NO);
  SetUpActiveWebState();

  mediator_.webStateList = browser_->GetWebStateList();

  // Force model update.
  mediator_.model = model_;

  ASSERT_FALSE(HasEnterpriseInfoItem());
}
// Tests that the "Managed by..." item is shown for user level policies when
// the UserPolicy features is enabled and the browser is signed in with a
// managed account.
TEST_F(OverflowMenuMediatorTest, TestEnterpriseInfoShownForUserLevelPolicies) {
  // Enable the UserPolicy feature.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{policy::kUserPolicyForSigninOrSyncConsentLevel},
      {});

  // Add managed account to sign in with.
  FakeSystemIdentityManager* fake_system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  fake_system_identity_manager->AddManagedIdentities(@[ @"managedfoo" ]);
  ChromeAccountManagerService* account_manager =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          browser_state_.get());

  // Emulate signing in with managed account.
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
  authentication_service->SignIn(
      account_manager->GetDefaultIdentity(),
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  EXPECT_TRUE(authentication_service->HasPrimaryIdentityManaged(
      signin::ConsentLevel::kSignin));

  CreateMediator(/*is_incognito=*/NO);
  // Set the objects needed to detect the signed in managed account.
  mediator_.authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
  mediator_.browserStatePrefs = browser_state_->GetPrefs();

  // Force model update.
  mediator_.model = model_;

  ASSERT_TRUE(HasEnterpriseInfoItem());
}

// Tests that the "Managed by..." item is shown for machine level policies
// (e.g. from MDM or CBCM).
TEST_F(OverflowMenuMediatorTest,
       TestEnterpriseInfoShownForMachineLevelPolicies) {
  // Set a policy.
  base::ScopedTempDir state_directory;
  ASSERT_TRUE(state_directory.CreateUniqueTempDir());

  std::unique_ptr<EnterprisePolicyTestHelper> enterprise_policy_helper =
      std::make_unique<EnterprisePolicyTestHelper>(state_directory.GetPath());
  BrowserPolicyConnectorIOS* connector =
      enterprise_policy_helper->GetBrowserPolicyConnector();

  policy::PolicyMap map;
  map.Set("test-policy", policy::POLICY_LEVEL_MANDATORY,
          policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
          base::Value("hello"), nullptr);
  enterprise_policy_helper->GetPolicyProvider()->UpdateChromePolicy(map);

  CreateMediatorWithBrowserPolicyConnector(
      /*is_incognito=*/NO, connector);

  SetUpActiveWebState();

  mediator_.webStateList = browser_->GetWebStateList();

  // Force model update.
  mediator_.model = model_;

  ASSERT_TRUE(HasEnterpriseInfoItem());
}

// Tests that the Family Link item is hidden for non-supervised users.
TEST_F(OverflowMenuMediatorTest, TestFamilyLinkInfoHidden) {
  supervised_user::DisableParentalControls(*browser_state_->GetPrefs());

  CreateMediator(/*is_incognito=*/NO);
  SetUpActiveWebState();

  mediator_.webStateList = browser_->GetWebStateList();

  // Force model update.
  mediator_.model = model_;

  ASSERT_FALSE(HasFamilyLinkInfoItem());
}

// Tests that the Family Link item is shown for supervised users.
TEST_F(OverflowMenuMediatorTest, TestFamilyLinkInfoShown) {
  supervised_user::EnableParentalControls(*browser_state_->GetPrefs());

  CreateMediator(/*is_incognito=*/NO);
  SetUpActiveWebState();

  mediator_.webStateList = browser_->GetWebStateList();

  // Force model update.
  mediator_.model = model_;

  ASSERT_TRUE(HasFamilyLinkInfoItem());
}

// Tests that 1) the tools menu has an enabled 'Add to Bookmarks' button when
// the current URL is not in bookmarks 2) the bookmark button changes to an
// enabled 'Edit bookmark' button when navigating to a bookmarked URL, 3) the
// bookmark button changes to 'Add to Bookmarks' when the bookmark is removed.
TEST_F(OverflowMenuMediatorTest, TestBookmarksToolsMenuButtons) {
  const GURL nonBookmarkedURL("https://nonbookmarked.url");
  const GURL bookmarkedURL("https://bookmarked.url");

  web_state_->SetCurrentURL(nonBookmarkedURL);
  SetUpActiveWebState();

  CreateMediator(/*is_incognito=*/NO);
  CreateBrowserStatePrefs();
  SetUpBookmarks();
  // TODO(crbug.com/1448014): Revise this test to ensure account model support.
  bookmarks::AddIfNotBookmarked(local_or_syncable_bookmark_model_,
                                bookmarkedURL,
                                base::SysNSStringToUTF16(@"Test bookmark"));
  mediator_.webStateList = browser_->GetWebStateList();
  mediator_.browserStatePrefs = browserStatePrefs_.get();

  // Force model update.
  mediator_.model = model_;

  EXPECT_TRUE(HasItem(kToolsMenuAddToBookmarks, /*enabled=*/YES));

  // Navigate to bookmarked url
  web_state_->SetCurrentURL(bookmarkedURL);
  web::FakeNavigationContext context;
  web_state_->OnNavigationFinished(&context);

  EXPECT_FALSE(HasItem(kToolsMenuAddToBookmarks, /*enabled=*/YES));
  EXPECT_TRUE(HasItem(kToolsMenuEditBookmark, /*enabled=*/YES));

  local_or_syncable_bookmark_model_->RemoveAllUserBookmarks();
  EXPECT_TRUE(HasItem(kToolsMenuAddToBookmarks, /*enabled=*/YES));
  EXPECT_FALSE(HasItem(kToolsMenuEditBookmark, /*enabled=*/YES));
}

// Tests that the bookmark button is disabled when EditBookmarksEnabled pref is
// changed to false.
TEST_F(OverflowMenuMediatorTest, TestDisableBookmarksButton) {
  const GURL url("https://chromium.test");
  web_state_->SetCurrentURL(url);
  SetUpActiveWebState();

  CreateMediator(/*is_incognito=*/NO);
  CreateBrowserStatePrefs();
  mediator_.webStateList = browser_->GetWebStateList();
  mediator_.browserStatePrefs = browserStatePrefs_.get();

  // Force model update.
  mediator_.model = model_;

  EXPECT_TRUE(HasItem(kToolsMenuAddToBookmarks, /*enabled=*/YES));

  browserStatePrefs_->SetBoolean(bookmarks::prefs::kEditBookmarksEnabled,
                                 false);
  EXPECT_TRUE(HasItem(kToolsMenuAddToBookmarks, /*enabled=*/NO));
}

// Tests that WhatsNew destination was added to the OverflowMenuModel when
// What's New is enabled.
TEST_F(OverflowMenuMediatorTest, TestWhatsNewEnabled) {
  const GURL kUrl("https://chromium.test");
  web_state_->SetCurrentURL(kUrl);
  CreateBrowserStatePrefs();
  CreateLocalStatePrefs();
  CreateMediator(/*is_incognito=*/NO);
  SetUpActiveWebState();
  mediator_.webStateList = browser_->GetWebStateList();
  mediator_.webContentAreaOverlayPresenter = OverlayPresenter::FromBrowser(
      browser_.get(), OverlayModality::kWebContentArea);
  mediator_.browserStatePrefs = browserStatePrefs_.get();
  mediator_.localStatePrefs = localStatePrefs_.get();

  // Force model update.
  mediator_.model = model_;

  EXPECT_TRUE(HasItem(kToolsMenuWhatsNewId, /*enabled=*/YES));
}

// This tests that crbug.com/1404673 does not regress. It isn't perfect, as
// that bug was never reproduced, but it tests part of the issue.
TEST_F(OverflowMenuMediatorTest, TestOpenWhatsNewDoesntCrashWithNoTracker) {
  // Create Mediator and DO NOT set the Tracker on it.
  CreateMediator(/*is_incognito=*/NO);

  std::unique_ptr<MockPromosManager> promos_manager =
      std::make_unique<MockPromosManager>();
  EXPECT_CALL(*promos_manager, DeregisterPromo(testing::_));
  mediator_.promosManager = promos_manager.get();

  // Force model update.
  mediator_.model = model_;

  // Find the What's New destination.
  OverflowMenuDestination* whatsNewDestination;
  for (OverflowMenuDestination* destination in mediator_.model.destinations) {
    if (destination.accessibilityIdentifier == kToolsMenuWhatsNewId) {
      whatsNewDestination = destination;
      break;
    }
  }

  EXPECT_NSNE(nil, whatsNewDestination);

  // Call What's New Destination's handler, which should not crash.
  whatsNewDestination.handler();
}

// Tests that the Settings destination is badged with an error dot and
// positioned at at most kNewDestinationsInsertionIndex when there is an
// eligible identity error that can be resolved from the Settings menu.
TEST_F(OverflowMenuMediatorTest, TestEligibleIdentityErrorWhenSyncOff) {
  CreateMediator(/*is_incognito=*/NO);

  syncer::MockSyncService syncService;
  // Inject eligible identity error in Sync Service.
  ON_CALL(syncService, GetUserActionableError())
      .WillByDefault(Return(kEligibleIdentityErrorWhenSyncOff));
  mediator_.syncService = &syncService;
  CreateLocalStatePrefs();
  mediator_.localStatePrefs = localStatePrefs_.get();
  mediator_.model = model_;

  // Verify that the Settings destination is put at
  // the kNewDestinationsInsertionIndex position and that it has the error
  // badge to indicate the error.
  OverflowMenuDestination* promotedDestination =
      mediator_.model.destinations[kNewDestinationsInsertionIndex];
  EXPECT_NSEQ(kToolsMenuSettingsId,
              promotedDestination.accessibilityIdentifier);
  EXPECT_EQ(BadgeTypeError, promotedDestination.badge);
}

// Tests that there is no error badge displayed on the Settings destination when
// there is no eligible identity error. Sync is OFF.
TEST_F(OverflowMenuMediatorTest, TestNoEligibleIdentityErrorWhenSyncOff) {
  CreateMediator(/*is_incognito=*/NO);

  syncer::MockSyncService syncService;
  ON_CALL(syncService, GetUserActionableError())
      .WillByDefault(Return(kIneligibleIdentityErrorWhenSyncOff));
  mediator_.syncService = &syncService;
  CreateLocalStatePrefs();
  mediator_.localStatePrefs = localStatePrefs_.get();
  mediator_.model = model_;

  // Verify that the Settings destination it still there and does not have the
  // error badge.
  OverflowMenuDestination* settingsDestination =
      GetDestination(kToolsMenuSettingsId);
  ASSERT_NE(nil, settingsDestination);
  EXPECT_EQ(BadgeTypeNone, settingsDestination.badge);
}

// Tests that there is an error badge on the Settings destination when there is
// a Sync error that will be indicated in the Settings menu. The account is
// signed in and has Sync turned ON.
TEST_F(OverflowMenuMediatorTest, TestSyncError) {
  CreateMediator(/*is_incognito=*/NO);

  syncer::MockSyncService syncService;
  // Inject Sync error in Sync Service.
  ON_CALL(syncService, GetUserActionableError())
      .WillByDefault(Return(kIneligibleIdentityErrorWhenSyncOff));
  SetupSyncServiceEnabledExpectations(&syncService);
  mediator_.syncService = &syncService;
  CreateLocalStatePrefs();
  mediator_.localStatePrefs = localStatePrefs_.get();
  mediator_.model = model_;

  // Verify that the Settings destination is put at the front of the
  // destinations and that it has the red dot badge to indicate the error.
  OverflowMenuDestination* promotedDestination =
      mediator_.model.destinations[kNewDestinationsInsertionIndex];
  EXPECT_NSEQ(kToolsMenuSettingsId,
              promotedDestination.accessibilityIdentifier);
  EXPECT_EQ(BadgeTypeError, promotedDestination.badge);
}

// Tests that there is no error cue (red dot) displayed on the Settings
// destination when there is no error in both Sync and Identity levels.
TEST_F(OverflowMenuMediatorTest, TestNoSyncError) {
  CreateMediator(/*is_incognito=*/NO);

  syncer::MockSyncService syncService;
  ON_CALL(syncService, GetUserActionableError())
      .WillByDefault(Return(syncer::SyncService::UserActionableError::kNone));
  SetupSyncServiceEnabledExpectations(&syncService);
  mediator_.syncService = &syncService;
  CreateLocalStatePrefs();
  mediator_.localStatePrefs = localStatePrefs_.get();
  mediator_.model = model_;

  // Verify that the Settings destination it still there and does not have the
  // error badge.
  OverflowMenuDestination* settingsDestination =
      GetDestination(kToolsMenuSettingsId);
  ASSERT_NE(nil, settingsDestination);
  EXPECT_EQ(BadgeTypeNone, settingsDestination.badge);
}

// Tests that the Settings destination that has an error cue has predence over
// the promoted What's New destination.
TEST_F(OverflowMenuMediatorTest, TestIdentityErrorWithWhatsNewPromo) {
  const GURL kUrl("https://chromium.test");
  web_state_->SetCurrentURL(kUrl);
  CreateBrowserStatePrefs();
  CreateMediator(/*is_incognito=*/NO);
  SetUpActiveWebState();
  mediator_.webStateList = browser_->GetWebStateList();
  mediator_.webContentAreaOverlayPresenter = OverlayPresenter::FromBrowser(
      browser_.get(), OverlayModality::kWebContentArea);
  mediator_.browserStatePrefs = browserStatePrefs_.get();
  CreateLocalStatePrefs();
  mediator_.localStatePrefs = localStatePrefs_.get();

  syncer::MockSyncService syncService;
  ON_CALL(syncService, GetUserActionableError())
      .WillByDefault(
          Return(syncer::SyncService::UserActionableError::kNeedsPassphrase));
  mediator_.syncService = &syncService;

  mediator_.model = model_;

  // Verify that the Settings destination is put at the front of the
  // destinations and that What's New is put at the second place.
  EXPECT_NSEQ(kToolsMenuSettingsId,
              mediator_.model.destinations[kNewDestinationsInsertionIndex]
                  .accessibilityIdentifier);
  EXPECT_NSEQ(kToolsMenuWhatsNewId,
              mediator_.model.destinations[kNewDestinationsInsertionIndex + 1]
                  .accessibilityIdentifier);
}

// Tests that the destinations are still promoted when there is no usage
// history ranking.
TEST_F(OverflowMenuMediatorTest,
       TestPromotedDestinationsWhenNoHistoryUsageRanking) {
  CreateMediator(/*is_incognito=*/NO);
  syncer::MockSyncService syncService;
  ON_CALL(syncService, GetUserActionableError())
      .WillByDefault(
          Return(syncer::SyncService::UserActionableError::kNeedsPassphrase));
  mediator_.syncService = &syncService;
  mediator_.model = model_;

  // Verify the destinations to be promoted are put in the right rank and have
  // the right badge.
  EXPECT_NSEQ(kToolsMenuSettingsId,
              mediator_.model.destinations[kNewDestinationsInsertionIndex]
                  .accessibilityIdentifier);
  EXPECT_EQ(BadgeTypeError,
            mediator_.model.destinations[kNewDestinationsInsertionIndex].badge);
  EXPECT_NSEQ(kToolsMenuWhatsNewId,
              mediator_.model.destinations[kNewDestinationsInsertionIndex + 1]
                  .accessibilityIdentifier);
  EXPECT_EQ(
      BadgeTypeNew,
      mediator_.model.destinations[kNewDestinationsInsertionIndex + 1].badge);
  EXPECT_EQ(8U, [mediator_.model.destinations count]);
}

// Tests that the actions have the correct longpress items set.
TEST_F(OverflowMenuMediatorTest, ActionLongpressItems) {
  base::test::ScopedFeatureList scoped_feature_list(kOverflowMenuCustomization);
  CreateMediator(/*is_incognito=*/NO);

  mediator_.model = model_;

  // The 1st action group should contain modifiable actions with 2 longpress
  // items. All other action groups should contain un-modifiable actions with no
  // longpress items.
  for (NSUInteger index = 0; index < mediator_.model.actionGroups.count;
       index++) {
    OverflowMenuActionGroup* actionGroup = mediator_.model.actionGroups[index];
    for (OverflowMenuAction* action in actionGroup.actions) {
      if (index == 1) {
        EXPECT_EQ(action.longPressItems.count, 2ul);
      } else {
        EXPECT_EQ(action.longPressItems.count, 0ul);
      }
    }
  }
}

// Tests that the destinations have the correct longpress items set.
TEST_F(OverflowMenuMediatorTest, DestinationLongpressItems) {
  base::test::ScopedFeatureList scoped_feature_list(kOverflowMenuCustomization);
  CreateMediator(/*is_incognito=*/NO);

  mediator_.model = model_;

  // The 1st action group should contain modifiable actions with 2 longpress
  // items. Settings and Site Info are exceptions, as they are not hideable, so
  // they shold only have one longpress item
  for (OverflowMenuDestination* destination in mediator_.model.destinations) {
    overflow_menu::Destination destinationType =
        static_cast<overflow_menu::Destination>(destination.destination);
    if (destinationType == overflow_menu::Destination::Settings ||
        destinationType == overflow_menu::Destination::SiteInfo) {
      EXPECT_EQ(destination.longPressItems.count, 1ul);
      continue;
    }
    EXPECT_EQ(destination.longPressItems.count, 2ul);
  }
}

// Tests that when a destination becomes hidden during customization, the
// corresponding action gains a subtitle and a highlight.
TEST_F(OverflowMenuMediatorTest, DestinationHideShowsActionSubtitle) {
  base::test::ScopedFeatureList scoped_feature_list(kOverflowMenuCustomization);
  CreateMediator(/*is_incognito=*/NO);

  mediator_.model = model_;

  // Start customization sessions.
  DestinationCustomizationModel* destinationModel =
      orderer_.destinationCustomizationModel;
  ActionCustomizationModel* actionModel = orderer_.actionCustomizationModel;

  // Find destination in customization model.
  OverflowMenuDestination* bookmarksDestination;
  for (OverflowMenuDestination* destination in destinationModel
           .shownDestinations) {
    if (destination.accessibilityIdentifier == kToolsMenuBookmarksId) {
      bookmarksDestination = destination;
      break;
    }
  }
  bookmarksDestination.shown = NO;

  // Find corresponding action.
  OverflowMenuAction* addToBookmarksAction;
  for (OverflowMenuAction* action in actionModel.shownActions) {
    if (action.accessibilityIdentifier == kToolsMenuAddToBookmarks) {
      addToBookmarksAction = action;
      break;
    }
  }

  // Make sure that the corresponding action has a subtitle.
  EXPECT_NE(addToBookmarksAction, nil);
  EXPECT_TRUE(addToBookmarksAction.highlighted);
  EXPECT_NE(addToBookmarksAction.subtitle, nil);
}

// Tests that when the the right metric is recorder when the Password Manager
// item is tapped.
TEST_F(OverflowMenuMediatorTest, OpenPasswordsMetricLogged) {
  CreateMediator(/*is_incognito=*/NO);

  mediator_.model = model_;

  // Find the Password Manager destination.
  OverflowMenuDestination* passwordsDestination;
  for (OverflowMenuDestination* destination in mediator_.model.destinations) {
    if (destination.accessibilityIdentifier == kToolsMenuPasswordsId) {
      passwordsDestination = destination;
      break;
    }
  }
  EXPECT_NSNE(nil, passwordsDestination);

  base::HistogramTester histogram_tester;

  // Verify that bucker count is zero.
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kChromeMenuItem, 0);

  // Call Password Manager destination's handler.
  passwordsDestination.handler();

  // Bucket count should now be one.
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kChromeMenuItem, 1);
}
