// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_table_view_controller.h"

#import "base/guid.h"
#import "base/mac/foundation_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/ui/settings/personal_data_manager_finished_profile_tasks_waiter.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"
#import "ios/chrome/browser/webdata_services/web_data_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class AutofillProfileTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  AutofillProfileTableViewControllerTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    // Profile import requires a PersonalDataManager which itself needs the
    // WebDataService; this is not initialized on a TestChromeBrowserState by
    // default.
    test_cbs_builder.AddTestingFactory(
        ios::WebDataServiceFactory::GetInstance(),
        ios::WebDataServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        chrome_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());

    // Set circular SyncService dependency to null.
    autofill::PersonalDataManagerFactory::GetForBrowserState(
        chrome_browser_state_.get())
        ->SetSyncServiceForTest(nullptr);
  }

  ChromeTableViewController* InstantiateController() override {
    return [[AutofillProfileTableViewController alloc]
        initWithBrowser:browser_.get()];
  }

  void TearDown() override {
    [base::mac::ObjCCastStrict<AutofillProfileTableViewController>(controller())
        settingsWillBeDismissed];
    ChromeTableViewControllerTest::TearDown();
  }

  void AddProfile(const std::string& origin,
                  const std::string& name,
                  const std::string& address) {
    autofill::PersonalDataManager* personal_data_manager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            chrome_browser_state_.get());
    if (base::FeatureList::IsEnabled(
            autofill::features::kAutofillUseAlternativeStateNameMap)) {
      personal_data_manager->personal_data_manager_cleaner_for_testing()
          ->alternative_state_name_map_updater_for_testing()
          ->set_local_state_for_testing(local_state_.Get());
    }
    personal_data_manager->SetSyncServiceForTest(nullptr);
    PersonalDataManagerFinishedProfileTasksWaiter waiter(personal_data_manager);

    autofill::AutofillProfile autofill_profile(base::GenerateGUID(), origin);
    autofill_profile.SetRawInfo(autofill::NAME_FULL, base::ASCIIToUTF16(name));
    autofill_profile.SetRawInfo(autofill::ADDRESS_HOME_LINE1,
                                base::ASCIIToUTF16(address));
    personal_data_manager->SaveImportedProfile(autofill_profile);
    waiter.Wait();  // Wait for completion of the asynchronous operation.
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
};

// Default test case of no addresses.
TEST_F(AutofillProfileTableViewControllerTest, TestInitialization) {
  ChromeTableViewController* controller =
      ChromeTableViewControllerTest::controller();
  CheckController();

  // Expect only the header section.
  EXPECT_EQ(1, NumberOfSections());
  // Expect header section to contain one row (the address Autofill toggle).
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  // Expect subtitle section to contain one row (the address Autofill toggle
  // subtitle).
  EXPECT_NE(nil, [controller.tableViewModel footerForSectionIndex:0]);

  // Check the footer of the first section.
  CheckSectionFooterWithId(IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_SUBLABEL, 0);
}

// Adding a single address results in an address section.
TEST_F(AutofillProfileTableViewControllerTest, TestOneProfile) {
  AddProfile("https://www.example.com/", "John Doe", "1 Main Street");
  CreateController();
  CheckController();

  // Expect two sections (header, and addresses section).
  EXPECT_EQ(2, NumberOfSections());
  // Expect address section to contain one row (the address itself).
  EXPECT_EQ(1, NumberOfItemsInSection(1));
}

// Deleting the only profile results in item deletion and section deletion.
TEST_F(AutofillProfileTableViewControllerTest, TestOneProfileItemDeleted) {
  AddProfile("https://www.example.com/", "John Doe", "1 Main Street");
  CreateController();
  CheckController();

  // Expect two sections (header and addresses section).
  EXPECT_EQ(2, NumberOfSections());
  // Expect address section to contain one row (the address itself).
  EXPECT_EQ(1, NumberOfItemsInSection(1));

  AutofillProfileTableViewController* view_controller =
      base::mac::ObjCCastStrict<AutofillProfileTableViewController>(
          controller());
  // Put the tableView in 'edit' mode.
  [view_controller editButtonPressed];

  AutofillProfileTableViewController* autofill_controller =
      static_cast<AutofillProfileTableViewController*>(controller());
  [autofill_controller deleteItems:@[ [NSIndexPath indexPathForRow:0
                                                         inSection:1] ]];

  // Verify the resulting UI.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return NumberOfSections() == 1;
      }));

  // Exit 'edit' mode.
  [view_controller editButtonPressed];

  // Expect address section to have been removed.
  EXPECT_EQ(1, NumberOfSections());
}

}  // namespace
