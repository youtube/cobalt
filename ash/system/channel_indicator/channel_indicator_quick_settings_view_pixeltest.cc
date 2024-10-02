// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/channel_indicator/channel_indicator_quick_settings_view.h"
#include "base/memory/raw_ptr.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/system/unified/quick_settings_header.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test_shell_delegate.h"
#include "base/test/scoped_feature_list.h"
#include "components/version_info/channel.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Parameterized by feature QsRevamp.
class ChannelIndicatorQuickSettingsViewPixelTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ChannelIndicatorQuickSettingsViewPixelTest() = default;
  ChannelIndicatorQuickSettingsViewPixelTest(
      const ChannelIndicatorQuickSettingsViewPixelTest&) = delete;
  ChannelIndicatorQuickSettingsViewPixelTest& operator=(
      const ChannelIndicatorQuickSettingsViewPixelTest&) = delete;
  ~ChannelIndicatorQuickSettingsViewPixelTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitWithFeatureState(features::kQsRevamp, GetParam());

    // Install a test delegate to allow overriding channel version.
    auto delegate = std::make_unique<TestShellDelegate>();
    delegate->set_channel(version_info::Channel::BETA);
    AshTestBase::SetUp(std::move(delegate));

    system_tray_client_ = GetSystemTrayClient();
    system_tray_client_->set_user_feedback_enabled(true);

    // Place the view in a large views::Widget so the buttons are clickable.
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    if (IsQsRevampEnabled()) {
      // Implicitly instantiate the view by creating the quick settings header.
      model_ = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);
      controller_ = std::make_unique<UnifiedSystemTrayController>(model_.get());
      auto header = std::make_unique<QuickSettingsHeader>(controller_.get());
      header_ = header.get();
      widget_->SetContentsView(std::move(header));
    } else {
      // Explicitly instantiate view.
      auto view = std::make_unique<ChannelIndicatorQuickSettingsView>(
          version_info::Channel::BETA,
          system_tray_client_->IsUserFeedbackEnabled());
      view_ = view.get();
      widget_->SetContentsView(std::move(view));
    }
  }

  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void TearDown() override {
    controller_.reset();
    model_.reset();
    widget_.reset();
    AshTestBase::TearDown();
  }

  bool IsQsRevampEnabled() const { return GetParam(); }

  QuickSettingsHeader* header() { return header_; }
  ChannelIndicatorQuickSettingsView* view() {
    return IsQsRevampEnabled() ? header()->channel_view_for_test()
                               : view_.get();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<TestSystemTrayClient, ExperimentalAsh> system_tray_client_ = nullptr;
  scoped_refptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<UnifiedSystemTrayController> controller_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<ChannelIndicatorQuickSettingsView, ExperimentalAsh> view_ = nullptr;
  raw_ptr<QuickSettingsHeader, ExperimentalAsh> header_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(QsRevampEnabled,
                         ChannelIndicatorQuickSettingsViewPixelTest,
                         testing::Bool());

// Verifies the UI when the feedback button is visible.
TEST_P(ChannelIndicatorQuickSettingsViewPixelTest, FeedbackButtonVisible) {
  // Basic verification that buttons are visible before taking screenshot.
  if (IsQsRevampEnabled()) {
    ASSERT_TRUE(header()->GetVisible());
  }
  ASSERT_TRUE(view());
  ASSERT_TRUE(view()->version_button_for_test());
  ASSERT_TRUE(view()->version_button_for_test()->GetVisible());
  ASSERT_TRUE(view()->feedback_button_for_test());
  ASSERT_TRUE(view()->feedback_button_for_test()->GetVisible());

  // Don't capture any part of the UI except for
  // `ChannelIndicatorQuickSettingsView`.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "feedback_button_visible",
      /*revision_number=*/0, view()));
}

}  // namespace ash
