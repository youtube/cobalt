// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>
#import <string_view>

#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/common/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/bottom_sheet_constants.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_constants.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/infobars/ui_bundled/infobar_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_address_profile_modal_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

namespace {

// URLs of the test pages.
constexpr char kProfileForm[] = "/autofill_smoke_test.html";
constexpr char kXframeFormPage[] = "/xhr_xframe_submit.html";
constexpr char kFullAddressFormPage[] = "/full_address_form.html";

// Ids of fields in the form.
constexpr char kFormElementName[] = "form_name";
constexpr char kFormElementEmail[] = "form_email";
constexpr char kFormElementSubmit[] = "submit_profile";

// Minimal cooldown period to wait for between typing characters. The bare
// minimum should be the frame rate, something aroung 17ms (1/60hz), but we
// prefer giving extra buffer as there are probably other latencies.
constexpr base::TimeDelta kTypingCoolDownPeriod = base::Milliseconds(50);

// Email value used by the tests.
constexpr char kEmail[] = "foo1@gmail.com";

// Histogram bucket representing renderer errors.
constexpr int kRendererErrorHistogramBucket = 8;

struct FullAddressFormPageParams {
  // True if the submission should be default prevented.
  bool default_prevented = false;
  // True if there should be redirection done after submitting with
  // a parameter that can stop the submit event from being handled by Autofill.
  bool redirect = false;
  // True if the submission should be prevented from propagating to any other
  // listener regardless of their positioning.
  bool stop_immediate_propagation = false;
};

// Matcher for the banner button.
id<GREYMatcher> BannerButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabel(l10n_util::GetNSString(
      IDS_IOS_AUTOFILL_SAVE_ADDRESS_MESSAGE_PRIMARY_ACTION));
}

// Matcher for the update banner button.
id<GREYMatcher> UpdateBannerButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabel(l10n_util::GetNSString(
      IDS_IOS_AUTOFILL_UPDATE_ADDRESS_MESSAGE_PRIMARY_ACTION));
}

// Matcher for the "Save Address" modal button.
id<GREYMatcher> ModalButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL));
}

// Matcher for the "Update Address" modal button.
id<GREYMatcher> UpdateModalButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabel(l10n_util::GetNSString(
      IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL));
}

// Matcher for the modal button.
id<GREYMatcher> ModalEditButtonMatcher() {
  return grey_allOf(grey_accessibilityID(kInfobarSaveAddressModalEditButton),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Matcher for the migration button in modal view.
id<GREYMatcher> ModalMigrationButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabel(l10n_util::GetNSString(
      IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_OK_BUTTON_LABEL));
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
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabelId(IDS_APP_CANCEL),
      grey_kindOfClass([UIButton class]),
      grey_ancestor(grey_kindOfClass([UISearchBar class])),
      grey_sufficientlyVisible(), nil);
}

// Matcher for the search bar's scrim.
id<GREYMatcher> SearchBarScrim() {
  return grey_accessibilityID(kAutofillCountrySelectionSearchScrimId);
}

id<GREYMatcher> TextFieldWithLabel(NSString* textFieldLabel) {
  return grey_allOf(grey_accessibilityID(
                        [textFieldLabel stringByAppendingString:@"_textField"]),
                    grey_kindOfClass([UITextField class]), nil);
}

id<GREYMatcher> EditProfileBottomSheet() {
  return grey_accessibilityID(kEditProfileBottomSheetViewIdentfier);
}

// Slowly type characters using the keyboard by waiting between each tap.
void SlowlyTypeText(NSString* text) {
  for (NSUInteger i = 0; i < [text length]; ++i) {
    // Wait some time before typing the character.
    base::test::ios::SpinRunLoopWithMinDelay(kTypingCoolDownPeriod);
    // Type a single character so the user input can be effective.
    [ChromeEarlGrey
        simulatePhysicalKeyboardEvent:[text
                                          substringWithRange:NSMakeRange(i, 1)]
                                flags:0];
  }
  // Give some cooldown period so the character has the time to be typed
  // before doing something else on the page.
  base::test::ios::SpinRunLoopWithMinDelay(kTypingCoolDownPeriod);
}

void TypeTextInXframeField(NSString* fieldID, NSString* text) {
  // Focus on the field that corresponds to `fieldID` to pop up the keyboard.
  NSString* script = [NSString
      stringWithFormat:@"document.querySelector('iframe')"
                        ".contentDocument.getElementById('%@').focus()",
                       fieldID];
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];

  // Type the `text` on the field.
  SlowlyTypeText(text);
}

}  // namespace

@interface SaveProfileEGTest : ChromeTestCase

@end

@implementation SaveProfileEGTest

- (void)setUp {
  [super setUp];
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
}

- (void)tearDownHelper {
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);

  // Clear existing profile.
  [AutofillAppInterface clearProfilesStore];

  [super tearDownHelper];
}

// TODO(crbug.com/391826905): Re-enable this test on simulator.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testEditBottomSheetAlertBySwipingDown \
  FLAKY_testEditBottomSheetAlertBySwipingDown
#else
#define MAYBE_testEditBottomSheetAlertBySwipingDown \
  testEditBottomSheetAlertBySwipingDown
#endif
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  if ([self isRunningTest:@selector(testUserData_LocalEditViaBottomSheet)] ||
      [self
          isRunningTest:@selector(testUserData_LocalHideBottomSheetOnCancel)] ||
      [self isRunningTest:@selector
            (MAYBE_testEditBottomSheetAlertBySwipingDown)]) {
    config.features_enabled.push_back(
        kAutofillDynamicallyLoadsFieldsForAddressInput);
  }

  config.features_disabled.push_back(
      autofill::features::test::kAutofillServerCommunication);

  if ([self isRunningTest:@selector(testStickySavePromptJourney)]) {
    config.features_enabled.push_back(kAutofillStickyInfobarIos);
  }

  if ([self isRunningTest:@selector
            (testUserData_AccountSave_AutofillAcrossIframe_XHR)]) {
    config.features_enabled.push_back(
        autofill::features::kAutofillAcrossIframesIos);
    config.features_enabled.push_back(kAutofillFixXhrForXframe);
  }

  if ([self isRunningTest:@selector
            (testSubmissionDetection_defaultPrevented_whenAllowed)]) {
    config.features_enabled.push_back(kAutofillAllowDefaultPreventedSubmission);
  }

  if ([self isRunningTest:@selector
            (testSubmissionDetection_defaultPrevented_whenNotAllowed)]) {
    config.features_disabled.push_back(
        kAutofillAllowDefaultPreventedSubmission);
  }

  if ([self isRunningTest:@selector(testSubmissionDetectionWithDeduping)]) {
    config.features_enabled.push_back(kAutofillDedupeFormSubmission);
    // Default must be prevented to allow triggering multiple submissions from
    // the same form.
    config.features_enabled.push_back(kAutofillAllowDefaultPreventedSubmission);
  }

  if ([self isRunningTest:@selector(testSubmissionDetectionWithoutDeduping)]) {
    config.features_disabled.push_back(kAutofillDedupeFormSubmission);
    // Default must be prevented to allow triggering multiple submissions from
    // the same form.
    config.features_enabled.push_back(kAutofillAllowDefaultPreventedSubmission);
  }

  if ([self isRunningTest:@selector(testSubmissionDetection_inCaptureMode)]) {
    config.features_enabled.push_back(
        kAutofillFormSubmissionEventsInCaptureMode);
  }

  if ([self
          isRunningTest:@selector(testSubmissionDetection_notInCaptureMode)]) {
    config.features_disabled.push_back(
        kAutofillFormSubmissionEventsInCaptureMode);
  }

  if ([self isRunningTest:@selector(testSubmissionErrorReporting_Enabled)]) {
    config.features_enabled.push_back(kAutofillIsolatedWorldForJavascriptIos);
    config.features_enabled.push_back(kAutofillReportFormSubmissionErrors);
  }

  if ([self isRunningTest:@selector(testSubmissionErrorReporting_Disabled)]) {
    config.features_enabled.push_back(kAutofillIsolatedWorldForJavascriptIos);
    config.features_disabled.push_back(kAutofillReportFormSubmissionErrors);
  }

  return config;
}

#pragma mark - Test helper methods

// Fills the president profile in the form by clicking on the button, submits
// the form to the save address profile infobar.
- (void)fillPresidentProfileAndShowSaveInfobar {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kProfileForm)];

  // Ensure there are no saved profiles.
  GREYAssertEqual(0U, [AutofillAppInterface profilesCount],
                  @"There should be no saved profile.");

  [ChromeEarlGrey tapWebStateElementWithID:@"fill_profile_president"];
  [ChromeEarlGrey tapWebStateElementWithID:@"submit_profile"];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];
}

// Triggers the save infobar via XHR submission.
- (void)triggerSaveInfobarViaXHRSubmission {
  // Load the xframe address form page.
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kXframeFormPage)];

  // Ensure there are no saved profiles at this point - make sure we start from
  // a clean slate.
  GREYAssertEqual(0U, [AutofillAppInterface profilesCount],
                  @"There should be no saved profile.");

  // Manually type the text in the fields so profile saving can be triggered
  // when autofill across iframes is enabled which require manually editing the
  // fields.
  TypeTextInXframeField(@"form_name", @"User");
  TypeTextInXframeField(@"form_address", @"1234 Pkw Ave");
  TypeTextInXframeField(@"form_city", @"MuteCity");
  TypeTextInXframeField(@"form_zip", @"12345");

  // Trigger XHR submission in the child frame using the dedicated button in the
  // main frame.
  [ChromeEarlGrey tapWebStateElementWithID:@"do-xhr-submit"];

  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];
}

- (void)fillPresidentProfileAndShowSaveModal {
  [self fillPresidentProfileAndShowSaveInfobar];

  // Accept the banner.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];
}

// Fills, submits the form and saves the address profile.
- (void)fillFormAndSaveProfile {
  [self fillPresidentProfileAndShowSaveModal];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");
}

// Loads, fills, and submits the full address form.
- (void)loadFullAddressFormWithParams:(FullAddressFormPageParams)params {
  // Start server.
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  auto makeQueryString = [](FullAddressFormPageParams params) -> std::string {
    std::vector<std::string_view> queryParameters;
    if (params.default_prevented) {
      queryParameters.push_back("preventDefault");
    }
    if (params.redirect) {
      queryParameters.push_back("redirectWhenSubmissionPrevented");
    }
    if (params.stop_immediate_propagation) {
      queryParameters.push_back("stopImmediatePropagation");
    }
    return base::JoinString(queryParameters, "&");
  };

  // Get the URL for the served test page with the query parameters for setting
  // it up.
  const GURL baseURL = self.testServer->GetURL(kFullAddressFormPage);
  GURL::Replacements replacements;
  std::string query = makeQueryString(params).c_str();
  replacements.SetQueryStr(query);
  const GURL fullURL = baseURL.ReplaceComponents(replacements);

  // Load the URL and wait for its content to be loaded.
  [ChromeEarlGrey loadURL:fullURL];

  // Call the helper function embedded in the page content to fill the form.
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:@"FillForm();"];
}

- (void)loadAndSubmitFullAddressFormWithParams:
    (FullAddressFormPageParams)params {
  [self loadFullAddressFormWithParams:params];
  // Submit the form via the dedicated <button>.
  [ChromeEarlGrey tapWebStateElementWithID:@"submit-button"];
}

// Focuses on the name field and initiates autofill on the form with the saved
// profile.
- (void)focusOnNameAndAutofill {
  // Ensure there is a saved local profile.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"There should a saved local profile.");

  // Tap on a field to trigger form activity.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Wait for the keyboard to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Wait for suggestions as it may take some time because of form fetch
  // throttling or other delays.
  [ChromeEarlGrey
      waitForMatcher:chrome_test_util::AutofillSuggestionViewMatcher()];

  // Tap on the suggestion.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          AutofillSuggestionViewMatcher()]
      performAction:grey_tap()];

  // Verify Web Content was filled.
  NSString* javaScriptCondition = [NSString
      stringWithFormat:@"document.getElementById('%s').value.length > 0",
                       kFormElementName];
  [ChromeEarlGrey waitForJavaScriptCondition:javaScriptCondition];
}

#pragma mark - Tests

// Ensures that the profile is updated after submitting the form.
- (void)testUserData_LocalUpdate {
  // Save a profile.
  [self fillFormAndSaveProfile];

  // Load the form.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kProfileForm)];

  // Autofill the form.
  [self focusOnNameAndAutofill];

  // Tap on email field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementEmail)];

  // Wait for the keyboard to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Populate the email field.
  // TODO(crbug.com/40916974): This should use grey_typeText when fixed.
  for (int i = 0; kEmail[i] != '\0'; ++i) {
    NSString* letter = base::SysUTF8ToNSString(std::string(1, kEmail[i]));
    if (kEmail[i] == '@') {
      [ChromeEarlGrey simulatePhysicalKeyboardEvent:letter
                                              flags:UIKeyModifierShift];
      continue;
    }

    [ChromeEarlGrey simulatePhysicalKeyboardEvent:letter flags:0];
  }

  // Submit the form.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementSubmit)];

  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];

  // Accept the banner.
  [[EarlGrey selectElementWithMatcher:UpdateBannerButtonMatcher()]
      performAction:grey_tap()];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  // Update the profile.
  [[EarlGrey selectElementWithMatcher:UpdateModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is updated locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been updated.");
}

// Ensures that the profile is saved to Chrome after submitting and editing the
// form.
- (void)testUserData_LocalEdit {
  if ([AutofillAppInterface isDynamicallyLoadFieldsOnInputEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"This test is not relevant when the fields "
                           @"are loaded dynamically on input.");
  }

  // Fill and submit the form.
  [self fillPresidentProfileAndShowSaveModal];

  // Edit the profile.
  [[EarlGrey selectElementWithMatcher:ModalEditButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                   IDS_IOS_AUTOFILL_CITY)]
      performAction:grey_replaceText(@"New York")];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");
}

// Ensures that the profile is saved to Account after submitting the form.
- (void)testUserData_AccountSave {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  [self fillPresidentProfileAndShowSaveModal];

  id<GREYMatcher> footerMatcher = grey_text(
      l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER,
                              base::UTF8ToUTF16(std::string(kEmail))));

  [[EarlGrey selectElementWithMatcher:footerMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  [SigninEarlGrey signOut];
}

// Test that the profile can be saved for the edge case where the address form
// is hosted in a frame and is submitted there via XHR - when autofill across
// iframes is enabled.
- (void)testUserData_AccountSave_AutofillAcrossIframe_XHR {
  // Sign-in so the profile can be saved into the account.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Trigger the save infobar via XHR submission in the child frame.
  [self triggerSaveInfobarViaXHRSubmission];

  // Accept the banner to save the profile.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  [SigninEarlGrey signOut];
}

// Ensures that the profile is saved to Account after submitting and editing the
// form.
- (void)testUserData_AccountEdit {
  if ([AutofillAppInterface isDynamicallyLoadFieldsOnInputEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"This test is not relevant when the fields "
                           @"are loaded dynamically on input.");
  }

  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  [self fillPresidentProfileAndShowSaveModal];

  // Edit the profile.
  [[EarlGrey selectElementWithMatcher:ModalEditButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TextFieldForCellWithLabelId(
                                   IDS_IOS_AUTOFILL_CITY)]
      performAction:grey_replaceText(@"New York")];

  id<GREYMatcher> footerMatcher = grey_text(
      l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER,
                              base::UTF8ToUTF16(std::string(kEmail))));

  [[EarlGrey selectElementWithMatcher:footerMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  [SigninEarlGrey signOut];
}

// Ensures that if a local profile is filled in a form and submitted, the user
// is asked for a migration prompt and the profile is moved to the Account.
- (void)testUserData_MigrationToAccount {
  [AutofillAppInterface clearProfilesStore];

  // Store one local address.
  [AutofillAppInterface saveExampleProfile];

  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kProfileForm)];

  [self focusOnNameAndAutofill];

  // Submit the form.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementSubmit)];

  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];

  // Accept the banner.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  id<GREYMatcher> footerMatcher = grey_text(l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_ADDRESS_MIGRATE_IN_ACCOUNT_FOOTER,
      base::UTF8ToUTF16(std::string(kEmail))));
  // Check if there is footer suggesting it's a migration prompt.
  [[EarlGrey selectElementWithMatcher:footerMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Confirm to migrate the profile.
  [[EarlGrey selectElementWithMatcher:ModalMigrationButtonMatcher()]
      performAction:grey_tap()];

  // Ensure the count of profiles saved remains unchanged.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  [SigninEarlGrey signOut];
}

// Tests that the user can edit a field in the edit via in the save address
// flow.
- (void)testUserData_LocalEditViaBottomSheet {
  // Fill and submit the form.
  [self fillPresidentProfileAndShowSaveModal];

  // Edit the profile.
  [[EarlGrey selectElementWithMatcher:ModalEditButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(@"City")]
      performAction:grey_replaceText(@"New York")];

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
      performAction:grey_replaceText(@"Germany")];

  // Verify that scrim is not visible anymore.
  [[EarlGrey selectElementWithMatcher:SearchBarScrim()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:CountryEntry(@"Germany")]
      performAction:grey_tap()];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");
}

// Tests that the bottom sheet to edit address is just hidden on Cancel.
- (void)testUserData_LocalHideBottomSheetOnCancel {
  // Fill and submit the form.
  [self fillPresidentProfileAndShowSaveModal];

  // Edit the profile.
  [[EarlGrey selectElementWithMatcher:ModalEditButtonMatcher()]
      performAction:grey_tap()];

  // Tap "Cancel"
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(
                                       kEditProfileBottomSheetCancelButton),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];

  // Open modal by selecting the badge that shouldn't be accepted.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBadgeButtonSaveAddressProfileAccessibilityIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];
}

// Tests the sticky address prompt journey where the prompt remains there when
// navigating without an explicit user gesture, and then the prompt is dismissed
// when navigating with a user gesture. Test with the address save prompt but
// the type of address prompt doesn't matter in this test case.
- (void)testStickySavePromptJourney {
  [self fillPresidentProfileAndShowSaveInfobar];

  {
    // Reloading page from script shouldn't dismiss the infobar.
    NSString* script = @"location.reload();";
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  }
  {
    // Assigning url from script to the page aka open an url shouldn't dismiss
    // the infobar.
    NSString* script = @"window.location.assign(window.location.href);";
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  }
  {
    // Pushing new history entry without reloading content shouldn't dismiss the
    // infobar.
    NSString* script = @"history.pushState({}, '', 'destination2.html');";
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  }
  {
    // Replacing history entry without reloading content shouldn't dismiss the
    // infobar.
    NSString* script = @"history.replaceState({}, '', 'destination3.html');";
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  }

  // Wait some time for things to settle.
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(200));

  // Verify that the prompt is still there after the non-user initiated
  // navigations.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Navigate with an emulated user gesture and verify that dismisses the
  // prompt.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kProfileForm)];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];
}

// Tests that there is an alert shown if the user tries to dismiss an alert
// after they edited a field in the edit prompt without saving.
// Note that this test is defined above.
- (void)MAYBE_testEditBottomSheetAlertBySwipingDown {
  // TODO(crbug.com/377270834): Fix implementation on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test fails on iPad currently.");
  }

  // Fill and submit the form.
  [self fillPresidentProfileAndShowSaveModal];

  // Edit the profile.
  [[EarlGrey selectElementWithMatcher:ModalEditButtonMatcher()]
      performAction:grey_tap()];

  // Replace city field value.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(@"City")]
      performAction:grey_replaceText(@"New York")];

  // Swipe down the sheet.
  [[EarlGrey selectElementWithMatcher:EditProfileBottomSheet()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  id<GREYMatcher> keepEditingAlert = grey_text(
      l10n_util::GetNSString(IDS_IOS_VIEW_CONTROLLER_DISMISS_CANCEL_CHANGES));
  // Ensure the error alert is shown.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:keepEditingAlert];

  // Keep editing.
  [[EarlGrey selectElementWithMatcher:keepEditingAlert]
      performAction:grey_tap()];

  // Swipe down the sheet again.
  [[EarlGrey selectElementWithMatcher:EditProfileBottomSheet()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the save changes button exists.
  id<GREYMatcher> saveChangesAlert = grey_text(
      l10n_util::GetNSString(IDS_IOS_VIEW_CONTROLLER_DISMISS_SAVE_CHANGES));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:saveChangesAlert];

  [[EarlGrey selectElementWithMatcher:saveChangesAlert]
      performAction:grey_tap()];
}

// Tests that the 'Save' button is only enabled when all the required fields are
// filled.
// TODO(crbug.com/407573862): Re-enable after the test is fixed for
// ios-fieldtrial-rel.
- (void)DISABLED_testSaveButtonEnabledStateDependingOnRequiredFields {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey waitForSyncTransportStateActiveWithTimeout:base::Seconds(10)];

  // Fill and submit the form.
  [self fillPresidentProfileAndShowSaveModal];

  // Edit the profile.
  [[EarlGrey selectElementWithMatcher:ModalEditButtonMatcher()]
      performAction:grey_tap()];

  // Ensure the 'Save' button is initially enabled.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      assertWithMatcher:grey_enabled()];

  NSString* streetAddressLabel = base::SysUTF8ToNSString(
      autofill::FieldTypeToDeveloperRepresentationString(
          autofill::ADDRESS_HOME_STREET_ADDRESS));

  // Empty the street address field, which is required.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(streetAddressLabel)]
      performAction:grey_replaceText(@"")];

  // Scroll down to show the 'Save' button.
  [[EarlGrey selectElementWithMatcher:EditProfileBottomSheet()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Ensure the 'Save' button is disabled.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Re-fill the street address field.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(streetAddressLabel)]
      performAction:grey_replaceText(@"Street")];

  // Ensure the 'Save' button is enabled.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      assertWithMatcher:grey_enabled()];

  // Sign out.
  [SigninEarlGrey signOut];
}

// Tests that submission is detected hence the infobar is displayed when the
// "form" event behind the submission is `defaultPrevented` while the
// corresponding feature allows it.
- (void)testSubmissionDetection_defaultPrevented_whenAllowed {
  // Sign-in so the profile can be saved into the account.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Submit the form with `defaultPrevented` not considered.
  FullAddressFormPageParams params{.default_prevented = true, .redirect = true};
  [self loadAndSubmitFullAddressFormWithParams:params];

  // Wait on the infobar to be displayed after submission.
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];

  // Accept the banner to save the profile.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];

  // Wait for the save profile dialog to appear.
  [ChromeEarlGrey waitForMatcher:ModalButtonMatcher()];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  [SigninEarlGrey signOut];
}

// Tests that submission isn't detected hence the infobar isn't displayed when
// the "form" event behind the submission is `defaultPrevented` while the
// corresponding feature doesn't allows it.
- (void)testSubmissionDetection_defaultPrevented_whenNotAllowed {
  // Sign-in so the profile would be saved into the account.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Submit the form with `defaultPrevented` considered.
  FullAddressFormPageParams params{.default_prevented = true, .redirect = true};
  [self loadAndSubmitFullAddressFormWithParams:params];

  // Make sure the infobar isn't displayed.
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  [SigninEarlGrey signOut];
}

// Tests that multiple submissions on the same form are not deduped when
// deduping is disabled where all submissions are sent over to the browser.
- (void)testSubmissionDetectionWithoutDeduping {
  // Submit the form with `defaultPrevented` not considered and without
  // redirecting so the same form can be submitted multiple time.
  FullAddressFormPageParams params{.default_prevented = true,
                                   .redirect = false};
  [self loadAndSubmitFullAddressFormWithParams:params];

  // Wait on the infobar to be displayed after submission, meaning that
  // submission was detected.
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];

  // Spam submissions.
  for (int i = 0; i < 5; ++i) {
    [ChromeEarlGrey tapWebStateElementWithID:@"submit-button"];
  }

  // Verify that all submissions were sent over to the browser and recorded.
  GREYAssertTrue(
      base::test::ios::WaitUntilConditionOrTimeout(
          base::Milliseconds(200),
          ^{
            NSError* error = [MetricsAppInterface
                expectTotalCount:6
                    forHistogram:@"Autofill.iOS.FormSubmission.OutcomeV2"];
            return error == nil;
          }),
      @"Timed out waiting for all form submission events.");
}

// Tests that multiple submissions on the same form are deduped when deduping is
// enabled where only one submission per form element is allowed when.
- (void)testSubmissionDetectionWithDeduping {
  // Submit the form with `defaultPrevented` not considered and without
  // redirecting so the same form can be submitted multiple time.
  FullAddressFormPageParams params{.default_prevented = true,
                                   .redirect = false};
  [self loadAndSubmitFullAddressFormWithParams:params];

  // Wait on the infobar to be displayed after submission, meaning that
  // submission was detected.
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];

  // Spam submissions.
  for (int i = 0; i < 5; ++i) {
    [ChromeEarlGrey tapWebStateElementWithID:@"submit-button"];
  }

  // Wait some time so the hypothetical form submission messages would have been
  // sent over to the browser by then.
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(200));

  // Verify that only one submission was actually recorded despite triggering
  // multiple submissions on the same form.
  chrome_test_util::GREYAssertErrorNil([MetricsAppInterface
      expectTotalCount:1
          forHistogram:@"Autofill.iOS.FormSubmission.OutcomeV2"]);
}

// Tests that the submission errors that occur in the renderer are reported to
// the browser.
- (void)testSubmissionErrorReporting_Enabled {
  // Inject a bug that will trigger error when handling the form submission in
  // the renderer.
  constexpr char kInjectedBug[] = R"(
    // Swizzle autofillSubmissionData() with an erroring function.
    gcrweb.gCrWeb.fill.autofillSubmissionData = function() {
      throw new Error("Oh no, something bad happened!");
    };
    // This is to give a return value to make the thing handling the JS
    // execution happy.
    true
  )";

  // Load page without submitting the form.
  [self loadFullAddressFormWithParams:{}];

  // Inject the bug in the submission handler so it triggers an error that will
  // be reported to the browser.
  [ChromeEarlGrey
      evaluateJavaScriptInIsolatedWorldForSideEffect:base::SysUTF8ToNSString(
                                                         kInjectedBug)];

  // Now that the submission handler is buggy, submit the form to trigger the
  // error.
  [ChromeEarlGrey tapWebStateElementWithID:@"submit-button"];

  // Verify that no infobar is displayed when there is a submission error.
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  // Verify that the submission error was reported and recorded.
  GREYAssertTrue(
      base::test::ios::WaitUntilConditionOrTimeout(
          base::Milliseconds(200),
          ^{
            NSError* error = [MetricsAppInterface
                expectUniqueSampleWithCount:1
                                  forBucket:kRendererErrorHistogramBucket
                               forHistogram:
                                   @"Autofill.iOS.FormSubmission.OutcomeV2"];
            return error == nil;
          }),
      @"Timed out waiting for the submission error uma record.");
}

// Tests that the submission errors that occur in the renderer are not reported
// to the browser when the feature is disabled.
- (void)testSubmissionErrorReporting_Disabled {
  // Inject a bug that will trigger error when handling the form submission in
  // the renderer.
  constexpr char kInjectedBug[] = R"(
    // Swizzle autofillSubmissionData() with an erroring function.
    gcrweb.gCrWeb.fill.autofillSubmissionData = function() {
      throw new Error("Oh no, something bad happened!");
    };
    // This is to give a return value to make the thing handling the JS
    // execution happy.
    true
  )";

  // Load page without submitting the form.
  [self loadFullAddressFormWithParams:{}];

  // Inject the bug in the submission handler so it triggers an error that will
  // be reported to the browser.
  [ChromeEarlGrey
      evaluateJavaScriptInIsolatedWorldForSideEffect:base::SysUTF8ToNSString(
                                                         kInjectedBug)];

  // Now that the submission handler is buggy, submit the form to trigger the
  // error.
  [ChromeEarlGrey tapWebStateElementWithID:@"submit-button"];

  // Verify for some time that no infobar is displayed when there is a
  // submission error.
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  // Verify that no submission error was not reported and recorded. At this
  // point there should have been enough time to hypothetically handle the
  // submit event if there was no error.
  chrome_test_util::GREYAssertErrorNil([MetricsAppInterface
      expectTotalCount:0
          forHistogram:@"Autofill.iOS.FormSubmission.OutcomeV2"]);
}

// Tests that submission is detected hence the infobar is displayed when the
// "form" event behind the submission has its propagation entirely stopped via
// stopImmediatePropagation() while the form submit event listener for Autofill
// is set in capture mode.
- (void)testSubmissionDetection_inCaptureMode {
  // Sign-in so the profile can be saved into the account.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Submit the form with `defaultPrevented` not considered.
  FullAddressFormPageParams params{.redirect = true,
                                   .stop_immediate_propagation = true};
  [self loadAndSubmitFullAddressFormWithParams:params];

  // Wait on the infobar to be displayed after submission.
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];

  // Accept the banner to save the profile.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];

  // Wait for the save profile dialog to appear.
  [ChromeEarlGrey waitForMatcher:ModalButtonMatcher()];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  [SigninEarlGrey signOut];
}

// Tests that submission isn't detected hence the infobar not displayed when the
// "form" event behind the submission has its propagation entirely stopped via
// stopImmediatePropagation() while the form submit event listener for Autofill
// isn't set in capture mode.
- (void)testSubmissionDetection_notInCaptureMode {
  // Sign-in so the profile can be saved into the account.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Submit the form with the submit event propagation stopped via
  // stopImmediatePropagation().
  FullAddressFormPageParams params{.redirect = true,
                                   .stop_immediate_propagation = true};
  [self loadAndSubmitFullAddressFormWithParams:params];

  // Make sure the infobar isn't displayed.
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  [SigninEarlGrey signOut];
}

@end
