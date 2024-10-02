// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/button_util.h"

#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const CGFloat kButtonVerticalInsets = 14.5;
const CGFloat kPrimaryButtonCornerRadius = 15;

UIButton* PrimaryActionButton(BOOL pointer_interaction_enabled) {
  UIButton* primary_blue_button = [UIButton buttonWithType:UIButtonTypeSystem];

  // TODO(crbug.com/1418068): Replace with UIButtonConfiguration when min
  // deployment target is iOS 15.
  UIEdgeInsets contentInsets =
      UIEdgeInsetsMake(kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
  SetContentEdgeInsets(primary_blue_button, contentInsets);

  [primary_blue_button setBackgroundColor:[UIColor colorNamed:kBlueColor]];
  UIColor* titleColor = [UIColor colorNamed:kSolidButtonTextColor];
  [primary_blue_button setTitleColor:titleColor forState:UIControlStateNormal];
  primary_blue_button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  primary_blue_button.layer.cornerRadius = kPrimaryButtonCornerRadius;
  primary_blue_button.titleLabel.adjustsFontForContentSizeCategory = NO;
  primary_blue_button.translatesAutoresizingMaskIntoConstraints = NO;

  if (pointer_interaction_enabled) {
    primary_blue_button.pointerInteractionEnabled = YES;
    primary_blue_button.pointerStyleProvider =
        CreateOpaqueButtonPointerStyleProvider();
  }

  return primary_blue_button;
}
