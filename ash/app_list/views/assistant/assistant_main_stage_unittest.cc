// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_main_stage.h"

#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"

namespace ash {

SkColor GetCenterColor(views::Separator* separator) {
  gfx::Size canvas_size = separator->GetPreferredSize();
  gfx::Canvas canvas(canvas_size, /*image_scale=*/1.0f, /*is_opaque=*/false);
  separator->OnPaint(&canvas);
  return canvas.GetBitmap().getColor(canvas_size.width() / 2,
                                     canvas_size.height() / 2);
}

class AssistantMainStageTest : public AssistantAshTestBase {
 public:
  // AssistantAshTestBase:
  void TearDown() override {
    // NativeTheme instance will be re-used across test cases. Make sure that a
    // test case ends with setting ShouldUseDarkColors to false.
    ASSERT_FALSE(
        ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors());

    AssistantAshTestBase::TearDown();
  }
};

TEST_F(AssistantMainStageTest, DarkAndLightTheme) {
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
  const bool initial_dark_mode_status =
      dark_light_mode_controller->IsDarkModeEnabled();

  ShowAssistantUi();

  views::View* main_stage = page_view()->GetViewByID(kMainStage);
  views::Separator* separator = static_cast<views::Separator*>(
      main_stage->GetViewByID(kHorizontalSeparator));

  EXPECT_EQ(separator->GetColorId(), ui::kColorAshSystemUIMenuSeparator);

  // Switch the color mode.
  dark_light_mode_controller->ToggleColorMode();
  ASSERT_NE(initial_dark_mode_status,
            dark_light_mode_controller->IsDarkModeEnabled());

  EXPECT_EQ(separator->GetColorId(), ui::kColorAshSystemUIMenuSeparator);
  EXPECT_EQ(GetCenterColor(separator), separator->GetColorProvider()->GetColor(
                                           ui::kColorAshSystemUIMenuSeparator));

  // Turn off dark mode, this will make NativeTheme::ShouldUseDarkColors return
  // false. See a comment in TearDown about details.
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kDarkModeEnabled, false);
}

}  // namespace ash
