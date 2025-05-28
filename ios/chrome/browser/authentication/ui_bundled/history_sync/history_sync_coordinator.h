// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_HISTORY_SYNC_HISTORY_SYNC_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_HISTORY_SYNC_HISTORY_SYNC_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/interruptible_chrome_coordinator.h"

@class HistorySyncCoordinator;
namespace history_sync {
enum class HistorySyncSkipReason;
}
namespace signin_metrics {
enum class AccessPoint : int;
}  // namespace signin_metrics

// Delegate for the history sync coordinator.
@protocol HistorySyncCoordinatorDelegate <NSObject>

// Called once the dialog can be closed.
- (void)closeHistorySyncCoordinator:
            (HistorySyncCoordinator*)historySyncCoordinator
                     declinedByUser:(BOOL)declined;

@end

// Coordinator for history sync view. The current implementation supports only
// showing the view in a navigation controller.
@interface HistorySyncCoordinator : InterruptibleChromeCoordinator

// Records metric if the History Sync Opt-In screen is skipped for the given
// reason, for the given access point.
+ (void)recordHistorySyncSkipMetric:(history_sync::HistorySyncSkipReason)reason
                        accessPoint:(signin_metrics::AccessPoint)accessPoint;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Initiates a HistorySyncCoordinator with `navigationController`,
// `browser` and `delegate`.
- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                            delegate:
                                (id<HistorySyncCoordinatorDelegate>)delegate
                            firstRun:(BOOL)firstRun
                       showUserEmail:(BOOL)showUserEmail
                          isOptional:(BOOL)isOptional
                         accessPoint:(signin_metrics::AccessPoint)accessPoint
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_HISTORY_SYNC_HISTORY_SYNC_COORDINATOR_H_
