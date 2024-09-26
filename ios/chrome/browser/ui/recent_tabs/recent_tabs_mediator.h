// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/synced_sessions/synced_sessions_bridge.h"
#import "ios/chrome/browser/ui/recent_tabs/closed_tabs_observer_bridge.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller_delegate.h"

class ChromeBrowserState;
class WebStateList;

@protocol RecentTabsConsumer;

// RecentTabsMediator controls the RecentTabsConsumer,
// based on the user's signed-in and chrome-sync states.
//
// RecentTabsMediator listens for notifications about Chrome Sync
// and ChromeToDevice and changes/updates the RecentTabsConsumer
// accordingly.
@interface RecentTabsMediator : NSObject<ClosedTabsObserving,
                                         RecentTabsTableViewControllerDelegate,
                                         TableViewFaviconDataSource>

// The consumer for this object. This can change during the lifetime of this
// object and may be nil.
@property(nonatomic, strong) id<RecentTabsConsumer> consumer;

// The coordinator's BrowserState.
@property(nonatomic, assign) ChromeBrowserState* browserState;

// The WebStateList that this mediator listens for.
@property(nonatomic, assign) WebStateList* webStateList;

// Starts observing the he user's signed-in and chrome-sync states.
- (void)initObservers;

// Disconnects the mediator from all observers.
- (void)disconnect;

// Configures the consumer with current data. Intended to be called immediately
// after initialization.
- (void)configureConsumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_MEDIATOR_H_
