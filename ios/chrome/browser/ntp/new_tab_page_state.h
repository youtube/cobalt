// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_STATE_H_
#define IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_STATE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/discover_feed/feed_constants.h"
#import "ios/web/public/web_state.h"

// The saved state of a new tab page. Since a single NTP coordinator is shared
// across all web states, each web state is associated with a NewTabPageState to
// recover its properties when the user navigates back to it.
@interface NewTabPageState : NSObject

// Initializes an NTP state with a given scroll position and feed type.
- (instancetype)initWithScrollPosition:(CGFloat)scrollPosition
                          selectedFeed:(FeedType)selectedFeed;

// The saved content offset in the NTP.
@property(nonatomic, assign) CGFloat scrollPosition;

// The currently visible feed.
@property(nonatomic, assign) FeedType selectedFeed;

// Whether the NTP should be scrolled to the top of the feed.
@property(nonatomic, assign) BOOL shouldScrollToTopOfFeed;

@end

#endif  // IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_STATE_H_
