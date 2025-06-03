// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TOOLBAR_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TOOLBAR_COMMANDS_H_

// Protocol that describes the commands that trigger Toolbar UI changes.
@protocol ToolbarCommands

// Triggers a slide-in animation for the toolbar(s).
- (void)triggerToolbarSlideInAnimation;
// Set the in-product-help highlighted state of the tab grid button.
- (void)setTabGridButtonIPHHighlighted:(BOOL)iphHighlighted;
// Set the in-product-help highlighted state of the new tab button.
- (void)setNewTabButtonIPHHighlighted:(BOOL)iphHighlighted;
// Triggers the in-product-help of the share button after the location bar is
// unfocused. Showing it while the location bar is focused is not desired
// because the share button is hidden on phone factors.
- (void)showShareButtonIPHAfterLocationBarUnfocus;
@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TOOLBAR_COMMANDS_H_
