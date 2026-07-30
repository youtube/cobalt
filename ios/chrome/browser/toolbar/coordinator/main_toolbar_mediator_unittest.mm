// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/main_toolbar_mediator.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface MainToolbarMediator (Testing)
- (BOOL)isBottomOmniboxPrefEnabled;
@end

class MainToolbarMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    feature_list_.InitAndEnableFeature(kChromeNextIa);
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterBooleanPref(omnibox::kIsOmniboxInBottomPosition,
                                            false);

    layout_state_ = [[LayoutState alloc] init];
    mediator_ = [[MainToolbarMediator alloc] initWithPrefService:prefs_.get()
                                                     layoutState:layout_state_];
  }

  void TearDown() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  LayoutState* layout_state_;
  MainToolbarMediator* mediator_;
  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Tests that the mediator correctly reports the omnibox position and updates
// the layout state when it changes.
TEST_F(MainToolbarMediatorTest, TestPrefChangeUpdatesLayoutState) {
  EXPECT_FALSE([mediator_ isBottomOmniboxPrefEnabled]);
  EXPECT_EQ(layout_state_.toolbarPosition, ToolbarPosition::kTop);

  prefs_->SetBoolean(omnibox::kIsOmniboxInBottomPosition, true);

  EXPECT_EQ(layout_state_.toolbarPosition, IsBottomOmniboxAvailable()
                                               ? ToolbarPosition::kBottom
                                               : ToolbarPosition::kTop);
  EXPECT_TRUE([mediator_ isBottomOmniboxPrefEnabled] ||
              !IsBottomOmniboxAvailable());
}
