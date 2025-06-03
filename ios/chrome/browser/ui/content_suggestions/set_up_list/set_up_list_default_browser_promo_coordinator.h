// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_DEFAULT_BROWSER_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_DEFAULT_BROWSER_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@protocol SetUpListDefaultBrowserPromoCoordinatorDelegate;

// A coordinator that handles the display of the Default Browser Promo for the
// Set Up List.
@interface SetUpListDefaultBrowserPromoCoordinator
    : ChromeCoordinator <PromoStyleViewControllerDelegate,
                         UIAdaptivePresentationControllerDelegate>

// Creates a coordinator that uses `viewController` and `browser`. Uses
// `application` to open the app's settings.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               application:(UIApplication*)application
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The delegate that receives events from this coordinator.
@property(nonatomic, weak) id<SetUpListDefaultBrowserPromoCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_DEFAULT_BROWSER_PROMO_COORDINATOR_H_
