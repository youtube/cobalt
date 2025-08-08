// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/test/ink_drop_highlight_test_api.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace {

class TestButton : public views::Button {
 public:
  TestButton() : Button(views::Button::PressedCallback()) {}
  TestButton(const TestButton&) = delete;
  TestButton& operator=(const TestButton&) = delete;
  ~TestButton() override = default;
};

}  // namespace

class ToolbarInkDropUtilTest : public ChromeViewsTestBase,
                               public ::testing::WithParamInterface<bool> {
 public:
  ToolbarInkDropUtilTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Enable or disable the feature based on the test parameter
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(features::kChromeRefresh2023);
    } else {
      feature_list_.InitAndDisableFeature(features::kChromeRefresh2023);
    }

    button_host_ = std::make_unique<TestButton>();
  }

  void TearDown() override { ChromeViewsTestBase::TearDown(); }

  ~ToolbarInkDropUtilTest() override = default;

 protected:
  std::unique_ptr<TestButton> button_host_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Basic test to check for various inkdrop properties for toolbar buttons.
TEST_P(ToolbarInkDropUtilTest, ConfigureInkDropForToolbarTest) {
  ConfigureInkDropForToolbar(button_host_.get());

  if (!features::IsChromeRefresh2023()) {
    EXPECT_TRUE(button_host_->GetHasInkDropActionOnClick());
    EXPECT_EQ(views::InkDrop::Get(button_host_.get())->GetMode(),
              views::InkDropHost::InkDropMode::ON);
    EXPECT_EQ(views::InkDrop::Get(button_host_.get())->GetVisibleOpacity(),
              kToolbarInkDropVisibleOpacity);
  } else {
    EXPECT_EQ(views::InkDrop::Get(button_host_.get())->GetLayerRegion(),
              views::LayerRegion::kAbove);
    std::unique_ptr<views::InkDropHighlight> highlight =
        views::InkDrop::Get(button_host_.get())->CreateInkDropHighlight();
    EXPECT_NE(highlight, nullptr);
  }
}

// Parameterized test cases. The parameter is whether the feature is enabled.
INSTANTIATE_TEST_SUITE_P(ChromeRefresh2023OnOff,
                         ToolbarInkDropUtilTest,
                         testing::Bool());
