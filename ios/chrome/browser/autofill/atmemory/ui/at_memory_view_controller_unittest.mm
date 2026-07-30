// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_view_controller.h"

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_consumer.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_empty_state_view_controller.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using at_memory::AtMemoryContentState;

using AtMemoryViewControllerTest = PlatformTest;

// Tests that setting all possible AtMemoryContentState values on the view
// controller transitions to the correct child view controller states.
TEST_F(AtMemoryViewControllerTest, TransitionsToCorrectChildViewControllers) {
  AtMemoryViewController* viewController =
      [[AtMemoryViewController alloc] init];
  // Trigger view load.
  (void)viewController.view;

  [viewController setContentState:AtMemoryContentState::kEmpty];
  EXPECT_EQ(viewController.childViewControllers.count, 1u);
  EXPECT_TRUE([viewController.childViewControllers.firstObject
      isKindOfClass:[AtMemoryEmptyStateViewController class]]);

  // TODO(crbug.com/522326512): Verify other states when they are implemented.
  [viewController setContentState:AtMemoryContentState::kPreviouslyFilled];
  EXPECT_EQ(viewController.childViewControllers.count, 0u);

  [viewController setContentState:AtMemoryContentState::kSearch];
  EXPECT_EQ(viewController.childViewControllers.count, 0u);

  [viewController setContentState:AtMemoryContentState::kSearchResults];
  EXPECT_EQ(viewController.childViewControllers.count, 0u);

  [viewController setContentState:AtMemoryContentState::kQueryUnsupported];
  EXPECT_EQ(viewController.childViewControllers.count, 0u);

  [viewController setContentState:AtMemoryContentState::kNoData];
  EXPECT_EQ(viewController.childViewControllers.count, 0u);
}

}  // namespace
