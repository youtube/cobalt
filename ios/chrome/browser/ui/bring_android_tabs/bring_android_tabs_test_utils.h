// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BRING_ANDROID_TABS_BRING_ANDROID_TABS_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_UI_BRING_ANDROID_TABS_BRING_ANDROID_TABS_TEST_UTILS_H_

#import <Foundation/Foundation.h>

struct AppLaunchConfiguration;

// Returns the app launch configuration for Android switcher, if
// `is_android_switcher` is YES, or non Android switcher if NO. The returned
// config forces first run experience and enables `kBringYourOwnTabsIOS`
// feature.
AppLaunchConfiguration GetConfiguration(BOOL is_android_switcher,
                                        BOOL show_bottom_message);

// On first run experience promos, logs in, enrolls in sync if `sync` is YES,
// and dismisses the default browser promo to show the new tab page.
void CompleteFREWithSyncEnabled(BOOL sync);

// Opens Chrome Settings, sign in and enroll in sync.
void SignInAndSync();

// Verifies the visual state of the prompt. If `visibility` is YES, this
// verification passes that the prompt is visible; otherwise it passes when the
// prompt is invisible.
void VerifyBottomMessagePromptVisibility(BOOL visibility);
void VerifyConfirmationAlertPromptVisibility(BOOL visibility);
void VerifyTabListPromptVisibility(BOOL visibility);

// Restarts the app, goes to the tab grid, and verifies that the prompt is not
// shown.
void VerifyThatPromptDoesNotShowOnRestart(BOOL bottom_message);

// Factory resets the state of the app. Should be called at the end of each
// test.
void CleanUp();

#endif  // IOS_CHROME_BROWSER_UI_BRING_ANDROID_TABS_BRING_ANDROID_TABS_TEST_UTILS_H_
