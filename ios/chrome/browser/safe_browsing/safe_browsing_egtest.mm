// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::BackButton;
using chrome_test_util::ForwardButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::TappableBookmarkNodeWithLabel;

namespace {

// Text that is found when expanding details on the phishing warning page.
const char kPhishingWarningDetails[] =
    "Google Safe Browsing recently detected phishing";

// Text that is found when expanding details on the malware warning page.
const char kMalwareWarningDetails[] =
    "Google Safe Browsing recently detected malware";

// Request handler for net::EmbeddedTestServer that returns the request URL's
// path as the body of the response if the request URL's path starts with
// "/echo". Otherwise, returns nulltpr to allow other handlers to handle the
// request.
std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url, "/echo",
                        base::CompareCase::SENSITIVE)) {
    return nullptr;
  }
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_content(request.relative_url);
  http_response->set_content_type("text/html");
  return http_response;
}
}

// Tests Safe Browsing URL blocking.
@interface SafeBrowsingTestCase : ChromeTestCase {
  // A URL that is treated as an unsafe phishing page.
  GURL _phishingURL;
  // Text that is found on the phishing page.
  std::string _phishingContent;
  // A URL that is treated as an unsafe phishing page by real-time lookups.
  GURL _realTimePhishingURL;
  // Text that is found on the real-time phishing page.
  std::string _realTimePhishingContent;
  // A URL that is treated as an unsafe malware page.
  GURL _malwareURL;
  // Text that is found on the malware page.
  std::string _malwareContent;
  // A URL of a page with an iframe that is treated as having malware.
  GURL _iframeWithMalwareURL;
  // Text that is found on the iframe that is treated as having malware.
  std::string _iframeWithMalwareContent;
  // A URL that is treated as a safe page.
  GURL _safeURL1;
  // Text that is found on the safe page.
  std::string _safeContent1;
  // Another URL that is treated as a safe page.
  GURL _safeURL2;
  // Text that is found on the safe page.
  std::string _safeContent2;
  // The default value for SafeBrowsingEnabled pref.
  BOOL _safeBrowsingEnabledPrefDefault;
  // The default value for SafeBrowsingEnhanced pref.
  BOOL _safeBrowsingEnhancedPrefDefault;
  // The default value for SafeBrowsingProceedAnywayDisabled pref.
  BOOL _proceedAnywayDisabledPrefDefault;
}
@end

@implementation SafeBrowsingTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Use commandline args to insert fake unsafe URLs into the Safe Browsing
  // database.
  config.additional_args.push_back(std::string("--mark_as_phishing=") +
                                   _phishingURL.spec());
  config.additional_args.push_back(std::string("--mark_as_malware=") +
                                   _malwareURL.spec());
  config.additional_args.push_back(
      std::string("--mark_as_real_time_phishing=") +
      _realTimePhishingURL.spec());
  config.additional_args.push_back(
      std::string("--mark_as_allowlisted_for_real_time=") + _safeURL1.spec());
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

- (void)setUp {
  self.testServer->RegisterRequestHandler(base::BindRepeating(&HandleRequest));
  bool started = self.testServer->Start();
  _phishingURL = self.testServer->GetURL("/echo_phishing_page");
  _phishingContent = "phishing_page";

  _realTimePhishingURL = self.testServer->GetURL("/echo_realtime_page");
  _realTimePhishingContent = "realtime_page";

  _malwareURL = self.testServer->GetURL("/echo_malware_page");
  _malwareContent = "malware_page";

  _iframeWithMalwareURL =
      self.testServer->GetURL("/iframe?" + _malwareURL.spec());
  _iframeWithMalwareContent = _malwareContent;

  _safeURL1 = self.testServer->GetURL("/echo_safe_page");
  _safeContent1 = "safe_page";

  _safeURL2 = self.testServer->GetURL("/echo_also_safe");
  _safeContent2 = "also_safe";

  if (@available(iOS 15.1, *)) {
  } else {
    if (@available(iOS 14.5, *)) {
      // Workaround https://bugs.webkit.org/show_bug.cgi?id=226323, which breaks
      // some back/forward navigations between pages that share a renderer
      // process. Use 'localhost' instead of '127.0.0.1' for safe URLs to
      // prevent sharing renderer processes with unsafe URLs.
      GURL::Replacements replacements;
      replacements.SetHostStr("localhost");
      _safeURL1 = _safeURL1.ReplaceComponents(replacements);
      _safeURL2 = _safeURL2.ReplaceComponents(replacements);
    }
  }

  // `appConfigurationForTestCase` is called during [super setUp], and
  // depends on the URLs initialized above.
  [super setUp];

  // GREYAssertTrue cannot be called before [super setUp].
  GREYAssertTrue(started, @"Test server failed to start.");

  // Save the existing value of the pref to set it back in tearDown.
  _safeBrowsingEnabledPrefDefault =
      [ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnabled];
  // Ensure that Safe Browsing opt-out starts in its default (opted-out) state.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnabled];

  // Save the existing value of the pref to set it back in tearDown.
  _safeBrowsingEnhancedPrefDefault =
      [ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnhanced];
  // Ensure that Enhanced Safe Browsing opt-out starts in its default (opted-in)
  // state.
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kSafeBrowsingEnhanced];

  // Save the existing value of the pref to set it back in tearDown.
  _proceedAnywayDisabledPrefDefault = [ChromeEarlGrey
      userBooleanPref:prefs::kSafeBrowsingProceedAnywayDisabled];
  // Ensure that Proceed link is shown by default in the safe browsing warning.
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:prefs::kSafeBrowsingProceedAnywayDisabled];

  // Ensure that the real-time Safe Browsing opt-in starts in the default
  // (opted-out) state.
  [ChromeEarlGrey setURLKeyedAnonymizedDataCollectionEnabled:NO];
}

- (void)tearDown {
  // Ensure that Safe Browsing is reset to its original value.
  [ChromeEarlGrey setBoolValue:_safeBrowsingEnabledPrefDefault
                   forUserPref:prefs::kSafeBrowsingEnabled];

  // Ensure that Enhanced Safe Browsing is reset to its original value.
  [ChromeEarlGrey setBoolValue:_safeBrowsingEnhancedPrefDefault
                   forUserPref:prefs::kSafeBrowsingEnhanced];

  // Ensure that Proceed link is reset to its original value.
  [ChromeEarlGrey setBoolValue:_proceedAnywayDisabledPrefDefault
                   forUserPref:prefs::kSafeBrowsingProceedAnywayDisabled];

  // Ensure that the real-time Safe Browsing opt-in is reset to its original
  // value.
  [ChromeEarlGrey setURLKeyedAnonymizedDataCollectionEnabled:NO];

  [super tearDown];
}

#pragma mark - Helper methods

// Instantiates an ElementSelector to detect the enhanced protection message on
// interstitial page.
- (ElementSelector*)enhancedProtectionMessage {
  NSString* selector =
      @"(function() {"
       "  var element = document.getElementById('enhanced-protection-message');"
       "  if (element == null) return false;"
       "  if (element.classList.contains('hidden')) return false;"
       "  return true;"
       "})()";
  NSString* description = @"Enhanced Safe Browsing message.";
  return [ElementSelector selectorWithScript:selector
                         selectorDescription:description];
}

#pragma mark - Tests
// Tests that safe pages are not blocked.
- (void)testSafePage {
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
}

// Tests that a phishing page is blocked, and the "Back to safety" button on
// the warning page works as expected.
- (void)testPhishingPage {
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the phishing page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];

  // Tap on the "Back to safety" button and verify that the previous page's
  // contents are loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
}

// Tests expanding the details on a phishing warning, and proceeding past the
// warning. Also verifies that a warning is still shown when visiting the unsafe
// URL in a new tab.
- (void)testProceedingPastPhishingWarning {
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the phishing page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];

  // Tap on the Details button and verify that warning details are shown.
  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:kPhishingWarningDetails];

  // Tap on the link to proceed to the unsafe page, and verify that this page is
  // loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateContainingText:_phishingContent];

  // In a new tab, a warning should still be shown, even though the user
  // proceeded to the unsafe content in the other tab.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];
}

// Tests expanding the details on a phishing warning, and proceeding past the
// warning in incognito mode. Also verifies that a warning is still shown when
// visiting the unsafe URL in a new incognito tab.
- (void)testProceedingPastPhishingWarningInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the phishing page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];

  // Tap on the Details button and verify that warning details are shown.
  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:kPhishingWarningDetails];

  // Tap on the link to proceed to the unsafe page, and verify that this page is
  // loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateContainingText:_phishingContent];

  // In a new tab, a warning should still be shown, even though the user
  // proceeded to the unsafe content in the other tab.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];
}

// Tests that a malware page is blocked, and the "Back to safety" button on the
// warning page works as expected.
- (void)testMalwarePage {
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the malware page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  // Tap on the "Back to safety" button and verify that the previous page's
  // contents are loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
}

// Tests expanding the details on a malware warning, proceeding past the
// warning, and navigating back/forward to the unsafe page.
- (void)testProceedingPastMalwareWarning {
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the malware page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  // Tap on the Details button and verify that warning details are shown.
  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:kMalwareWarningDetails];

  if (@available(iOS 15.1, *)) {
  } else {
    if (@available(iOS 14.5, *)) {
      // Workaround https://bugs.webkit.org/show_bug.cgi?id=226323, which can
      // break loading the unsafe page below.
      return;
    }
  }

  // Tap on the link to proceed to the unsafe page, and verify that this page is
  // loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateFrameContainingText:_malwareContent];

  // Verify that no warning is shown when navigating back and then forward to
  // the unsafe page.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:_malwareContent];

  // Visit another safe page, and then navigate back to the unsafe page and
  // verify that no warning is shown.
  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:_malwareContent];
}

// Tests expanding the details on a malware warning, proceeding past the
// warning, and navigating back/forward to the unsafe page, in incognito mode.
- (void)testProceedingPastMalwareWarningInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the malware page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  // Tap on the Details button and verify that warning details are shown.
  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:kMalwareWarningDetails];

  if (@available(iOS 15.1, *)) {
  } else {
    if (@available(iOS 14.5, *)) {
      // Workaround https://bugs.webkit.org/show_bug.cgi?id=226323, which can
      // break loading the unsafe page below.
      return;
    }
  }

  // Tap on the link to proceed to the unsafe page, and verify that this page is
  // loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateFrameContainingText:_malwareContent];

  // Verify that no warning is shown when navigating back and then forward to
  // the unsafe page.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_malwareContent];

  // Visit another safe page, and then navigate back to the unsafe page and
  // verify that no warning is shown.
  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_malwareContent];
}

// Tests that disabling and re-enabling Safe Browsing works as expected.
- (void)testDisableAndEnableSafeBrowsing {
  // Disable Safe Browsing and verify that unsafe content is shown.
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kSafeBrowsingEnabled];
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:_phishingContent];

  // Re-enable Safe Browsing and verify that a warning is shown for unsafe
  // content.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnabled];
  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];
}

// Tests enabling Enhanced Protection from a Standard Protection state (Default
// state) from the interstitial blocking page.
- (void)testDisableAndEnableEnhancedSafeBrowsing {
  // Disable Enhanced Safe Browsing and verify that a dark red box prompting to
  // turn on Enhanced Protection is visible.
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kSafeBrowsingEnhanced];
  ElementSelector* enhancedSafeBrowsingMessage =
      [self enhancedProtectionMessage];

  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingElement:enhancedSafeBrowsingMessage];

  // Re-enable Enhanced Safe Browsing and verify that a dark red box prompting
  // to turn on Enhanced Protection is not visible.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnhanced];
  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
  [ChromeEarlGrey loadURL:_realTimePhishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];
  [ChromeEarlGrey
      waitForWebStateNotContainingElement:enhancedSafeBrowsingMessage];
}

- (void)testEnhancedSafeBrowsingLink {
  // Disable Enhanced Safe Browsing and verify that a dark red box prompting to
  // turn on Enhanced Protection is visible.
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kSafeBrowsingEnhanced];
  ElementSelector* enhancedSafeBrowsingMessage =
      [self enhancedProtectionMessage];

  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingElement:enhancedSafeBrowsingMessage];
  [ChromeEarlGrey tapWebStateElementWithID:@"enhanced-protection-link"];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kSettingsSafeBrowsingEnhancedProtectionCellId)]
      performAction:grey_tap()];
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnhanced],
                 @"Failed to toggle-on Enhanced Safe Browsing");
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Verify that a dark red box prompting to turn on Enhanced Protection is not
  // visible.
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];
  [ChromeEarlGrey
      waitForWebStateNotContainingElement:enhancedSafeBrowsingMessage];
}

// Tests displaying a warning for an unsafe page in incognito mode, and
// proceeding past the warning.
- (void)testWarningInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the phishing page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];

  // Tap on the Details button and verify that warning details are shown.
  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:kPhishingWarningDetails];

  // Tap on the link to proceed to the unsafe page, and verify that this page is
  // loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateContainingText:_phishingContent];
}

// Tests that the proceed option is not shown when
// kSafeBrowsingProceedAnywayDisabled is enabled.
- (void)testProceedAlwaysDisabled {
  // Enable the pref.
  NSString* prefName =
      base::SysUTF8ToNSString(prefs::kSafeBrowsingProceedAnywayDisabled);
  [ChromeEarlGreyAppInterface setBoolValue:YES forUserPref:prefName];

  // Load the a malware safe browsing error page.
  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"The site ahead contains malware"];

  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:
                      "Google Safe Browsing recently detected malware"];

  // Verify that the proceed-link element is not found.  When the proceed link
  // is disabled, the entire second paragraph hidden.
  NSString* selector =
      @"(function() {"
       "  var element = document.getElementById('final-paragraph');"
       "  if (element.classList.contains('hidden')) return true;"
       "  return false;"
       "})()";
  NSString* description = @"Hidden proceed-anyway link.";
  ElementSelector* proceedLink =
      [ElementSelector selectorWithScript:selector
                      selectorDescription:description];
  GREYAssert(
      [ChromeEarlGreyAppInterface webStateContainsElement:proceedLink],
      @"Proceed anyway link shown despite kSafeBrowsingProceedAnywayDisabled");
}

// Tests performing a back navigation to a warning page and a forward navigation
// from a warning page.
- (void)testBackForwardNavigationWithWarning {
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the malware page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
  // TODO(crbug.com/1153261): Adding a delay to avoid never-ending load on the
  // last navigation forward. Should be fixed in newer iOS version.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];
  // TODO(crbug.com/1153261): Adding a delay to avoid never-ending load on the
  // last navigation forward. Should be fixed in newer iOS version.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));

  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
}

// Tests performing a back navigation to a warning page and a forward navigation
// from a warning page, in incognito mode.
- (void)testBackForwardNavigationWithWarningInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the malware page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];

  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
}

// Tests that performing session restoration to a Safe Browsing warning page
// preserves navigation history.
- (void)testRestoreToWarningPagePreservesHistory {
  // Build up navigation history that consists of a safe URL, a warning page,
  // and another safe URL.
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the malware page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  // Tap on the "Back to safety" button and verify that the previous page's
  // contents are loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];
  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];

  // Navigate back so that both the back list and the forward list are
  // non-empty.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  // Do a session restoration and verify that all navigation history is
  // preserved.
  [ChromeEarlGrey triggerRestoreViaTabGridRemoveAllUndo];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
}

// Tests that a page with an unsafe ifame is blocked, back history is preserved,
// and forward navigation to the warning works as expected.
- (void)testPageWithUnsafeIframe {
  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load a page that has an iframe with malware, and verify that a warning is
  // shown.
  [ChromeEarlGrey loadURL:_iframeWithMalwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  // Ensure back history is preserved. Tap on the "Back to safety" button and
  // verify that the previous page's contents are loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];

  // Verify that going forward results in the warning being displayed.
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];
}

// Tests that a page with an unsafe ifame is blocked, back history is preserved,
// and forward navigation to the warning works as expected, in incognito mode.
- (void)testPageWithUnsafeIframeInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load a page that has an iframe with malware, and verify that a warning is
  // shown.
  [ChromeEarlGrey loadURL:_iframeWithMalwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  // Ensure back history is preserved. Tap on the "Back to safety" button and
  // verify that the previous page's contents are loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];

  // Verify that going forward results in the warning being displayed.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];
}

// Tests performing a back navigation to a warning page for an unsafe iframe,
// and then performing a forward navigation from the warning.
- (void)testBackForwardNavigationWithIframeWarning {
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load a page that has an iframe with malware, and verify that a warning is
  // shown.
  [ChromeEarlGrey loadURL:_iframeWithMalwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];

  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
}

// Tests expanding the details on a warning for an unsafe iframe, proceeding
// past the warning, and navigating away from and back to the unsafe page. Also
// verifies that a warning is still shown when visiting the unsafe URL in a new
// tab.
- (void)testProceedingPastIframeWarning {
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load a page that has an iframe with malware, and verify that a warning is
  // shown.
  [ChromeEarlGrey loadURL:_iframeWithMalwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  // Tap on the Details button and verify that warning details are shown.
  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:kMalwareWarningDetails];

  // Tap on the link to proceed to the unsafe page, and verify that this page is
  // loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateFrameContainingText:_malwareContent];

  // Verify that no warning is shown when navigating back and then forward to
  // the unsafe page.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateFrameContainingText:_malwareContent];

  // Visit another safe page, and then navigate back to the unsafe page and
  // verify that no warning is shown.
  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateFrameContainingText:_malwareContent];

  // Verify that a warning is still shown when loading the page in a new tab.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_iframeWithMalwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];
}

// Tests that real-time lookups are not performed when opted-out of real-time
// lookups.
- (void)testRealTimeLookupsWhileOptedOut {
  // Load the real-time phishing page and verify that no warning is shown.
  [ChromeEarlGrey loadURL:_realTimePhishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:_realTimePhishingContent];
}

// Tests that real-time lookups are not performed when opted-out of Safe
// Browsing, regardless of the state of the real-time opt-in.
- (void)testRealTimeLookupsWhileOptedOutOfSafeBrowsing {
  // Opt out of Safe Browsing.
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kSafeBrowsingEnabled];

  // Load the real-time phishing page and verify that no warning is shown.
  [ChromeEarlGrey loadURL:_realTimePhishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:_realTimePhishingContent];

  // Opt-in to real-time checks and verify that it's still the case that no
  // warning is shown.
  [ChromeEarlGrey setURLKeyedAnonymizedDataCollectionEnabled:YES];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_realTimePhishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:_realTimePhishingContent];
}

// Tests that a page identified as unsafe by real-time Safe Browsing is blocked
// when opted-in to real-time lookups.
- (void)testRealTimeLookupsWhileOptedIn {
  // Opt-in to real-time checks.
  [ChromeEarlGrey setURLKeyedAnonymizedDataCollectionEnabled:YES];

  // Load the real-time phishing page and verify that a warning page is shown.
  [ChromeEarlGrey loadURL:_realTimePhishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];
}

// Tests that real-time lookups are not performed in incognito mode.
- (void)testRealTimeLookupsInIncognito {
  // Opt-in to real-time checks.
  [ChromeEarlGrey setURLKeyedAnonymizedDataCollectionEnabled:YES];

  // Load the real-time phishing page and verify that no warning is shown.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_realTimePhishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:_realTimePhishingContent];
}

// Tests that a page identified as unsafe by real-time Safe Browsing is blocked
// when loaded as part of session restoration.
- (void)testRestoreRealTimeWarning {
  // Opt-in to real-time checks.
  [ChromeEarlGrey setURLKeyedAnonymizedDataCollectionEnabled:YES];

  // Visit two safe pages, followed by an unsafe page.
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
  [ChromeEarlGrey loadURL:_realTimePhishingURL];

  // Verify that a warning is shown for the unsafe page.
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];

  // Perform session restoration, and verify that a warning is still shown.
  [ChromeEarlGrey triggerRestoreViaTabGridRemoveAllUndo];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];
}

// Tests that when a page identified as unsafe by real-time Safe Browsing is
// loaded using a bookmark, a warning is shown.
- (void)testRealTimeWarningForBookmark {
  NSString* phishingTitle = @"Real-time phishing";
  [BookmarkEarlGrey addBookmarkWithTitle:phishingTitle
                                     URL:base::SysUTF8ToNSString(
                                             _realTimePhishingURL.spec())];
  // Opt-in to real-time checks.
  [ChromeEarlGrey setURLKeyedAnonymizedDataCollectionEnabled:YES];

  // Load the real-time phishing page using its bookmark, and verify that a
  // warning is shown.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(phishingTitle)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];
}

@end
