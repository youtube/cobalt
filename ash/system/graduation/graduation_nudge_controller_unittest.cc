// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/graduation/graduation_nudge_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/edusumer/graduation_prefs.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr char kAppId[] = "test_id";
constexpr char kInvalidAppId[] = "invalid_app_id";
constexpr NudgeCatalogName kGraduationNudgeCatalogName =
    NudgeCatalogName::kGraduationAppEnabled;
constexpr char kNudgeShownCount[] = "Ash.NotifierFramework.Nudge.ShownCount";

bool IsNudgeShown() {
  return Shell::Get()->anchored_nudge_manager()->IsNudgeShown(
      "graduation.nudge");
}

gfx::ImageSkia CreateImageSkiaIcon() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(SK_ColorRED);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

}  // namespace

class GraduationNudgeControllerTest : public AshTestBase {
 public:
  GraduationNudgeControllerTest() {
    graduation_prefs::RegisterProfilePrefs(profile_prefs_.registry());
    nudge_controller_ = std::make_unique<graduation::GraduationNudgeController>(
        &profile_prefs_);
  }

  void SetUp() override {
    AshTestBase::SetUp();
    test_api_ = std::make_unique<ShelfViewTestAPI>(
        GetPrimaryShelf()->GetShelfViewForTesting());
    test_api_->SetAnimationDuration(base::Milliseconds(1));
  }

  graduation::GraduationNudgeController* nudge_controller() {
    return nudge_controller_.get();
  }

  // Add shelf items of various types, and optionally wait for animations.
  ShelfID AddItem(ShelfItemType type, bool wait_for_animations) {
    ShelfItem item = ShelfTestUtil::AddAppShortcutWithIcon(
        kAppId, type, CreateImageSkiaIcon());
    if (wait_for_animations) {
      test_api_->RunMessageLoopUntilAnimationsDone();
    }
    return item.id;
  }

  void SetNudgeShownCountPref(int count) {
    profile_prefs_.SetInteger(prefs::kGraduationNudgeShownCount, count);
  }

  void SetLastShownNudgeTimePref(base::Time time) {
    profile_prefs_.SetTime(prefs::kGraduationNudgeLastShownTime, time);
  }

 private:
  std::unique_ptr<graduation::GraduationNudgeController> nudge_controller_;
  TestingPrefServiceSimple profile_prefs_;
  std::unique_ptr<ShelfViewTestAPI> test_api_;
};

TEST_F(GraduationNudgeControllerTest, NudgeNotShownWhenAppNotInstalled) {
  EXPECT_FALSE(IsNudgeShown());
  nudge_controller()->MaybeShowNudge(ShelfID("testid"));
  EXPECT_FALSE(IsNudgeShown());
}

TEST_F(GraduationNudgeControllerTest, NudgeShownWhenAppInstalled) {
  ShelfID added_item = AddItem(ShelfItemType::TYPE_PINNED_APP, true);
  EXPECT_FALSE(IsNudgeShown());
  nudge_controller()->MaybeShowNudge(added_item);
  EXPECT_TRUE(IsNudgeShown());
}

TEST_F(GraduationNudgeControllerTest, NudgeNotShownWhenShownMaximumTimes) {
  SetNudgeShownCountPref(3);
  base::Time two_days_ago = base::Time::Now() - base::Days(2);
  SetLastShownNudgeTimePref(two_days_ago);
  ShelfID added_item = AddItem(ShelfItemType::TYPE_PINNED_APP, true);

  EXPECT_FALSE(IsNudgeShown());
  nudge_controller()->MaybeShowNudge(added_item);
  EXPECT_FALSE(IsNudgeShown());
}

TEST_F(GraduationNudgeControllerTest, NudgeNotShownWhenShownLessThanADayAgo) {
  SetNudgeShownCountPref(1);
  base::Time twelve_hours_ago = base::Time::Now() - base::Hours(12);
  SetLastShownNudgeTimePref(twelve_hours_ago);
  ShelfID added_item = AddItem(ShelfItemType::TYPE_PINNED_APP, true);

  EXPECT_FALSE(IsNudgeShown());
  nudge_controller()->MaybeShowNudge(added_item);
  EXPECT_FALSE(IsNudgeShown());
}

TEST_F(GraduationNudgeControllerTest, NudgeShownWhenLastShownOverADayAgo) {
  SetNudgeShownCountPref(1);
  base::Time two_days_ago = base::Time::Now() - base::Days(2);
  SetLastShownNudgeTimePref(two_days_ago);

  ShelfID added_item = AddItem(ShelfItemType::TYPE_PINNED_APP, true);

  EXPECT_FALSE(IsNudgeShown());
  nudge_controller()->MaybeShowNudge(added_item);
  EXPECT_TRUE(IsNudgeShown());
}

TEST_F(GraduationNudgeControllerTest, NudgeShownIfPrefReset) {
  SetNudgeShownCountPref(3);
  base::Time two_days_ago = base::Time::Now() - base::Days(2);
  SetLastShownNudgeTimePref(two_days_ago);
  ShelfID added_item = AddItem(ShelfItemType::TYPE_PINNED_APP, true);

  EXPECT_FALSE(IsNudgeShown());
  nudge_controller()->MaybeShowNudge(added_item);
  EXPECT_FALSE(IsNudgeShown());

  nudge_controller()->ResetNudgePref();
  nudge_controller()->MaybeShowNudge(added_item);

  EXPECT_TRUE(IsNudgeShown());
}

TEST_F(GraduationNudgeControllerTest, EnableAppWhenHotseatHidden) {
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlwaysHidden);

  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  // Activate the window and go to tablet mode.
  wm::ActivateWindow(window.get());
  TabletModeControllerTestApi().EnterTabletMode();

  EXPECT_EQ(
      HotseatState::kHidden,
      AshTestBase::GetPrimaryShelf()->shelf_layout_manager()->hotseat_state());

  ShelfID added_item = AddItem(ShelfItemType::TYPE_PINNED_APP, true);
  EXPECT_FALSE(IsNudgeShown());
  nudge_controller()->MaybeShowNudge(added_item);
  EXPECT_FALSE(IsNudgeShown());
}

TEST_F(GraduationNudgeControllerTest, RecordNudgeShownCountMetric) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectUniqueSample(kNudgeShownCount,
                                      kGraduationNudgeCatalogName, 0);

  ShelfID added_item = AddItem(ShelfItemType::TYPE_PINNED_APP, true);
  nudge_controller()->MaybeShowNudge(added_item);

  histogram_tester.ExpectUniqueSample(kNudgeShownCount,
                                      kGraduationNudgeCatalogName, 1);
}

TEST_F(GraduationNudgeControllerTest, RecordShowNudgeFailedMetric) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectUniqueSample(
      graduation::GraduationNudgeController::kShowNudgeFailedHistogramName,
      graduation::GraduationNudgeController::ShowNudgeFailureReason::
          kAppIconUnavailable,
      0);

  nudge_controller()->MaybeShowNudge(ShelfID(kInvalidAppId));

  histogram_tester.ExpectUniqueSample(
      graduation::GraduationNudgeController::kShowNudgeFailedHistogramName,
      graduation::GraduationNudgeController::ShowNudgeFailureReason::
          kAppIconUnavailable,
      1);
}

}  // namespace ash
