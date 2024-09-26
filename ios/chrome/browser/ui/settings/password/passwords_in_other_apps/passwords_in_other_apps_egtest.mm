// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_constants.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/constants.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_constants.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/earl_grey_scoped_block_swizzler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;

namespace {

// Checks if the current device is running iOS 16 and above. This may seem
// overly verbose, but the @available guard needs to be wrapped in an if() or
// else the compiler complains.
bool isIOS16AndAbove() {
  if (@available(iOS 16, *)) {
    return true;
  }
  return false;
}

// Matcher for view
id<GREYMatcher> PasswordsInOtherAppsViewMatcher() {
  return grey_accessibilityID(kPasswordsInOtherAppsViewAccessibilityIdentifier);
}

// Matcher for title.
id<GREYMatcher> PasswordsInOtherAppsTitleMatcher() {
  return grey_accessibilityID(
      kPasswordsInOtherAppsTitleAccessibilityIdentifier);
}

// Matcher for subtitle.
id<GREYMatcher> PasswordsInOtherAppsSubtitleMatcher() {
  return grey_accessibilityID(
      kPasswordsInOtherAppsSubtitleAccessibilityIdentifier);
}

// Matcher for banner image.
id<GREYMatcher> PasswordsInOtherAppsImageMatcher() {
  return grey_accessibilityID(
      kPasswordsInOtherAppsImageAccessibilityIdentifier);
}

// Matcher for the cell item in Password Settings page.
id<GREYMatcher> PasswordsInOtherAppsListItemMatcher() {
  return grey_accessibilityID(kPasswordSettingsPasswordsInOtherAppsRowId);
}

// Matcher for turn off instructions.
id<GREYMatcher> PasswordsInOtherAppsTurnOffInstruction() {
  return grey_text(
      isIOS16AndAbove()
          ? @"To turn off, open Settings and go to Password Options."
          : @"To turn off, open Settings and go to AutoFill Passwords.");
}

// Matcher for the Show password button in Password Details view.
id<GREYMatcher> OpenSettingsButton() {
  return grey_accessibilityID(
      kPasswordsInOtherAppsActionAccessibilityIdentifier);
}

// Action to open the Passwords in Other Apps modal from Chrome root view.
void OpensPasswordsInOtherApps() {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsMenuPasswordsButton()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsToolbarSettingsButtonId)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordsInOtherAppsListItemMatcher()]
      performAction:grey_tap()];
}
}  // namespace

// This test tests overall behaviors and interactions of Passwords In Other Apps
// view controller.
@interface PasswordsInOtherAppsTestCase : ChromeTestCase
@end

@implementation PasswordsInOtherAppsTestCase {
  // A swizzler to observe fake auto-fill status instead of real one.
  std::unique_ptr<EarlGreyScopedBlockSwizzler> _passwordAutoFillStatusSwizzler;
}

- (void)setUp {
  [super setUp];
  _passwordAutoFillStatusSwizzler =
      std::make_unique<EarlGreyScopedBlockSwizzler>(
          @"PasswordAutoFillStatusManager", @"sharedManager",
          [PasswordsInOtherAppsAppInterface
              swizzlePasswordAutoFillStatusManagerWithFake]);
}

- (void)tearDown {
  [super tearDown];
  [PasswordsInOtherAppsAppInterface resetManager];
  _passwordAutoFillStatusSwizzler.reset();
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  config.features_enabled.push_back(
      password_manager::features::kIOSPasswordUISplit);

  return config;
}

#pragma mark - helper functions

// Tests that the banner image, title and subtitle are visible.
- (void)checkThatCommonElementsAreVisible {
  [[EarlGrey selectElementWithMatcher:PasswordsInOtherAppsTitleMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PasswordsInOtherAppsSubtitleMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PasswordsInOtherAppsImageMatcher()]
      assertWithMatcher:grey_minimumVisiblePercent(0.2)];
}

// Tests that instructions to turn on Chrome auto-fill is visible.
- (void)checkThatTurnOnInstructionsAreVisible {
  NSArray<NSString*>* steps = @[
    ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
        ? l10n_util::GetNSString(
              IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_1_IPAD)
        : l10n_util::GetNSString(
              IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_1_IPHONE),
    l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_2),
    l10n_util::GetNSString(
        isIOS16AndAbove()
            ? IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_3_IOS16
            : IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_3),
    l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_4)
  ];
  for (NSString* step in steps) {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(step)]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
  [[EarlGrey selectElementWithMatcher:OpenSettingsButton()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that instructions to turn on Chrome auto-fill is invisible.
- (void)checkThatTurnOnInstructionsAreNotVisible {
  NSArray<NSString*>* steps = @[
    ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
        ? l10n_util::GetNSString(
              IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_1_IPAD)
        : l10n_util::GetNSString(
              IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_1_IPHONE),
    l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_2),
    l10n_util::GetNSString(
        isIOS16AndAbove()
            ? IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_3_IOS16
            : IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_3),
    l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_STEP_4)
  ];
  for (NSString* step in steps) {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(step)]
        assertWithMatcher:grey_notVisible()];
  }
}

// Tests that instructions to turn off Chrome auto-fill is visible.
- (void)checkThatTurnOffInstructionsAreVisible {
  [[EarlGrey selectElementWithMatcher:PasswordsInOtherAppsTurnOffInstruction()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that instructions to turn off Chrome auto-fill is invisible.
- (void)checkThatTurnOffInstructionsAreNotVisible {
  [[EarlGrey selectElementWithMatcher:PasswordsInOtherAppsTurnOffInstruction()]
      assertWithMatcher:grey_notVisible()];
}

#pragma mark - Test cases

// Tests Passwords In Other Apps first shows instructions when auto-fill is off,
// then shows the caption label after auto-fill is turned on.
- (void)testTurnOnPasswordsInOtherApps {
  // Rewrites passwordInAppsViewController.useShortInstruction property.
  EarlGreyScopedBlockSwizzler longInstruction(
      @"PasswordsInOtherAppsViewController", @"useShortInstruction", ^{
        return NO;
      });

  [PasswordsInOtherAppsAppInterface startFakeManagerWithAutoFillStatus:NO];
  OpensPasswordsInOtherApps();

  [self checkThatCommonElementsAreVisible];
  [self checkThatTurnOnInstructionsAreVisible];
  [self checkThatTurnOffInstructionsAreNotVisible];

  [PasswordsInOtherAppsAppInterface setAutoFillStatus:YES];

  [self checkThatTurnOnInstructionsAreNotVisible];
  [self checkThatTurnOffInstructionsAreVisible];
}

// Tests Passwords In Other Apps first shows instructions when auto-fill is on,
// then shows the caption label after auto-fill is turned off.
- (void)testTurnOffPasswordsInOtherApps {
  // Rewrites passwordInAppsViewController.useShortInstruction property.
  EarlGreyScopedBlockSwizzler longInstruction(
      @"PasswordsInOtherAppsViewController", @"useShortInstruction", ^{
        return NO;
      });

  [PasswordsInOtherAppsAppInterface startFakeManagerWithAutoFillStatus:YES];
  OpensPasswordsInOtherApps();

  [self checkThatCommonElementsAreVisible];
  [self checkThatTurnOffInstructionsAreVisible];
  [self checkThatTurnOnInstructionsAreNotVisible];

  [PasswordsInOtherAppsAppInterface setAutoFillStatus:NO];

  [self checkThatTurnOffInstructionsAreNotVisible];
  [self checkThatTurnOnInstructionsAreVisible];
}

// Tests Passwords In Other Apps shows instructions when auto-fill is off with
// short instruction.
- (void)testShowPasswordsInOtherAppsWithShortInstruction {
  // Rewrites passwordInAppsViewController.useShortInstruction property.
  EarlGreyScopedBlockSwizzler shortInstruction(
      @"PasswordsInOtherAppsViewController", @"useShortInstruction", ^{
        return YES;
      });

  [PasswordsInOtherAppsAppInterface startFakeManagerWithAutoFillStatus:NO];
  OpensPasswordsInOtherApps();
  // Check both turn off instructions and default turn on instructions aren't
  // visible.
  [self checkThatCommonElementsAreVisible];
  [self checkThatTurnOnInstructionsAreNotVisible];
  [self checkThatTurnOffInstructionsAreNotVisible];

  // Check backup instructions are visible.
  NSArray<NSString*>* steps = @[
    l10n_util::GetNSString(
        isIOS16AndAbove()
            ? IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_SHORTENED_STEP_1_IOS16
            : IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_SHORTENED_STEP_1),
    l10n_util::GetNSString(
        IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS_SHORTENED_STEP_2)
  ];
  for (NSString* step in steps) {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(step)]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
  [[EarlGrey selectElementWithMatcher:OpenSettingsButton()]
      assertWithMatcher:grey_interactable()];
}

// Tests Passwords In Other Apps shows instructions when auto-fill state is
// unknown.
- (void)testOpenPasswordsInOtherAppsWithAutoFillUnknown {
  OpensPasswordsInOtherApps();

  [self checkThatCommonElementsAreVisible];
  [self checkThatTurnOffInstructionsAreNotVisible];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_kindOfClassName(
                                              @"UIActivityIndicatorView"),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Simulate status retrieved.
  [PasswordsInOtherAppsAppInterface startFakeManagerWithAutoFillStatus:NO];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_kindOfClassName(
                                              @"UIActivityIndicatorView"),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

// Tests Passwords In Other Apps dismisses itself when top right "done" button
// is tapped.
- (void)testTapPasswordsInOtherAppsDoneButtonToDismiss {
  OpensPasswordsInOtherApps();
  [self checkThatCommonElementsAreVisible];
  // Taps done button and check settings dismissed.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SettingsDoneButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PasswordsInOtherAppsViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests Passwords In Other Apps dismisses itself when the user swipes down.
- (void)testSwipeDownPasswordsInOtherAppsToDismiss {
  OpensPasswordsInOtherApps();
  [self checkThatCommonElementsAreVisible];
  // Swipes down and check settings dismissed.
  [[EarlGrey selectElementWithMatcher:PasswordsInOtherAppsViewMatcher()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [[EarlGrey selectElementWithMatcher:PasswordsInOtherAppsViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests Passwords In Other Apps doesn't show the image on iPhone landscape
// mode, while showing it for iPad.
- (void)testImageVisibilityForLandscapeMode {
  OpensPasswordsInOtherApps();
  [[EarlGrey selectElementWithMatcher:PasswordsInOtherAppsImageMatcher()]
      assertWithMatcher:grey_minimumVisiblePercent(0.2)];
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE) {
    [[EarlGrey selectElementWithMatcher:PasswordsInOtherAppsImageMatcher()]
        assertWithMatcher:grey_notVisible()];
  } else {
    [[EarlGrey selectElementWithMatcher:PasswordsInOtherAppsImageMatcher()]
        assertWithMatcher:grey_minimumVisiblePercent(0.2)];
  }
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  [[EarlGrey selectElementWithMatcher:PasswordsInOtherAppsImageMatcher()]
      assertWithMatcher:grey_minimumVisiblePercent(0.2)];
}

@end
