// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/add_password_view_controller.h"

#import <memory>

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/password_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
constexpr char kPassword[] = "test";
}

@interface AddPasswordViewController (ForTesting)

- (void)setPassword:(NSString*)password;

@end

// Test class that conforms to AddPasswordViewControllerDelegate in order to
// test the delegate methods are called correctly.
@interface FakeAddPasswordDelegate
    : NSObject <AddPasswordViewControllerDelegate>

@property(nonatomic, strong) PasswordDetails* password;

@end

@implementation FakeAddPasswordDelegate

- (void)addPasswordViewController:(AddPasswordViewController*)viewController
           didEditPasswordDetails:(PasswordDetails*)password {
  self.password = password;
}

- (void)addPasswordViewController:(AddPasswordViewController*)viewController
            didAddPasswordDetails:(NSString*)username
                         password:(NSString*)password
                             note:(NSString*)note {
}

- (void)checkForDuplicates:(NSString*)username {
}

- (void)showExistingCredential:(NSString*)username {
}

- (void)didCancelAddPasswordDetails {
}

- (void)setWebsiteURL:(NSString*)website {
}

- (BOOL)isURLValid {
  return YES;
}

- (BOOL)isTLDMissing {
  return NO;
}

@end

// Unit tests for PasswordIssuesTableViewController.
class AddPasswordViewControllerTest : public ChromeTableViewControllerTest {
 protected:
  AddPasswordViewControllerTest() {
    delegate_ = [[FakeAddPasswordDelegate alloc] init];
  }

  ChromeTableViewController* InstantiateController() override {
    AddPasswordViewController* controller =
        [[AddPasswordViewController alloc] init];
    controller.delegate = delegate_;
    return controller;
  }

  void SetPassword(std::string password = kPassword) {
    AddPasswordViewController* passwords_controller =
        static_cast<AddPasswordViewController*>(controller());
    [passwords_controller loadModel];
    [passwords_controller setPassword:base::SysUTF8ToNSString(password)];
  }

  void CheckEditCellText(NSString* expected_text, int section, int item) {
    TableViewTextEditItem* cell =
        static_cast<TableViewTextEditItem*>(GetTableViewItem(section, item));
    EXPECT_NSEQ(expected_text, cell.textFieldValue);
  }

  void SetEditCellText(NSString* text, int section, int item) {
    TableViewTextEditItem* cell =
        static_cast<TableViewTextEditItem*>(GetTableViewItem(section, item));
    cell.textFieldValue = text;
  }

  void CheckDetailItemTextWithId(int expected_detail_text_id,
                                 int section,
                                 int item) {
    SettingsImageDetailTextItem* cell =
        static_cast<SettingsImageDetailTextItem*>(
            GetTableViewItem(section, item));
    EXPECT_NSEQ(l10n_util::GetNSString(expected_detail_text_id),
                cell.detailText);
  }

  FakeAddPasswordDelegate* delegate() { return delegate_; }

 private:
  FakeAddPasswordDelegate* delegate_ = nil;
};

// Tests that password is shown/hidden.
TEST_F(AddPasswordViewControllerTest, TestShowHidePassword) {
  SetPassword();
  NSIndexPath* indexOfPassword;
  CheckEditCellText(kMaskedPassword, 2, 1);
  indexOfPassword = [NSIndexPath indexPathForRow:1 inSection:2];

  TableViewTextEditCell* textFieldCell =
      base::mac::ObjCCastStrict<TableViewTextEditCell>([controller()
                      tableView:controller().tableView
          cellForRowAtIndexPath:indexOfPassword]);
  EXPECT_TRUE(textFieldCell);
  [textFieldCell.identifyingIconButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  CheckEditCellText(@"test", 2, 1);

  [textFieldCell.identifyingIconButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  CheckEditCellText(kMaskedPassword, 2, 1);
}

// Tests the layout of the view controller when adding a new credential.
TEST_F(AddPasswordViewControllerTest, TestSectionsInAdd) {
  AddPasswordViewController* passwords_controller =
      static_cast<AddPasswordViewController*>(controller());
  [passwords_controller loadModel];

  EXPECT_EQ(4, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  EXPECT_EQ(0, NumberOfItemsInSection(1));
  EXPECT_EQ(2, NumberOfItemsInSection(2));

  CheckSectionFooter(
      [NSString stringWithFormat:@"%@\n\n%@",
                                 l10n_util::GetNSString(
                                     IDS_IOS_SETTINGS_ADD_PASSWORD_DESCRIPTION),
                                 l10n_util::GetNSString(
                                     IDS_IOS_SAVE_PASSWORD_FOOTER_NOT_SYNCING)],
      3);
}

// Tests the layout of the view controller when adding a new credential with
// notes features disabled.
TEST_F(AddPasswordViewControllerTest, TestSectionsInAddWithNotesDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(syncer::kPasswordNotesWithBackup);

  AddPasswordViewController* passwords_controller =
      static_cast<AddPasswordViewController*>(controller());
  [passwords_controller loadModel];

  EXPECT_EQ(4, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  EXPECT_EQ(0, NumberOfItemsInSection(1));
  EXPECT_EQ(2, NumberOfItemsInSection(2));

  CheckSectionFooter(
      [NSString stringWithFormat:@"%@\n\n%@",
                                 l10n_util::GetNSString(
                                     IDS_IOS_SETTINGS_ADD_PASSWORD_DESCRIPTION),
                                 l10n_util::GetNSString(
                                     IDS_IOS_SAVE_PASSWORD_FOOTER_NOT_SYNCING)],
      3);
}

// Tests the layout of the view controller when adding a new credential with
// notes features enabled.
TEST_F(AddPasswordViewControllerTest, TestSectionsInAddWithNotesEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);

  AddPasswordViewController* passwords_controller =
      static_cast<AddPasswordViewController*>(controller());
  [passwords_controller loadModel];

  EXPECT_EQ(5, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  EXPECT_EQ(0, NumberOfItemsInSection(1));
  EXPECT_EQ(3, NumberOfItemsInSection(2));

  CheckSectionFooter(
      [NSString stringWithFormat:@"%@\n\n%@",
                                 l10n_util::GetNSString(
                                     IDS_IOS_SETTINGS_ADD_PASSWORD_DESCRIPTION),
                                 l10n_util::GetNSString(
                                     IDS_IOS_SAVE_PASSWORD_FOOTER_NOT_SYNCING)],
      4);
}

// Tests the layout of the view controller when adding a new credential with
// duplicate website/username combination.
TEST_F(AddPasswordViewControllerTest, TestSectionsInAddDuplicated) {
  SetPassword();

  AddPasswordViewController* passwords_controller =
      static_cast<AddPasswordViewController*>(controller());
  [passwords_controller loadModel];

  SetEditCellText(@"http://www.example.com/", 0, 0);
  SetEditCellText(@"test@egmail.com", 2, 0);

  [passwords_controller onDuplicateCheckCompletion:YES];

  EXPECT_EQ(5, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  EXPECT_EQ(0, NumberOfItemsInSection(1));
  EXPECT_EQ(2, NumberOfItemsInSection(2));
  EXPECT_EQ(2, NumberOfItemsInSection(3));
}

// Tests the footer text of the view controller when adding a new credential and
// the user email address is provided.
TEST_F(AddPasswordViewControllerTest, TestFooterTextWithEmail) {
  AddPasswordViewController* passwords_controller =
      static_cast<AddPasswordViewController*>(controller());
  [passwords_controller setAccountSavingPasswords:@"example@gmail.com"];
  [passwords_controller loadModel];

  CheckSectionFooter(
      [NSString
          stringWithFormat:@"%@\n\n%@",
                           l10n_util::GetNSString(
                               IDS_IOS_SETTINGS_ADD_PASSWORD_DESCRIPTION),
                           l10n_util::GetNSStringF(
                               IDS_IOS_SETTINGS_ADD_PASSWORD_FOOTER_BRANDED,
                               u"example@gmail.com")],
      3);
}
