// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hotspot/hotspot_tray_view.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/hotspot_config/public/cpp/cros_hotspot_config_test_helper.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

using hotspot_config::mojom::HotspotInfo;
using hotspot_config::mojom::HotspotState;

namespace {

bool AreImagesEqual(const gfx::ImageSkia& image,
                    const gfx::ImageSkia& reference) {
  return gfx::test::AreBitmapsEqual(*image.bitmap(), *reference.bitmap());
}

}  // namespace

class HotspotTrayViewTest : public AshTestBase,
                            public testing::WithParamInterface<bool> {
 public:
  HotspotTrayViewTest()
      : AshTestBase(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {}
  ~HotspotTrayViewTest() override = default;

  void SetUp() override {
    if (IsJellyEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {features::kHotspot, chromeos::features::kJelly}, {});
    } else {
      scoped_feature_list_.InitAndEnableFeature(features::kHotspot);
    }
    cros_hotspot_config_test_helper_ =
        std::make_unique<hotspot_config::CrosHotspotConfigTestHelper>(
            /*use_fake_implementation=*/true);
    AshTestBase::SetUp();
    std::unique_ptr<HotspotTrayView> hotspot_tray_view =
        std::make_unique<HotspotTrayView>(GetPrimaryShelf());
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    hotspot_tray_view_ = widget_->SetContentsView(std::move(hotspot_tray_view));

    // Spin the runloop to sync up the latest hotspot info.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    widget_.reset();
    cros_hotspot_config_test_helper_.reset();
    AshTestBase::TearDown();
  }

  void SetHotspotStateAndClientCount(HotspotState state, size_t client_count) {
    auto hotspot_info = HotspotInfo::New();
    hotspot_info->state = state;
    hotspot_info->client_count = client_count;
    cros_hotspot_config_test_helper_->SetFakeHotspotInfo(
        std::move(hotspot_info));
    base::RunLoop().RunUntilIdle();
  }

  std::u16string GetTooltip() {
    return hotspot_tray_view_->GetTooltipText(gfx::Point());
  }

  std::u16string GetAccessibleNameString() {
    return hotspot_tray_view_->GetAccessibleNameString();
  }

  bool IsIconVisible() { return hotspot_tray_view_->GetVisible(); }

  bool IsJellyEnabled() const { return GetParam(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<hotspot_config::CrosHotspotConfigTestHelper>
      cros_hotspot_config_test_helper_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<HotspotTrayView, DanglingUntriaged | ExperimentalAsh>
      hotspot_tray_view_;
};

INSTANTIATE_TEST_SUITE_P(Jelly, HotspotTrayViewTest, testing::Bool());

TEST_P(HotspotTrayViewTest, HotspotIconImage) {
  SetHotspotStateAndClientCount(HotspotState::kDisabled, 0);
  EXPECT_TRUE(AreImagesEqual(
      hotspot_tray_view_->image_view()->GetImage(),
      gfx::CreateVectorIcon(kHotspotOffIcon, kUnifiedTrayIconSize,
                            widget_->GetColorProvider()->GetColor(
                                cros_tokens::kCrosSysOnSurface))));

  SetHotspotStateAndClientCount(HotspotState::kEnabled, 0);
  EXPECT_TRUE(AreImagesEqual(
      hotspot_tray_view_->image_view()->GetImage(),
      gfx::CreateVectorIcon(kHotspotOnIcon, kUnifiedTrayIconSize,
                            widget_->GetColorProvider()->GetColor(
                                cros_tokens::kCrosSysOnSurface))));

  SetHotspotStateAndClientCount(HotspotState::kEnabling, 0);
  EXPECT_TRUE(AreImagesEqual(
      hotspot_tray_view_->image_view()->GetImage(),
      gfx::CreateVectorIcon(kHotspotDotIcon, kUnifiedTrayIconSize,
                            widget_->GetColorProvider()->GetColor(
                                cros_tokens::kCrosSysOnSurface))));
  // Verifies the hotspot icon is animating when enabling.
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_TRUE(AreImagesEqual(
      hotspot_tray_view_->image_view()->GetImage(),
      gfx::CreateVectorIcon(kHotspotOneArcIcon, kUnifiedTrayIconSize,
                            widget_->GetColorProvider()->GetColor(
                                cros_tokens::kCrosSysOnSurface))));
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_TRUE(AreImagesEqual(
      hotspot_tray_view_->image_view()->GetImage(),
      gfx::CreateVectorIcon(kHotspotOnIcon, kUnifiedTrayIconSize,
                            widget_->GetColorProvider()->GetColor(
                                cros_tokens::kCrosSysOnSurface))));
}

TEST_P(HotspotTrayViewTest, HotspotIconVisibility) {
  EXPECT_FALSE(IsIconVisible());

  SetHotspotStateAndClientCount(HotspotState::kEnabled, 0);
  EXPECT_TRUE(IsIconVisible());

  SetHotspotStateAndClientCount(HotspotState::kDisabled, 0);
  EXPECT_FALSE(IsIconVisible());

  SetHotspotStateAndClientCount(HotspotState::kEnabling, 0);
  EXPECT_TRUE(IsIconVisible());
}

TEST_P(HotspotTrayViewTest, HotspotIconTooltip) {
  SetHotspotStateAndClientCount(HotspotState::kEnabled, 0);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_HOTSPOT_ON_NO_CONNECTED_DEVICES,
                ui::GetChromeOSDeviceName()),
            GetTooltip());
  EXPECT_EQ(GetTooltip(), GetAccessibleNameString());

  SetHotspotStateAndClientCount(HotspotState::kEnabled, 1);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_HOTSPOT_ON_ONE_CONNECTED_DEVICE,
                ui::GetChromeOSDeviceName()),
            GetTooltip());
  EXPECT_EQ(GetTooltip(), GetAccessibleNameString());

  SetHotspotStateAndClientCount(HotspotState::kEnabled, 3);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_HOTSPOT_ON_MULTIPLE_CONNECTED_DEVICES,
                base::NumberToString16(3), ui::GetChromeOSDeviceName()),
            GetTooltip());
  EXPECT_EQ(GetTooltip(), GetAccessibleNameString());
}

}  // namespace ash
