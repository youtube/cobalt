// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tabs/tab_strip_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabStripView

@synthesize layoutDelegate = _layoutDelegate;

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.showsHorizontalScrollIndicator = NO;
    self.showsVerticalScrollIndicator = NO;
  }
  return self;
}

- (BOOL)touchesShouldCancelInContentView:(UIView*)view {
  // The default implementation returns YES for all views except for UIControls.
  // Override to return YES for UIControls as well.
  return YES;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self.layoutDelegate layoutTabStripSubviews];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self.layoutDelegate traitCollectionDidChange:previousTraitCollection];
  [self.layoutDelegate layoutTabStripSubviews];
}

@end
