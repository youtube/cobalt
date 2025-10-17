// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_UI_BUNDLED_PAGE_INFO_CONSTANTS_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_UI_BUNDLED_PAGE_INFO_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier for the page info view.
extern NSString* const kPageInfoViewAccessibilityIdentifier;

// Accessibility identifier for the page info's security view.
extern NSString* const kPageInfoSecurityViewAccessibilityIdentifier;

// Accessibility identifier for the navigation bar of page info view.
extern NSString* const kPageInfoViewNavigationBarAccessibilityIdentifier;

// Accessibility identifier for the navigation bar of page info's security view.
extern NSString* const
    kPageInfoSecurityViewNavigationBarAccessibilityIdentifier;

// The left edge insect for cell separators of page info.
extern const CGFloat kPageInfoTableViewSeparatorInset;

// The left edge insect for cell separators of page info with an icon.
extern const CGFloat kPageInfoTableViewSeparatorInsetWithIcon;

// The size of icons in the left hand side of page info.
extern const CGFloat kPageInfoSymbolPointSize;

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_UI_BUNDLED_PAGE_INFO_CONSTANTS_H_
