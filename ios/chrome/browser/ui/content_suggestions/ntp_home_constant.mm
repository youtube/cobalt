// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace ntp_home {
NSString* FakeOmniboxAccessibilityID() {
  return @"NTPHomeFakeOmniboxAccessibilityID";
}

NSString* DiscoverHeaderTitleAccessibilityID() {
  return @"DiscoverHeaderTitleAccessibilityID";
}

NSString* NTPLogoAccessibilityID() {
  return @"NTPLogoAccessibilityID";
}

const CGFloat kMostVisitedBottomMarginIPad = 80;
const CGFloat kMostVisitedBottomMarginIPhone = 60;
const CGFloat kSuggestionPeekingHeight = 60;

const CGFloat kIdentityAvatarDimension = 32;
const CGFloat kIdentityAvatarMargin = 16;
const CGFloat kSignedOutIdentityIconDimension = 24;

UIColor* NTPBackgroundColor() {
  return [UIColor colorNamed:kBackgroundColor];
}

}  // namespace ntp_home
