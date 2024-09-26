// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Key for saving collapsed session state in the UserDefaults.
extern NSString* const kCollapsedSectionsKey;
// Accessibility identifier for the main view.
extern NSString* const kRecentTabsTableViewControllerAccessibilityIdentifier;
// Accessibility identifier for the "Show History" cell.
extern NSString* const kRecentTabsShowFullHistoryCellAccessibilityIdentifier;
// Accessibility identifier for the Illustrated cell in the Other Devices
// section used on empty states.
extern NSString* const
    kRecentTabsOtherDevicesIllustratedCellAccessibilityIdentifier;

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_CONSTANTS_H_
