// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONSUMER_H_

#import <Foundation/Foundation.h>

// Handles updates to the NTP ViewController.
@protocol NewTabPageConsumer <NSObject>

// When the omnibox is focused, this value represents the scroll distance needed
// to pin the omnibox to the top. It is 0 if no scrolling was done in order pin
// the omnibox to the top (i.e. the NTP ScrollView was already scrolled far
// enough down that the omnibo was already pinned to the top).
@property(nonatomic, assign, readonly) CGFloat collectionShiftingOffset;

// Indicates that the omnibox stopped being the first responder to the keyboard.
- (void)omniboxDidResignFirstResponder;

// Sets the feed collection contentOffset from the saved state to `offset` to
// set the initial scroll position.
- (void)setSavedContentOffset:(CGFloat)offset;

// Sets the feed collection contentOffset to the top of the page. Resets fake
// omnibox back to initial state.
- (void)setContentOffsetToTop;

// Returns the height of the content above the feed. The views above the feed
// (like the content suggestions) are added through a content inset in the feed
// collection view, so this property is used to track the total height of those
// additional views.
- (CGFloat)heightAboveFeed;

// Returns the y content offset of the NTP collection view.
- (CGFloat)scrollPosition;

// Returns the Y value to use for the scroll view's contentOffset when scrolling
// the omnibox to the top of the screen.
- (CGFloat)pinnedOffsetY;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONSUMER_H_
