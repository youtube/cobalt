// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_FOLLOW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_FOLLOW_DELEGATE_H_

// Actions taken on the feed management UI related to follow management.
@protocol FeedManagementFollowDelegate

// User tapped to see the WebChannels they are following.
- (void)handleFollowingTapped;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_FOLLOW_DELEGATE_H_
