// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/layout_constants.h"

#import <cmath>

namespace {

// The threshold delta for reporting corner radius changes.
constexpr CGFloat kCornerRadiusUpdateThreshold = 0.1;

}  // namespace

const CGFloat kAppBarCornerRadius = 22.0;
const CGFloat kAppBarCornerRadiusMax = 46.0;

const CGFloat kAssistantSheetSpringStiffness = 384.0;
const CGFloat kAssistantSheetSpringDampingValue = 33.3;

bool IsCornerRadiusChangeSignificant(CGFloat current_radius, CGFloat target_radius) {
  return target_radius == 0.0 ||
         std::abs(current_radius - target_radius) > kCornerRadiusUpdateThreshold;
}
