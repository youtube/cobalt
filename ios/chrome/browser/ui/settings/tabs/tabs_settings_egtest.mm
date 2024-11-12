// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

// Test tabs settings which handle all tabs settings.
@interface TabsSettingsTestCase : ChromeTestCase
@end

@implementation TabsSettingsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kInactiveTabsIPadFeature);
  return config;
}

- (void)setUp {
  [super setUp];
  // Ensures that inactive tabs preference settings is set to its default state.
  [ChromeEarlGrey setIntegerValue:0
                forLocalStatePref:prefs::kInactiveTabsTimeThreshold];
}

- (void)tearDownHelper {
  // Resets preferences back to default values.
  [ChromeEarlGrey setIntegerValue:0
                forLocalStatePref:prefs::kInactiveTabsTimeThreshold];
  [super tearDownHelper];
}

// Ensures that the tabs settings open.
- (void)testOpenTabsSettings {
  [self openTabsSettings];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsTabsTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Ensures that the user still have access to tabs settings even if the inactive
// tabs feature has been manually disabled.
- (void)testOpenTabsSettingsWhenInactiveTabsDisabledByUser {
  GREYAssertEqual(
      0,
      [ChromeEarlGrey localStateIntegerPref:prefs::kInactiveTabsTimeThreshold],
      @"Inactive tabs preference is not set to default value.");
  [ChromeEarlGrey setIntegerValue:kInactiveTabsDisabledByUser
                forLocalStatePref:prefs::kInactiveTabsTimeThreshold];
  GREYAssertEqual(
      kInactiveTabsDisabledByUser,
      [ChromeEarlGrey localStateIntegerPref:prefs::kInactiveTabsTimeThreshold],
      @"Inactive tabs preference is not set to disable the feature.");

  [self openTabsSettings];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsTabsTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - Helpers

// Opens tabs settings.
- (void)openTabsSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::TabsSettingsButton()];
}

@end
