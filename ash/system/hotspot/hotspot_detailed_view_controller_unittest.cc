// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hotspot/hotspot_detailed_view_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/style/switch.h"
#include "ash/system/hotspot/hotspot_detailed_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/hotspot_config/public/cpp/cros_hotspot_config_test_helper.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "ui/views/view.h"

namespace ash {

using hotspot_config::mojom::HotspotAllowStatus;
using hotspot_config::mojom::HotspotConfig;
using hotspot_config::mojom::HotspotConfigPtr;
using hotspot_config::mojom::HotspotInfo;
using hotspot_config::mojom::HotspotState;

class HotspotDetailedViewControllerTest : public AshTestBase {
 public:
  HotspotDetailedViewControllerTest() = default;
  ~HotspotDetailedViewControllerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kHotspot, features::kQsRevamp}, {});
    cros_hotspot_config_test_helper_ =
        std::make_unique<hotspot_config::CrosHotspotConfigTestHelper>(
            /*use_fake_implementation=*/true);
    AshTestBase::SetUp();

    GetPrimaryUnifiedSystemTray()->ShowBubble();

    UnifiedSystemTrayController* unified_system_tray_controller =
        GetPrimaryUnifiedSystemTray()
            ->bubble()
            ->unified_system_tray_controller();
    unified_system_tray_controller->ShowHotspotDetailedView();
    hotspot_detailed_view_controller_ =
        static_cast<HotspotDetailedViewController*>(
            unified_system_tray_controller->detailed_view_controller());

    // Spin the runloop to have hotspot_detailed_view_controller_ observe the
    // hotspot info change.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    cros_hotspot_config_test_helper_.reset();
  }

  void UpdateHotspotInfo(HotspotState state,
                         HotspotAllowStatus allow_status,
                         int client_count = 0,
                         HotspotConfigPtr config = {}) {
    auto hotspot_info = HotspotInfo::New();
    hotspot_info->state = state;
    hotspot_info->allow_status = allow_status;
    hotspot_info->client_count = client_count;
    hotspot_info->config = std::move(config);
    cros_hotspot_config_test_helper_->SetFakeHotspotInfo(
        std::move(hotspot_info));
    // Spin the runloop to observe the hotspot info change.
    base::RunLoop().RunUntilIdle();
  }

  void EnableAndDisableHotspotOnce() {
    UpdateHotspotInfo(HotspotState::kEnabled, HotspotAllowStatus::kAllowed);
    UpdateHotspotInfo(HotspotState::kDisabled, HotspotAllowStatus::kAllowed);
  }

  HotspotDetailedView::Delegate* hotspot_detailed_view_delegate() {
    return hotspot_detailed_view_controller_;
  }

  HotspotDetailedView* hotspot_detailed_view() {
    return hotspot_detailed_view_controller_->view_;
  }

  Switch* GetToggleButton() { return hotspot_detailed_view()->toggle_; }

  void AssertSubtextLabel(const std::u16string& expected_text) {
    HoverHighlightView* entry_row = hotspot_detailed_view()->entry_row_;
    if (expected_text.empty()) {
      EXPECT_FALSE(entry_row->sub_text_label());
      return;
    }
    ASSERT_TRUE(entry_row->sub_text_label());
    EXPECT_TRUE(entry_row->sub_text_label()->GetVisible());
    EXPECT_EQ(expected_text, entry_row->sub_text_label()->GetText());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<HotspotDetailedViewController, DanglingUntriaged | ExperimentalAsh>
      hotspot_detailed_view_controller_;
  std::unique_ptr<hotspot_config::CrosHotspotConfigTestHelper>
      cros_hotspot_config_test_helper_;
};

TEST_F(HotspotDetailedViewControllerTest, ToggleClicked) {
  ASSERT_TRUE(hotspot_detailed_view_controller_);
  UpdateHotspotInfo(HotspotState::kDisabled, HotspotAllowStatus::kAllowed);

  ASSERT_TRUE(hotspot_detailed_view());
  Switch* toggle = GetToggleButton();
  ASSERT_TRUE(toggle);
  EXPECT_FALSE(toggle->GetIsOn());

  hotspot_detailed_view_delegate()->OnToggleClicked(/*new_state=*/true);
  // Spin the runloop to have cros_hotspot_config finish enable hotspot and
  // notify the hotspot_info change.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(toggle->GetIsOn());
  hotspot_detailed_view_delegate()->OnToggleClicked(/*new_state=*/false);

  // Spin the runloop to have cros_hotspot_config finish disable hotspot and
  // notify the hotspot_info change.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(toggle->GetIsOn());
}

TEST_F(HotspotDetailedViewControllerTest, NotifiesWhenHotspotInfoChanges) {
  ASSERT_TRUE(hotspot_detailed_view_controller_);
  UpdateHotspotInfo(HotspotState::kDisabled, HotspotAllowStatus::kAllowed);

  ASSERT_TRUE(hotspot_detailed_view());
  Switch* toggle = GetToggleButton();
  ASSERT_TRUE(toggle);
  EXPECT_FALSE(toggle->GetIsOn());

  UpdateHotspotInfo(HotspotState::kEnabled, HotspotAllowStatus::kAllowed);
  EXPECT_TRUE(toggle->GetIsOn());
  AssertSubtextLabel(u"No devices connected");

  UpdateHotspotInfo(HotspotState::kEnabled, HotspotAllowStatus::kAllowed,
                    /*client_count=*/1);
  EXPECT_TRUE(toggle->GetIsOn());
  AssertSubtextLabel(u"1 device connected");

  // Updating the hotspot configuration should not update hotspot detailed
  // view.
  auto config = HotspotConfig::New();
  config->ssid = "hotspot_ssid";
  config->passphrase = "hotspot_passphrase";
  UpdateHotspotInfo(HotspotState::kEnabled, HotspotAllowStatus::kAllowed,
                    /*client_count=*/1, std::move(config));
  EXPECT_TRUE(toggle->GetIsOn());
  AssertSubtextLabel(u"1 device connected");
}

}  // namespace ash
