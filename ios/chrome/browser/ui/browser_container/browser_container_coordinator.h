// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import <UIKit/UIKit.h>

@class BrowserContainerViewController;

// A coordinator that creates a container UIViewController that displays the
// web contents of the browser view.
@interface BrowserContainerCoordinator : ChromeCoordinator

// The view controller managing the container view.
@property(nonatomic, strong, readonly)
    BrowserContainerViewController* viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_COORDINATOR_H_
