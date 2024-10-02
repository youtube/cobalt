// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_component_options.h"

@class LayoutGuideCenter;
@class ToolbarConfiguration;

// UIButton subclass used as a Toolbar component.
@interface ToolbarButton : UIButton

// Configuration object used to get colors.
@property(nonatomic, weak) ToolbarConfiguration* toolbarConfiguration;
// Bitmask used for SizeClass visibility.
@property(nonatomic, assign) ToolbarComponentVisibility visibilityMask;
// Returns true if the ToolbarButton should be hidden in the current SizeClass.
@property(nonatomic, assign) BOOL hiddenInCurrentSizeClass;
// Returns true if the ToolbarButton should be hidden due to a current UI state
// or WebState.
@property(nonatomic, assign) BOOL hiddenInCurrentState;
// Name of the layout guide this button should be constrained to, if not nil.
// The constraints to the layout guide are only valid when the button is
// displayed. Also, they can be dropped/changed upon size class changes or
// rotations. Any view constrained to them is expected to be dismissed on such
// events.
@property(nonatomic, strong) GuideName* guideName;
// The layout guide center for this button.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;
// Whether this button is highlighted for an IPH, having a blue background. This
// color will override the spotlighted background color.
@property(nonatomic, assign) BOOL iphHighlighted;
// View used to display the view used for the spotlight effect.
@property(nonatomic, strong) UIView* spotlightView;

// Returns a ToolbarButton with a type system, using the `image` as image for
// normal state.
+ (instancetype)toolbarButtonWithImage:(UIImage*)image;

// Checks if the ToolbarButton should be visible in the current SizeClass,
// afterwards it calls setHiddenForCurrentStateAndSizeClass if needed.
- (void)updateHiddenInCurrentSizeClass;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_H_
