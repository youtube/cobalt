// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/scoped_eg_traits_overrider.h"

ScopedTraitOverrider::ScopedTraitOverrider(
    UIViewController* top_view_controller) {
  // Store the top presented view controller.
  top_view_controller_ = top_view_controller;

#if defined(__IPHONE_17_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_17_0
  // Keep original content size category.
  // For other trait overrides, don't forget to add it here. The
  // `traitOverrides` is read only.
  original_content_size_category_ =
      top_view_controller_.traitCollection.preferredContentSizeCategory;
#endif
}

ScopedTraitOverrider::~ScopedTraitOverrider() {
#if defined(__IPHONE_17_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_17_0
  // For other trait overrides, don't forget to remove it here. The
  // `traitOverrides` is read only.
  if (top_view_controller_) {
    top_view_controller_.traitOverrides.preferredContentSizeCategory =
        original_content_size_category_;
  }
#endif
}

void ScopedTraitOverrider::SetContentSizeCategory(
    UIContentSizeCategory new_content_size_category) {
#if defined(__IPHONE_17_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_17_0
  if (top_view_controller_) {
    top_view_controller_.traitOverrides.preferredContentSizeCategory =
        new_content_size_category;
  }
#endif
}
