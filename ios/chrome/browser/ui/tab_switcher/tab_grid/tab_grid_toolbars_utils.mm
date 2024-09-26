// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_toolbars_utils.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UIView* CreateTabGridScrolledToEdgeBackground() {
  UIView* scrolledToEdgeBackground = [[UIView alloc] init];
  scrolledToEdgeBackground.backgroundColor =
      [UIColor colorNamed:kGridBackgroundColor];

  return scrolledToEdgeBackground;
}

UIView* CreateTabGridOverContentBackground() {
  UIBlurEffect* effect = [UIBlurEffect
      effectWithStyle:UIBlurEffectStyleSystemUltraThinMaterialLight];

  UIVisualEffectView* visualEffectView =
      [[UIVisualEffectView alloc] initWithEffect:effect];
  UIView* background = [[UIView alloc] init];
  background.backgroundColor = [UIColor colorWithWhite:0
                                                 alpha:kToolbarBackgroundAlpha];
  background.translatesAutoresizingMaskIntoConstraints = NO;
  [visualEffectView.contentView addSubview:background];
  AddSameConstraints(visualEffectView, background);

  return visualEffectView;
}
