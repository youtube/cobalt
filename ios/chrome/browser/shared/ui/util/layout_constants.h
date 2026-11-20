// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_LAYOUT_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_LAYOUT_CONSTANTS_H_

#import <UIKit/UIKit.h>

// The default corner radius for the App Bar cutout.
extern const CGFloat kAppBarCornerRadius;

// The maximum corner radius offset supported for layout transitions.
extern const CGFloat kAppBarCornerRadiusMax;

// Spring constants for the assistant sheet transition animations.
extern const CGFloat kAssistantSheetSpringStiffness;
extern const CGFloat kAssistantSheetSpringDampingValue;

// Returns whether the change between `current_radius` and `target_radius` is
// significant enough to warrant updating layout/path animations (> 0.1 delta),
// or if the target radius is 0.0 (dismissed).
bool IsCornerRadiusChangeSignificant(CGFloat current_radius, CGFloat target_radius);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_LAYOUT_CONSTANTS_H_
