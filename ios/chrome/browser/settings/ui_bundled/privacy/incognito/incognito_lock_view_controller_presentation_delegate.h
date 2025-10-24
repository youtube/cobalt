// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_INCOGNITO_INCOGNITO_LOCK_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_INCOGNITO_INCOGNITO_LOCK_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

@class IncognitoLockViewController;

// Delegate for presentation events related to Incognito Lock View Controller.
@protocol IncognitoLockViewControllerPresentationDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)incognitoLockViewControllerDidRemove:
    (IncognitoLockViewController*)controller;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_INCOGNITO_INCOGNITO_LOCK_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
