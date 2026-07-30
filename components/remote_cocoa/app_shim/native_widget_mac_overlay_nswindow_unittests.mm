// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/native_widget_mac_overlay_nswindow.h"

#include <AppKit/AppKit.h>

#include "base/containers/fixed_flat_map.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/cocoa/window_size_constants.h"

using NativeWidgetMacOverlayNSWindowTest = PlatformTest;

// Test that only allowed collection behaviors are able to be set.
TEST(NativeWidgetMacOverlayNSWindowTest, CollectionBehavior) {
  NativeWidgetMacOverlayNSWindow* overlay_window =
      [[NativeWidgetMacOverlayNSWindow alloc]
          initWithContentRect:ui::kWindowSizeDeterminedLater
                    styleMask:NSWindowStyleMaskBorderless
                      backing:NSBackingStoreBuffered
                        defer:NO];

  const auto behavior_tests =
      base::MakeFixedFlatMap<NSWindowCollectionBehavior, bool>({
          {NSWindowCollectionBehaviorCanJoinAllSpaces, false},
          {NSWindowCollectionBehaviorMoveToActiveSpace, false},

          {NSWindowCollectionBehaviorManaged, false},
          {NSWindowCollectionBehaviorTransient, true},  // Allowed
          {NSWindowCollectionBehaviorStationary, false},

          {NSWindowCollectionBehaviorParticipatesInCycle, false},
          {NSWindowCollectionBehaviorIgnoresCycle, true},  // Allowed

          {NSWindowCollectionBehaviorFullScreenPrimary, false},
          {NSWindowCollectionBehaviorFullScreenAuxiliary, false},
          {NSWindowCollectionBehaviorFullScreenNone, false},

          {NSWindowCollectionBehaviorFullScreenAllowsTiling, false},
          {NSWindowCollectionBehaviorFullScreenDisallowsTiling, false},

          {NSWindowCollectionBehaviorPrimary, false},
          {NSWindowCollectionBehaviorAuxiliary, false},
          {NSWindowCollectionBehaviorCanJoinAllApplications, false},
      });

  // Ensure only the allowed bits are able to be set.
  ASSERT_EQ(overlay_window.collectionBehavior,
            NSWindowCollectionBehaviorDefault);
  for (auto const& [behavior, allowed] : behavior_tests) {
    overlay_window.collectionBehavior |= behavior;
    EXPECT_EQ(!!(overlay_window.collectionBehavior & behavior), allowed)
        << "NSWindowCollectionBehavior: " << behavior;
  }

  // Also test setting multiple bits at once.
  overlay_window.collectionBehavior = NSWindowCollectionBehaviorDefault;
  ASSERT_EQ(overlay_window.collectionBehavior,
            NSWindowCollectionBehaviorDefault);
  overlay_window.collectionBehavior = ~NSWindowCollectionBehaviorDefault;
  for (auto const& [behavior, allowed] : behavior_tests) {
    EXPECT_EQ(!!(overlay_window.collectionBehavior & behavior), allowed)
        << "NSWindowCollectionBehavior: " << behavior;
  }
}
