// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_egtest_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::TabGridEditButton;

namespace {

// Expectation of how the saved Autofill profile looks like, a map from cell
// name IDs to expected contents.
struct DisplayStringIDToExpectedResult {
  int display_string_id;
  NSString* expected_result;
};

// Will be used to test the country selection logic.
NSString* const kCountryForSelection = @"Germany";

const DisplayStringIDToExpectedResult kExpectedFields[] = {
    {IDS_IOS_AUTOFILL_FULLNAME, @"John H. Doe"},
    {IDS_IOS_AUTOFILL_COMPANY_NAME, @"Underworld"},
    {IDS_IOS_AUTOFILL_ADDRESS1, @"666 Erebus St."},
    {IDS_IOS_AUTOFILL_ADDRESS2, @"Apt 8"},
    {IDS_IOS_AUTOFILL_CITY, @"Elysium"},
    {IDS_IOS_AUTOFILL_STATE, @"CA"},
    {IDS_IOS_AUTOFILL_ZIP, @"91111"},
    {IDS_IOS_AUTOFILL_COUNTRY, @"United States"},
    {IDS_IOS_AUTOFILL_PHONE, @"16502111111"},
    {IDS_IOS_AUTOFILL_EMAIL, @"johndoe@hades.com"}};

NSString* const kProfileLabel = @"John H. Doe, 666 Erebus St.";

// Expectation of how user-typed country names should be canonicalized.
struct UserTypedCountryExpectedResultPair {
  NSString* user_typed_country;
  NSString* expected_result;
};

const UserTypedCountryExpectedResultPair kCountryTests[] = {
    {@"Brasil", @"Brazil"},
    {@"China", @"China mainland"},
    {@"DEUTSCHLAND", @"Germany"},
    {@"GREAT BRITAIN", @"United Kingdom"},
    {@"IN", @"India"},
    {@"JaPaN", @"Japan"},
    {@"JP", @"Japan"},
    {@"Nigeria", @"Nigeria"},
    {@"TW", @"Taiwan"},
    {@"U.S.A.", @"United States"},
    {@"UK", @"United Kingdom"},
    {@"USA", @"United States"},
    {@"Nonexistia", @""},
};

// Return the edit button from the navigation bar.
id<GREYMatcher> NavigationBarEditButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
      grey_not(TabGridEditButton()),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

// Matcher for a country entry with the given accessibility label.
id<GREYMatcher> CountryEntry(NSString* label) {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(label),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the search bar.
id<GREYMatcher> SearchBar() {
  return grey_allOf(grey_accessibilityID(kAutofillCountrySelectionTableViewId),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the search bar's cancel button.
id<GREYMatcher> SearchBarCancelButton() {
  return grey_allOf(ButtonWithAccessibilityLabelId(IDS_APP_CANCEL),
                    grey_kindOfClass([UIButton class]),
                    grey_ancestor(grey_kindOfClass([UISearchBar class])),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the search bar's scrim.
id<GREYMatcher> SearchBarScrim() {
  return grey_accessibilityID(kAutofillCountrySelectionSearchScrimId);
}

}  // namespace

// Various tests for the Autofill profiles section of the settings.
@interface AutofillProfileSettingsTestCase : ChromeTestCase
@end

@implementation AutofillProfileSettingsTestCase

- (void)setUp {
  [super setUp];
  [AutofillAppInterface clearProfilesStore];
}

- (void)tearDown {
  [AutofillAppInterface clearProfilesStore];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  if ([self isRunningTest:@selector(testConfirmationShownOnDeletion)] ||
      [self isRunningTest:@selector(testConfirmationShownOnSwipeToDelete)] ||
      [self isRunningTest:@selector(testCountrySelection)] ||
      [self isRunningTest:@selector(testRequiredFields)] ||
      [self isRunningTest:@selector(testAutoScrollInCountrySelector)] ||
      [self isRunningTest:@selector(testDoneButtonByRequirementsOfCountries)] ||
      [self isRunningTest:@selector(testFooterWithMultipleErrors)]) {
    config.features_enabled.push_back(
        autofill::features::kAutofillAccountProfilesUnionView);
  }

  // Either the test is a duplicate or incompatible with the feature.
  if ([self isRunningTest:@selector(testAutofillProfileEditing)] ||
      [self isRunningTest:@selector(testDeletionOfAddressProfile)]) {
    config.features_disabled.push_back(
        autofill::features::kAutofillAccountProfilesUnionView);
  }

  return config;
}

// Helper to open the settings page for Autofill profiles.
- (void)openAutofillProfilesSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::AddressesAndMoreButton()];
}

// Helper to open the settings page for the Autofill profile with `label`.
- (void)openEditProfile:(NSString*)label {
  [self openAutofillProfilesSettings];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(label)]
      performAction:grey_tap()];
}

// Close the settings.
- (void)exitSettingsMenu {
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  // Wait for UI components to finish loading.
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Helper to open the settings page for the Autofill profile card list in edit
// mode.
- (void)openProfileListInEditMode {
  [self openAutofillProfilesSettings];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
}

// Returns the delete button on the deletion confirmation action sheet.
- (id<GREYMatcher>)confirmButtonForNumberOfAddressesBeingDeleted:
    (int)numberOfAddresses {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetPluralNSStringF(
          IDS_IOS_SETTINGS_AUTOFILL_DELETE_ADDRESS_CONFIRMATION_BUTTON,
          numberOfAddresses)),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_userInteractionEnabled(), nil);
}

// Returns the footer based on the count of errors due to the empty required
// fields.
- (id<GREYMatcher>)footerWithCountOfEmptyRequiredFields:(int)countOfrrors {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetPluralNSStringF(
          IDS_IOS_SETTINGS_EDIT_AUTOFILL_ADDRESS_REQUIREMENT_ERROR,
          countOfrrors)),
      grey_sufficientlyVisible(), nil);
}

// Test that the page for viewing Autofill profile details is as expected.
- (void)testAutofillProfileViewPage {
  [AutofillAppInterface saveExampleProfile];
  [self openEditProfile:kProfileLabel];

  // Check that all fields and values match the expectations.
  for (const DisplayStringIDToExpectedResult& expectation : kExpectedFields) {
    id<GREYMatcher> elementMatcher = nil;
    if (expectation.display_string_id == IDS_IOS_AUTOFILL_COUNTRY) {
      elementMatcher = grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_AUTOFILL_COUNTRY));
    } else {
      elementMatcher = grey_accessibilityLabel(
          [NSString stringWithFormat:@"%@, %@",
                                     l10n_util::GetNSString(
                                         expectation.display_string_id),
                                     expectation.expected_result]);
    }
    [[[EarlGrey
        selectElementWithMatcher:grey_allOf(elementMatcher,
                                            grey_sufficientlyVisible(), nil)]
           usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
        onElementWithMatcher:grey_accessibilityID(
                                 kAutofillProfileEditTableViewId)]
        assertWithMatcher:grey_notNil()];
  }

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Test that editing country names is followed by validating the value and
// replacing it with a canonical one.
- (void)testAutofillProfileEditing {
  [AutofillAppInterface saveExampleProfile];
  [self openEditProfile:kProfileLabel];

  // Keep editing the Country field and verify that validation works.
  for (const UserTypedCountryExpectedResultPair& expectation : kCountryTests) {
    // Switch on edit mode.
    [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
        performAction:grey_tap()];

    // Replace the text field with the user-version of the country.
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                     IDS_IOS_AUTOFILL_COUNTRY)]
        performAction:grey_replaceText(expectation.user_typed_country)];

    // Switch off edit mode.
    [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
        performAction:grey_tap()];

    // Verify that the country value was changed to canonical.
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                     IDS_IOS_AUTOFILL_COUNTRY)]
        assertWithMatcher:grey_text(expectation.expected_result)];
  }

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Test that the page for viewing Autofill profile details is accessible.
- (void)testAccessibilityOnAutofillProfileViewPage {
  [AutofillAppInterface saveExampleProfile];
  [self openEditProfile:kProfileLabel];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Test that the page for editing Autofill profile details is accessible.
- (void)testAccessibilityOnAutofillProfileEditPage {
  [AutofillAppInterface saveExampleProfile];
  [self openEditProfile:kProfileLabel];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Checks that the Autofill profiles list view is in edit mode and the Autofill
// profiles switch is disabled.
- (void)testListViewEditMode {
  [AutofillAppInterface saveExampleProfile];
  [self openProfileListInEditMode];

  // Check the Autofill profile switch is disabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillAddressSwitchViewId,
                                   /*is_toggled_on=*/YES, /*is_enabled=*/NO)]
      assertWithMatcher:grey_notNil()];

  [self exitSettingsMenu];
}

// Checks that the Autofill profile switch can be toggled on/off and the list of
// Autofill profiles is not affected by it.
- (void)testToggleAutofillProfileSwitch {
  [AutofillAppInterface saveExampleProfile];
  [self openAutofillProfilesSettings];

  // Toggle the Autofill profiles switch off.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillAddressSwitchViewId,
                                   /*is_toggled_on=*/YES, /*is_enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Expect Autofill profiles to remain visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(kProfileLabel)]
      assertWithMatcher:grey_notNil()];

  // Toggle the Autofill profiles switch back on.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillAddressSwitchViewId,
                                   /*is_toggled_on=*/NO, /*is_enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

  // Expect Autofill profiles to remain visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(kProfileLabel)]
      assertWithMatcher:grey_notNil()];

  [self exitSettingsMenu];
}

// Checks that the autofill profile is deleted when the deletion is initiated.
- (void)testDeletionOfAddressProfile {
  [AutofillAppInterface saveExampleProfile];
  [self openProfileListInEditMode];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   [AutofillAppInterface exampleProfileName])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      assertWithMatcher:grey_nil()];
  // If the done button in the nav bar is enabled it is no longer in edit
  // mode.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that the confirmation action sheet is shown when an autofill profile
// is deleted and the profile is deleted when the confirmation is accepted.
- (void)testConfirmationShownOnDeletion {
  [AutofillAppInterface saveExampleProfile];
  [self openProfileListInEditMode];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   [AutofillAppInterface exampleProfileName])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 [self confirmButtonForNumberOfAddressesBeingDeleted:1]]
      performAction:grey_tap()];
  WaitForActivityOverlayToDisappear();

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      assertWithMatcher:grey_nil()];
  // If the done button in the nav bar is enabled it is no longer in edit
  // mode.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that the confirmation action sheet is shown when an autofill profile
// is swiped to be deleted.
- (void)testConfirmationShownOnSwipeToDelete {
  [AutofillAppInterface saveExampleProfile];
  [self openAutofillProfilesSettings];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   [AutofillAppInterface exampleProfileName])]
      performAction:chrome_test_util::SwipeToShowDeleteButton()];

  // There are multiple delete buttons but only one enabled.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   ButtonWithAccessibilityLabel(@"Delete"),
                                   grey_not(grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled)),
                                   nil)] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 [self confirmButtonForNumberOfAddressesBeingDeleted:1]]
      performAction:grey_tap()];
  WaitForActivityOverlayToDisappear();

  // Check the profile has been deleted.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   [AutofillAppInterface exampleProfileName])]
      assertWithMatcher:grey_notVisible()];
}

// Checks that the country field is a selection field in the edit mode and the
// newly selected country gets saved in the profile.
- (void)testCountrySelection {
  [AutofillAppInterface saveExampleProfile];
  [self openEditProfile:kProfileLabel];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_AUTOFILL_COUNTRY))]
      performAction:grey_tap()];

  // Focus the search bar.
  [[EarlGrey selectElementWithMatcher:SearchBar()] performAction:grey_tap()];

  // Verify the scrim is visible when search bar is focused but not typed in.
  [[EarlGrey selectElementWithMatcher:SearchBarScrim()]
      assertWithMatcher:grey_notNil()];

  // Verify the cancel button is visible and unfocuses search bar when tapped.
  [[EarlGrey selectElementWithMatcher:SearchBarCancelButton()]
      performAction:grey_tap()];

  // Verify countries are searchable using their name in the current locale.
  [[EarlGrey selectElementWithMatcher:SearchBar()] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SearchBar()]
      performAction:grey_replaceText(kCountryForSelection)];

  // Verify that scrim is not visible anymore.
  [[EarlGrey selectElementWithMatcher:SearchBarScrim()]
      assertWithMatcher:grey_nil()];

  // Verify the `kCountryForSelection` country is visible.
  [[EarlGrey selectElementWithMatcher:CountryEntry(kCountryForSelection)]
      assertWithMatcher:grey_notNil()];

  // Tap on `kCountryForSelection`.
  [[EarlGrey selectElementWithMatcher:CountryEntry(kCountryForSelection)]
      performAction:grey_tap()];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  // Open the profile again.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(kProfileLabel)]
      performAction:grey_tap()];

  // Check `kCountryForSelection` is saved.
  [[EarlGrey selectElementWithMatcher:grey_text(kCountryForSelection)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Checks when the country field is changed to Germany in the edit mode, the
// city is added to the required fields. When it is emptied, the save button in
// displayed. The profile is an account profile.
- (void)testRequiredFields {
  [AutofillAppInterface saveExampleAccountProfile];
  [self openEditProfile:kProfileLabel];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_AUTOFILL_COUNTRY))]
      performAction:grey_tap()];

  // Focus the search bar.
  [[EarlGrey selectElementWithMatcher:SearchBar()] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SearchBar()]
      performAction:grey_replaceText(kCountryForSelection)];

  // Verify that scrim is not visible anymore.
  [[EarlGrey selectElementWithMatcher:SearchBarScrim()]
      assertWithMatcher:grey_nil()];

  // Verify the `kCountryForSelection` country is visible.
  [[EarlGrey selectElementWithMatcher:CountryEntry(kCountryForSelection)]
      assertWithMatcher:grey_notNil()];

  // Tap on `kCountryForSelection`.
  [[EarlGrey selectElementWithMatcher:CountryEntry(kCountryForSelection)]
      performAction:grey_tap()];

  // Remove the text from the state field.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                   IDS_IOS_AUTOFILL_STATE)]
      performAction:grey_replaceText(@"")];

  // The "Done" button is still visible as the state field is not a required
  // field.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Remove the text from the city field.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                   IDS_IOS_AUTOFILL_CITY)]
      performAction:grey_replaceText(@"")];

  // The "Done" button is not enabled now.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Tests that when country selection view opens, the currently selected country
// is in view.
- (void)testAutoScrollInCountrySelector {
  [AutofillAppInterface saveExampleProfile];
  [self openEditProfile:kProfileLabel];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_AUTOFILL_COUNTRY))]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:CountryEntry(@"United States")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that when the state data is removed, the "Done" button is enabled for
// "Germany" but not for "India". Similarly, the "Done" is disabled for "US".
- (void)testDoneButtonByRequirementsOfCountries {
  [AutofillAppInterface saveExampleAccountProfile];
  [self openEditProfile:kProfileLabel];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  // Change text of state to empty.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                   IDS_IOS_AUTOFILL_STATE)]
      performAction:grey_replaceText(@"")];

  // The "Done" button should not be enabled now since "State" is a required
  // field for "United States".
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_AUTOFILL_COUNTRY))]
      performAction:grey_tap()];

  // Focus the search bar.
  [[EarlGrey selectElementWithMatcher:SearchBar()] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SearchBar()]
      performAction:grey_replaceText(kCountryForSelection)];

  // Verify the `kCountryForSelection` country is visible.
  [[EarlGrey selectElementWithMatcher:CountryEntry(kCountryForSelection)]
      assertWithMatcher:grey_notNil()];

  // Tap on `kCountryForSelection`.
  [[EarlGrey selectElementWithMatcher:CountryEntry(kCountryForSelection)]
      performAction:grey_tap()];

  // The "Done" button should be enabled since "State" is not a required field
  // for "Germany".
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_enabled()];

  // Tap on Country and select "India" now.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_AUTOFILL_COUNTRY))]
      performAction:grey_tap()];

  // Focus the search bar.
  [[EarlGrey selectElementWithMatcher:SearchBar()] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SearchBar()]
      performAction:grey_replaceText(@"India")];

  // Verify the "India" is visible.
  [[EarlGrey selectElementWithMatcher:CountryEntry(@"India")]
      assertWithMatcher:grey_notNil()];

  // Tap on "India".
  [[EarlGrey selectElementWithMatcher:CountryEntry(@"India")]
      performAction:grey_tap()];

  // The "Done" button should not be enabled now since "State" is a required
  // field for "India".
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Tests that the footer text is correctly displayed when there are multiple
// required empty fields.
- (void)testFooterWithMultipleErrors {
  [AutofillAppInterface saveExampleAccountProfile];
  [self openEditProfile:kProfileLabel];

  // Change text of city to empty.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                   IDS_IOS_AUTOFILL_CITY)]
      performAction:grey_replaceText(@"")];

  [[EarlGrey
      selectElementWithMatcher:[self footerWithCountOfEmptyRequiredFields:1]]
      assertWithMatcher:grey_nil()];

  // Change text of state to empty.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                   IDS_IOS_AUTOFILL_STATE)]
      performAction:grey_replaceText(@"")];

  [[EarlGrey
      selectElementWithMatcher:[self footerWithCountOfEmptyRequiredFields:2]]
      assertWithMatcher:grey_nil()];
}

@end
