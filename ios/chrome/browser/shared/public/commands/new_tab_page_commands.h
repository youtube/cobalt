// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_NEW_TAB_PAGE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_NEW_TAB_PAGE_COMMANDS_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/discover_feed/feed_constants.h"

// Commands related to the new tab page.
@protocol NewTabPageCommands

// Opens a new tab page scrolled into the feed with a given `feedType` selected.
- (void)openNTPScrolledIntoFeedType:(FeedType)feedType;

// Notifies the new tab page if there is new unseen content in the Following
// feed.
- (void)updateFollowingFeedHasUnseenContent:(BOOL)hasUnseenContent;

// Notifies the new tab page that the `feedType`'s model has completed updates.
// This can include, initial loading of cards, pagination, card removal, and
// refreshes.
- (void)handleFeedModelDidEndUpdates:(FeedType)feedType;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_NEW_TAB_PAGE_COMMANDS_H_
