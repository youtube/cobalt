// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_controller.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/fps_counter.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/test_window_builder.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_metrics_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

constexpr int kCaretHeightForTest = 8;

// The observer to observe the overview states in |root_window_|.
class OverviewStatesObserver : public OverviewObserver {
 public:
  explicit OverviewStatesObserver(aura::Window* root_window)
      : root_window_(root_window) {
    Shell::Get()->overview_controller()->AddObserver(this);
  }

  OverviewStatesObserver(const OverviewStatesObserver&) = delete;
  OverviewStatesObserver& operator=(const OverviewStatesObserver&) = delete;

  ~OverviewStatesObserver() override {
    Shell::Get()->overview_controller()->RemoveObserver(this);
  }

  // OverviewObserver:
  void OnOverviewModeStarting() override {
    // Reset the value to true.
    overview_animate_when_exiting_ = true;
  }
  void OnOverviewModeEnding(OverviewSession* overview_session) override {
    OverviewGrid* grid = overview_session->GetGridWithRootWindow(root_window_);
    if (!grid) {
      return;
    }
    overview_animate_when_exiting_ = grid->should_animate_when_exiting();
  }

  bool overview_animate_when_exiting() const {
    return overview_animate_when_exiting_;
  }

 private:
  bool overview_animate_when_exiting_ = true;
  raw_ptr<aura::Window, ExperimentalAsh> root_window_;
};

// The test BubbleDialogDelegateView for bubbles.
class TestBubbleDialogDelegateView : public views::BubbleDialogDelegateView {
 public:
  explicit TestBubbleDialogDelegateView(views::View* anchor_view)
      : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::NONE) {}

  TestBubbleDialogDelegateView(const TestBubbleDialogDelegateView&) = delete;
  TestBubbleDialogDelegateView& operator=(const TestBubbleDialogDelegateView&) =
      delete;

  ~TestBubbleDialogDelegateView() override {}
};

// Helper class to simulate the text input field in a window. When the text
// input field is focused, the attached window will also be focused and show the
// virtual keyboard. If the text input field is unfocused, it will hide the
// virtual keyboard.
class TestTextInputClient : public ui::DummyTextInputClient {
 public:
  explicit TestTextInputClient(aura::Window* window)
      : ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT), window_(window) {
    DCHECK(window_);
  }
  TestTextInputClient(const TestTextInputClient&) = delete;
  TestTextInputClient& operator=(const TestTextInputClient&) = delete;
  ~TestTextInputClient() override {
    auto* ime = keyboard::KeyboardUIController::Get()->GetInputMethodForTest();
    ime->DetachTextInputClient(this);
  }

  // ui::DummyTextInputClient:
  gfx::Rect GetCaretBounds() const override { return caret_bounds_; }

  void set_caret_bounds(gfx::Rect caret_bounds) {
    caret_bounds_ = caret_bounds;
  }

  // When the text client is focused, the attached window will also be focused
  // and the virtual keyboard is enabled.
  void Focus() {
    auto* ime = keyboard::KeyboardUIController::Get()->GetInputMethodForTest();
    ime->SetFocusedTextInputClient(this);

    if (window_) {
      window_->Focus();
    }

    ime->SetVirtualKeyboardVisibilityIfEnabled(true);
    ASSERT_TRUE(keyboard::WaitUntilShown());
  }

  // When the text client is unfocused, hide the virtual keyboard.
  void UnFocus() {
    auto* ime = keyboard::KeyboardUIController::Get()->GetInputMethodForTest();
    ime->DetachTextInputClient(this);
    keyboard::KeyboardUIController::Get()->HideKeyboardExplicitlyBySystem();
  }

 private:
  // The window to which the text client attaches to.
  raw_ptr<aura::Window, ExperimentalAsh> window_;
  // The bounds of the caret.
  gfx::Rect caret_bounds_;
};

}  // namespace

class SplitViewControllerTest : public AshTestBase {
 public:
  SplitViewControllerTest() = default;

  SplitViewControllerTest(const SplitViewControllerTest&) = delete;
  SplitViewControllerTest& operator=(const SplitViewControllerTest&) = delete;

  ~SplitViewControllerTest() override = default;

  // test::AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    // Avoid TabletModeController::OnGetSwitchStates() from disabling tablet
    // mode.
    base::RunLoop().RunUntilIdle();
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    FpsCounter::SetForceReportZeroAnimationForTest(true);
    ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        true);
  }
  void TearDown() override {
    ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        false);
    FpsCounter::SetForceReportZeroAnimationForTest(false);
    trace_names_.clear();
    AshTestBase::TearDown();
  }

  std::unique_ptr<aura::Window> CreateWindow(
      const gfx::Rect& bounds,
      aura::client::WindowType type = aura::client::WINDOW_TYPE_NORMAL) {
    std::unique_ptr<aura::Window> window =
        TestWindowBuilder()
            .SetBounds(bounds)
            .SetDelegate(new SplitViewTestWindowDelegate)
            .SetWindowType(type)
            .Build();
    // Create non maximizable window so that it's centered when created,
    // then allow maximize/fullscreen state.
    window->SetProperty(aura::client::kResizeBehaviorKey,
                        aura::client::kResizeBehaviorCanMaximize |
                            aura::client::kResizeBehaviorCanMinimize |
                            aura::client::kResizeBehaviorCanResize);
    return window;
  }

  std::unique_ptr<aura::Window> CreateNonSnappableWindow(
      const gfx::Rect& bounds) {
    return TestWindowBuilder()
        .SetWindowProperty(aura::client::kResizeBehaviorKey,
                           aura::client::kResizeBehaviorNone)
        .SetBounds(bounds)
        .SetDelegate(new SplitViewTestWindowDelegate)
        .Build();
  }

  bool IsDividerAnimating() {
    return split_view_controller()->IsDividerAnimating();
  }

  void SkipDividerSnapAnimation() {
    if (!IsDividerAnimating()) {
      return;
    }
    split_view_controller()->StopAndShoveAnimatedDivider();
    split_view_controller()->EndResizeWithDividerImpl();
    split_view_controller()->EndSplitViewAfterResizingAtEdgeIfAppropriate();
  }

  void EndSplitView() { split_view_controller()->EndSplitView(); }

  void LongPressOnOverivewButtonTray() {
    ui::GestureEvent event(0, 0, 0, base::TimeTicks(),
                           ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
    StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->overview_button_tray()
        ->OnGestureEvent(&event);
  }

  std::vector<aura::Window*> GetWindowsInOverviewGrids() {
    return Shell::Get()
        ->overview_controller()
        ->GetWindowsListInOverviewGridsForTest();
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

  SplitViewDivider* split_view_divider() {
    return split_view_controller()->split_view_divider();
  }

  int divider_position() { return split_view_controller()->divider_position(); }

  float divider_closest_ratio() {
    return split_view_controller()->divider_closest_ratio_;
  }

 protected:
  class SplitViewTestWindowDelegate : public aura::test::TestWindowDelegate {
   public:
    SplitViewTestWindowDelegate() = default;
    ~SplitViewTestWindowDelegate() override = default;

    // aura::test::TestWindowDelegate:
    void OnWindowDestroying(aura::Window* window) override { window->Hide(); }
    void OnWindowDestroyed(aura::Window* window) override { delete this; }
  };

  void CheckForDuplicateTraceName(const char* trace) {
    DCHECK(!base::Contains(trace_names_, trace)) << trace;
    trace_names_.push_back(trace);
  }

  void CheckOverviewEnterExitHistogram(const char* trace,
                                       std::vector<int>&& enter_counts,
                                       std::vector<int>&& exit_counts) {
    CheckForDuplicateTraceName(trace);

    // Overview histograms recorded via ui::ThroughputTracker is reported
    // on the next frame presented after animation stops. Wait for the next
    // frame with a 100ms timeout for the report, regardless of whether there
    // is a next frame.
    std::ignore = ui::WaitForNextFrameToBePresented(
        Shell::GetPrimaryRootWindow()->layer()->GetCompositor(),
        base::Milliseconds(100));

    {
      SCOPED_TRACE(trace + std::string(".Enter"));
      CheckOverviewHistogram("Ash.Overview.AnimationSmoothness.Enter",
                             std::move(enter_counts));
    }
    {
      SCOPED_TRACE(trace + std::string(".Exit"));
      CheckOverviewHistogram("Ash.Overview.AnimationSmoothness.Exit",
                             std::move(exit_counts));
    }
  }

  const base::HistogramTester& histograms() const { return histograms_; }

 private:
  void CheckOverviewHistogram(const char* histogram, std::vector<int> counts) {
    // These two events should never happen in this test.
    histograms_.ExpectTotalCount(histogram + std::string(".ClamshellMode"), 0);
    histograms_.ExpectTotalCount(
        histogram + std::string(".SingleClamshellMode"), 0);

    histograms_.ExpectTotalCount(histogram + std::string(".TabletMode"),
                                 counts[0]);
    histograms_.ExpectTotalCount(histogram + std::string(".SplitView"),
                                 counts[1]);
  }
  std::vector<std::string> trace_names_;
  base::HistogramTester histograms_;
};

class TestWindowStateDelegate : public WindowStateDelegate {
 public:
  TestWindowStateDelegate() = default;

  TestWindowStateDelegate(const TestWindowStateDelegate&) = delete;
  TestWindowStateDelegate& operator=(const TestWindowStateDelegate&) = delete;

  ~TestWindowStateDelegate() override = default;

  // WindowStateDelegate:
  std::unique_ptr<PresentationTimeRecorder> OnDragStarted(
      int component) override {
    drag_in_progress_ = true;
    return nullptr;
  }
  void OnDragFinished(bool cancel, const gfx::PointF& location) override {
    drag_in_progress_ = false;
  }

  bool drag_in_progress() { return drag_in_progress_; }

 private:
  bool drag_in_progress_ = false;
};

// Tests the basic functionalities.
TEST_F(SplitViewControllerTest, Basic) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kNoSnap);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_EQ(split_view_controller()->primary_window(), window1.get());
  EXPECT_NE(split_view_controller()->primary_window(), window2.get());
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(window1->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                SplitViewController::SnapPosition::kPrimary, window1.get()));

  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->secondary_window(), window2.get());
  EXPECT_NE(split_view_controller()->secondary_window(), window1.get());
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(window2->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                SplitViewController::SnapPosition::kSecondary, window2.get()));

  EndSplitView();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kNoSnap);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
}

// Tests that the default snapped window is the first window that gets snapped.
TEST_F(SplitViewControllerTest, DefaultSnappedWindow) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_EQ(window1.get(), split_view_controller()->GetDefaultSnappedWindow());

  EndSplitView();
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_EQ(window2.get(), split_view_controller()->GetDefaultSnappedWindow());
}

// Tests that if there are two snapped windows, closing one of them will open
// overview window grid on the closed window side of the screen. If there is
// only one snapped windows, closing the snapped window will end split view mode
// and adjust the overview window grid bounds if the overview mode is active at
// that moment.
TEST_F(SplitViewControllerTest, WindowCloseTest) {
  // 1 - First test one snapped window scenario.
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window0(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  split_view_controller()->SnapWindow(
      window0.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  // Closing this snapped window should exit split view mode.
  window0.reset();
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  // 2 - Then test two snapped windows scenario.
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::SnapPosition::kPrimary);

  // Closing one of the two snapped windows will not end split view mode.
  window1.reset();
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kSecondarySnapped);
  // Since left window was closed, its default snap position changed to RIGHT.
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::SnapPosition::kSecondary);
  // Window grid is showing no recent items, and has no windows, but it is still
  // available.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Now close the other snapped window.
  window2.reset();
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kNoSnap);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // 3 - Then test the scenario with more than two windows.
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window5(CreateWindow(bounds));
  split_view_controller()->SnapWindow(
      window3.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window4.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::SnapPosition::kPrimary);

  // Close one of the snapped windows.
  window4.reset();
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::SnapPosition::kPrimary);
  // Now overview window grid can be opened.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Close the other snapped window.
  window3.reset();
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kNoSnap);
  // Test the overview winow grid should still open.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that if there are two snapped windows, minimizing one of them will open
// overview window grid on the minimized window side of the screen. If there is
// only one snapped windows, minimizing the sanpped window will end split view
// mode and adjust the overview window grid bounds if the overview mode is
// active at that moment.
TEST_F(SplitViewControllerTest, MinimizeWindowTest) {
  const gfx::Rect bounds(0, 0, 400, 400);

  // 1 - First test one snapped window scenario.
  std::unique_ptr<aura::Window> window0(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  split_view_controller()->SnapWindow(
      window0.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  WMEvent minimize_event(WM_EVENT_MINIMIZE);
  WindowState::Get(window0.get())->OnWMEvent(&minimize_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  // 2 - Then test the scenario that has 2 or more windows.
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::SnapPosition::kPrimary);

  // Minimizing one of the two snapped windows will not end split view mode.
  WindowState::Get(window1.get())->OnWMEvent(&minimize_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kSecondarySnapped);
  // Since left window was minimized, its default snap position changed to
  // RIGHT.
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::SnapPosition::kSecondary);
  // The overview window grid will open.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Now minimize the other snapped window.
  WindowState::Get(window2.get())->OnWMEvent(&minimize_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kNoSnap);
  // The overview window grid is still open.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that if one of the snapped window gets maximized / full-screened, the
// split view mode ends.
TEST_F(SplitViewControllerTest, WindowStateChangeTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  // 1 - First test one snapped window scenario.
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);

  WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  WindowState::Get(window1.get())->OnWMEvent(&maximize_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);

  WMEvent fullscreen_event(WM_EVENT_FULLSCREEN);
  WindowState::Get(window1.get())->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  // 2 - Then test two snapped window scenario.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  // Reactivate |window1| because it is the one that we will be maximizing and
  // fullscreening. When |window1| goes out of scope at the end of the test, it
  // will be a full screen window, and if it is not the active window, then the
  // destructor will cause a |DCHECK| failure in |ash::WindowState::Get|.
  wm::ActivateWindow(window1.get());
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);

  // Maximize one of the snapped window will end the split view mode.
  WindowState::Get(window1.get())->OnWMEvent(&maximize_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);

  // Full-screen one of the snapped window will also end the split view mode.
  WindowState::Get(window1.get())->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  // 3 - Test the scenario that part of the screen is a snapped window and part
  // of the screen is the overview window grid.
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  ToggleOverview();
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);

  // Maximize the snapped window will end the split view mode and overview mode.
  WindowState::Get(window1.get())->OnWMEvent(&maximize_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  ToggleOverview();
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);

  // Fullscreen the snapped window will end the split view mode and overview
  // mode.
  WindowState::Get(window1.get())->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that if split view mode is active, activate another window will snap
// the window to the non-default side of the screen.
TEST_F(SplitViewControllerTest, WindowActivationTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->primary_window(), window1.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);

  wm::ActivateWindow(window2.get());
  EXPECT_EQ(split_view_controller()->secondary_window(), window2.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);

  wm::ActivateWindow(window3.get());
  EXPECT_EQ(split_view_controller()->secondary_window(), window3.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
}

// Test that if the overview mode is active in clamshell mode, the window with
// |kUnresizableSnappedSizeKey| property can be snapped with size constraints.
TEST_F(SplitViewControllerTest, SnapWindowWithUnresizableSnapProperty) {
  UpdateDisplay("800x600");
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorNone);
  window->SetProperty(kUnresizableSnappedSizeKey, new gfx::Size(300, 0));

  // Switch to clamshell mode and enter overview mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  ToggleOverview();
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  split_view_controller()->SnapWindow(
      window.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_EQ(window->GetBoundsInScreen().width(), 300);
}

// Tests that if split view mode and overview mode are active at the same time,
// i.e., half of the screen is occupied by a snapped window and half of the
// screen is occupied by the overview windows grid, the next activatable window
// will be picked to snap when exiting the overview mode.
// TODO(crbug.com/1312252): Re-enable this test
TEST_F(SplitViewControllerTest, DISABLED_ExitOverviewTest) {
  ui::ScopedAnimationDurationScaleMode anmatin_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  ToggleOverview();
  WaitForOverviewEnterAnimation();
  CheckOverviewEnterExitHistogram("EnterInTablet", {1, 0}, {0, 0});

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_EQ(split_view_controller()->primary_window(), window1.get());
  // Activate |window1| in preparation to verify that it stays active when
  // overview mode is ended.
  wm::ActivateWindow(window1.get());

  ToggleOverview();
  WaitForOverviewExitAnimation();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->secondary_window(), window3.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  CheckOverviewEnterExitHistogram("ExitInSplitView", {1, 0}, {0, 1});
}

// Tests that in split view with a single overview window, when overview is
// ended, the wallpaper stays blurred until the window finishes animating.
TEST_F(SplitViewControllerTest,
       WallpaperUnblurredAfterLoneOverviewWindowSnapAnimationCompleted) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  ToggleOverview();
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);

  WallpaperWidgetController* wallpaper_widget_controller =
      Shell::GetPrimaryRootWindowController()->wallpaper_widget_controller();
  EXPECT_GT(wallpaper_widget_controller->GetWallpaperBlur(), 0);
  EXPECT_FALSE(wallpaper_widget_controller->IsAnimating());

  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ToggleOverview();
  EXPECT_GT(wallpaper_widget_controller->GetWallpaperBlur(), 0);
  EXPECT_FALSE(wallpaper_widget_controller->IsAnimating());

  WaitForOverviewExitAnimation();
  // The wallpaper is unblurred without animation, because the wallpaper is
  // covered by the windows and the split view divider.
  EXPECT_EQ(wallpaper_widget_controller->GetWallpaperBlur(), 0);
  EXPECT_FALSE(wallpaper_widget_controller->IsAnimating());
}

// Tests that if split view mode is active when entering overview, the overview
// windows grid should show in the non-default side of the screen, and the
// default snapped window should not be shown in the overview window grid.
// TODO(crbug.com/1335854): Deflake this test.
TEST_F(SplitViewControllerTest, DISABLED_EnterOverviewModeTest) {
  ui::ScopedAnimationDurationScaleMode anmatin_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->GetDefaultSnappedWindow(), window1.get());

  ToggleOverview();
  WaitForOverviewEnterAnimation();
  CheckOverviewEnterExitHistogram("EnterInSplitView", {0, 1}, {0, 0});
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_FALSE(
      base::Contains(GetWindowsInOverviewGrids(),
                     split_view_controller()->GetDefaultSnappedWindow()));

  ToggleOverview();
  WaitForOverviewExitAnimation();
  CheckOverviewEnterExitHistogram("ExitInSplitView", {0, 1}, {0, 1});
}

// Tests that the split divider was created when the split view mode is active
// and destroyed when the split view mode is ended. The split divider should be
// always above the two snapped windows.
TEST_F(SplitViewControllerTest, SplitDividerBasicTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  EXPECT_TRUE(!split_view_divider());
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_divider());
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_TRUE(split_view_divider());
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  EXPECT_TRUE(IsStackedBelow(
      window1.get(),
      split_view_divider()->divider_widget()->GetNativeWindow()));
  EXPECT_TRUE(IsStackedBelow(
      window2.get(),
      split_view_divider()->divider_widget()->GetNativeWindow()));

  // Test that activating an non-snappable window ends the split view mode.
  std::unique_ptr<aura::Window> window3(CreateNonSnappableWindow(bounds));
  wm::ActivateWindow(window3.get());
  EXPECT_FALSE(split_view_divider());
}

// Tests that the split divider has the correct state when the dragged overview
// item is destroyed.
TEST_F(SplitViewControllerTest, DividerStateWhenDraggedOverviewItemDestroyed) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());

  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  OverviewItem* overview_item =
      overview_session->GetOverviewItemForWindow(window2.get());
  gfx::PointF drag_point = overview_item->target_bounds().CenterPoint();
  overview_session->InitiateDrag(overview_item, drag_point,
                                 /*is_touch_dragging=*/false);
  drag_point.Offset(5.f, 0.f);
  overview_session->Drag(overview_item, drag_point);
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());

  window2.reset();
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());

  // The split view divider should always be on top of the two snapped windows.
  split_view_controller()->SnapWindow(
      window3.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  EXPECT_TRUE(IsStackedBelow(window1.get(), window3.get()));
  EXPECT_TRUE(IsStackedBelow(
      window3.get(),
      split_view_divider()->divider_widget()->GetNativeWindow()));
}

// Tests that the split divider has the correct state when the drag of the
// overview item is cancelled.
TEST_F(SplitViewControllerTest, DividerStateWhenOverviewItemDragCancelled) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());

  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  OverviewItem* overview_item =
      overview_session->GetOverviewItemForWindow(window2.get());
  gfx::PointF drag_point = overview_item->target_bounds().CenterPoint();
  overview_session->InitiateDrag(overview_item, drag_point,
                                 /*is_touch_dragging=*/false);
  drag_point.Offset(5.f, 0.f);
  overview_session->Drag(overview_item, drag_point);
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());

  // If the drag is canceled, the divider should be placed on top of the snapped
  // window.
  overview_session->ResetDraggedWindowGesture();
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());

  split_view_controller()->SnapWindow(
      window3.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  wm::ActivateWindow(window3.get());
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  EXPECT_TRUE(IsStackedBelow(
      window1.get(),
      split_view_divider()->divider_widget()->GetNativeWindow()));
  EXPECT_TRUE(IsStackedBelow(
      window3.get(),
      split_view_divider()->divider_widget()->GetNativeWindow()));
}

// Verifys that the bounds of the two windows in splitview are as expected.
TEST_F(SplitViewControllerTest, SplitDividerWindowBounds) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  ASSERT_TRUE(split_view_divider());

  // Verify with two freshly snapped windows are roughly the same width (off by
  // one pixel at most due to the display maybe being even and the divider being
  // a fixed odd pixel width).
  int window1_width = window1->GetBoundsInScreen().width();
  int window2_width = window2->GetBoundsInScreen().width();
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParent(window1.get()).width();
  EXPECT_NEAR(window1_width, window2_width, 1);
  EXPECT_EQ(screen_width,
            window1_width + divider_bounds.width() + window2_width);

  // Drag the divider to a position two thirds of the screen size. Verify window
  // 1 is wider than window 2.
  GetEventGenerator()->set_current_screen_location(
      divider_bounds.CenterPoint());
  GetEventGenerator()->DragMouseTo(screen_width * 0.67f, 0);
  SkipDividerSnapAnimation();
  window1_width = window1->GetBoundsInScreen().width();
  window2_width = window2->GetBoundsInScreen().width();
  const int old_window1_width = window1_width;
  const int old_window2_width = window2_width;
  EXPECT_GT(window1_width, 2 * window2_width);
  EXPECT_EQ(screen_width,
            window1_width + divider_bounds.width() + window2_width);

  // Drag the divider to a position close to two thirds of the screen size.
  // Verify the divider snaps to two thirds of the screen size, and the windows
  // remain the same size as previously.
  divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  GetEventGenerator()->set_current_screen_location(
      divider_bounds.CenterPoint());
  GetEventGenerator()->DragMouseTo(screen_width * 0.7f, 0);
  SkipDividerSnapAnimation();
  window1_width = window1->GetBoundsInScreen().width();
  window2_width = window2->GetBoundsInScreen().width();
  EXPECT_EQ(window1_width, old_window1_width);
  EXPECT_EQ(window2_width, old_window2_width);

  // Drag the divider to a position one third of the screen size. Verify window
  // 1 is wider than window 2.
  divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  GetEventGenerator()->set_current_screen_location(
      divider_bounds.CenterPoint());
  GetEventGenerator()->DragMouseTo(screen_width * 0.33f, 0);
  SkipDividerSnapAnimation();
  window1_width = window1->GetBoundsInScreen().width();
  window2_width = window2->GetBoundsInScreen().width();
  EXPECT_GT(window2_width, 2 * window1_width);
  EXPECT_EQ(screen_width,
            window1_width + divider_bounds.width() + window2_width);

  // Verify that the left window from dragging the divider to two thirds of the
  // screen size is roughly the same size as the right window after dragging the
  // divider to one third of the screen size, and vice versa.
  EXPECT_NEAR(window1_width, old_window2_width, 1);
  EXPECT_NEAR(window2_width, old_window1_width, 1);
}

// Verify that disconnecting a display which has a snapped window in it in
// tablet mode won't lead to a crash. Regression test for
// https://crbug.com/1316230.
TEST_F(SplitViewControllerTest,
       DisplayDisconnectionWithSnappedWindowInTabletMode) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  UpdateDisplay("800x600,800x600");

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(EnterOverview());

  // Turn off the display mirror mode.
  Shell::Get()->display_manager()->SetMirrorMode(display::MirrorMode::kOff,
                                                 absl::nullopt);

  std::unique_ptr<aura::Window> w1(
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateTestWindowInShellWithBounds(gfx::Rect(900, 0, 100, 100)));
  ASSERT_NE(w1->GetRootWindow(), w2->GetRootWindow());

  // Snap the window on the second display.
  auto* split_view_controller_on_display2 =
      SplitViewController::Get(w2->GetRootWindow());
  split_view_controller_on_display2->SnapWindow(
      w2.get(), SplitViewController::SnapPosition::kPrimary);
  ASSERT_TRUE(split_view_controller_on_display2->split_view_divider());

  // Now disconnect the second display, verify there's no crash.
  UpdateDisplay("800x600");
  base::RunLoop().RunUntilIdle();
}

// Verify that disconnecting a display while dragging the split view divider in
// it in tablet mode won't lead to a crash. Regression test for
// https://crbug.com/1316892.
TEST_F(SplitViewControllerTest,
       DisplayDisconnectionWhileDraggingSplitDividerInTabletMode) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  UpdateDisplay("800x600,800x600");

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(EnterOverview());

  // Turn off the display mirror mode.
  Shell::Get()->display_manager()->SetMirrorMode(display::MirrorMode::kOff,
                                                 absl::nullopt);

  // Create a window on the secondary display.
  std::unique_ptr<aura::Window> w(
      CreateTestWindowInShellWithBounds(gfx::Rect(900, 0, 100, 100)));

  // Snap the window on the second display.
  auto* split_view_controller = SplitViewController::Get(w->GetRootWindow());
  split_view_controller->SnapWindow(
      w.get(), SplitViewController::SnapPosition::kPrimary);
  auto* split_view_divider = split_view_controller->split_view_divider();
  ASSERT_TRUE(split_view_divider);

  auto* event_generator = GetEventGenerator();
  const gfx::Point divider_center_pointer =
      split_view_divider->GetDividerBoundsInScreen(/*is_dragging=*/false)
          .CenterPoint();
  event_generator->PressTouch(divider_center_pointer);

  // Drag the split view divider without releasing the drag.
  const gfx::Vector2d delta(100, 0);
  event_generator->MoveTouch(divider_center_pointer + delta);

  // Now disconnect the second display, verify there's no crash.
  UpdateDisplay("800x600");
  base::RunLoop().RunUntilIdle();
}

// Tests that the bounds of the snapped windows and divider are adjusted when
// the screen display configuration changes.
TEST_F(SplitViewControllerTest, DisplayConfigurationChangeTest) {
  UpdateDisplay("407x400");
  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);

  const gfx::Rect bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1| and |window2| has the same width and height after snap.
  EXPECT_NEAR(bounds_window1.width(), bounds_window2.width(), 1);
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());
  EXPECT_EQ(bounds_divider.height(), bounds_window1.height());

  // Test that |window1|, divider, |window2| are aligned properly.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());

  // Now change the display configuration.
  UpdateDisplay("507x500");
  const gfx::Rect new_bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect new_bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect new_bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that the new bounds are different with the old ones.
  EXPECT_NE(bounds_window1, new_bounds_window1);
  EXPECT_NE(bounds_window2, new_bounds_window2);
  EXPECT_NE(bounds_divider, new_bounds_divider);

  // Test that |window1|, divider, |window2| are still aligned properly.
  EXPECT_EQ(new_bounds_divider.x(),
            new_bounds_window1.x() + new_bounds_window1.width());
  EXPECT_EQ(new_bounds_window2.x(),
            new_bounds_divider.x() + new_bounds_divider.width());
}

// Tests that the bounds of the snapped windows and divider are adjusted when
// the internal screen display configuration changes.
TEST_F(SplitViewControllerTest, InternalDisplayConfigurationChangeTest) {
  UpdateDisplay("407x400");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);

  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);

  const gfx::Rect bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1| and |window2| has the same width and height after snap.
  EXPECT_NEAR(bounds_window1.width(), bounds_window2.width(), 1);
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());
  EXPECT_EQ(bounds_divider.height(), bounds_window1.height());

  // Test that |window1|, divider, |window2| are aligned properly.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());

  // Now change the display configuration.
  UpdateDisplay("507x500");
  const gfx::Rect new_bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect new_bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect new_bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that the new bounds are different with the old ones.
  EXPECT_NE(bounds_window1, new_bounds_window1);
  EXPECT_NE(bounds_window2, new_bounds_window2);
  EXPECT_NE(bounds_divider, new_bounds_divider);

  // Test that |window1|, divider, |window2| are still aligned properly.
  EXPECT_EQ(new_bounds_divider.x(),
            new_bounds_window1.x() + new_bounds_window1.width());
  EXPECT_EQ(new_bounds_window2.x(),
            new_bounds_divider.x() + new_bounds_divider.width());
}

// Test that if the internal screen display configuration changes during the
// divider snap animation, then this animation stops, and the bounds of the
// snapped windows and divider are adjusted as normal.
TEST_F(SplitViewControllerTest,
       InternalDisplayConfigurationChangeDuringDividerSnap) {
  UpdateDisplay("407x400");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);

  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);

  const gfx::Rect bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1| and |window2| has the same width and height after snap.
  EXPECT_NEAR(bounds_window1.width(), bounds_window2.width(), 1);
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());
  EXPECT_EQ(bounds_divider.height(), bounds_window1.height());

  // Test that |window1|, divider, |window2| are aligned properly.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());

  // Drag the divider to trigger the snap animation.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->set_current_screen_location(divider_center);
  GetEventGenerator()->DragMouseBy(20, 0);
  ASSERT_TRUE(IsDividerAnimating());
  const gfx::Rect animation_start_bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect animation_start_bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect animation_start_bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Change the display configuration and check that the snap animation stops.
  UpdateDisplay("507x500");
  EXPECT_FALSE(IsDividerAnimating());
  const gfx::Rect new_bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect new_bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect new_bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that the new bounds are different with the old ones.
  EXPECT_NE(bounds_window1, new_bounds_window1);
  EXPECT_NE(bounds_window2, new_bounds_window2);
  EXPECT_NE(bounds_divider, new_bounds_divider);

  // Test that the new bounds are also different with the ones from the start of
  // the divider snap animation.
  EXPECT_NE(bounds_window1, animation_start_bounds_window1);
  EXPECT_NE(bounds_window2, animation_start_bounds_window2);
  EXPECT_NE(bounds_divider, animation_start_bounds_divider);

  // Test that |window1|, divider, |window2| are still aligned properly.
  EXPECT_EQ(new_bounds_divider.x(),
            new_bounds_window1.x() + new_bounds_window1.width());
  EXPECT_EQ(new_bounds_window2.x(),
            new_bounds_divider.x() + new_bounds_divider.width());
}

// Test that if the internal screen display configuration changes during the
// divider snap animation, and if the adjusted divider bounds place it at the
// left edge of the screen, then split view ends.
TEST_F(SplitViewControllerTest,
       InternalDisplayConfigurationChangeDuringDividerSnapToLeft) {
  UpdateDisplay("407x400");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);

  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);

  const gfx::Rect bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1| and |window2| has the same width and height after snap.
  EXPECT_NEAR(bounds_window1.width(), bounds_window2.width(), 1);
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());
  EXPECT_EQ(bounds_divider.height(), bounds_window1.height());

  // Test that |window1|, divider, |window2| are aligned properly.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());

  // Drag the divider to end split view pending the snap animation.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->set_current_screen_location(divider_center);
  GetEventGenerator()->DragMouseBy(20 - bounds_window1.width(), 0);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  ASSERT_TRUE(IsDividerAnimating());

  // Change the display configuration and check that split view ends.
  UpdateDisplay("507x500");
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Test that if the internal screen display configuration changes during the
// divider snap animation, and if the adjusted divider bounds place it at the
// right edge of the screen, then split view ends.
TEST_F(SplitViewControllerTest,
       InternalDisplayConfigurationChangeDuringDividerSnapToRight) {
  UpdateDisplay("407x400");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);

  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);

  const gfx::Rect bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1| and |window2| has the same width and height after snap.
  EXPECT_NEAR(bounds_window1.width(), bounds_window2.width(), 1);
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());
  EXPECT_EQ(bounds_divider.height(), bounds_window1.height());

  // Test that |window1|, divider, |window2| are aligned properly.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());

  // Drag the divider to end split view pending the snap animation.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->set_current_screen_location(divider_center);
  GetEventGenerator()->DragMouseBy(bounds_window2.width() - 20, 0);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  ASSERT_TRUE(IsDividerAnimating());

  // Change the display configuration and check that split view ends.
  UpdateDisplay("507x500");
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Verify the left and right windows get swapped when SwapWindows is called or
// the divider is double clicked.
TEST_F(SplitViewControllerTest, SwapWindows) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  ASSERT_EQ(split_view_controller()->primary_window(), window1.get());
  ASSERT_EQ(split_view_controller()->secondary_window(), window2.get());

  gfx::Rect left_bounds = window1->GetBoundsInScreen();
  gfx::Rect right_bounds = window2->GetBoundsInScreen();

  // Verify that after swapping windows, the windows and their bounds have been
  // swapped.
  split_view_controller()->SwapWindows(
      SplitViewController::SwapWindowsSource::kDoubleTap);
  EXPECT_EQ(split_view_controller()->primary_window(), window2.get());
  EXPECT_EQ(split_view_controller()->secondary_window(), window1.get());
  EXPECT_EQ(left_bounds, window2->GetBoundsInScreen());
  EXPECT_EQ(right_bounds, window1->GetBoundsInScreen());

  // End split view mode and snap the window to RIGHT first, verify the function
  // SwapWindows() still works properly.
  EndSplitView();
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kSecondary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kPrimary);
  ASSERT_EQ(split_view_controller()->secondary_window(), window1.get());
  ASSERT_EQ(split_view_controller()->primary_window(), window2.get());

  left_bounds = window2->GetBoundsInScreen();
  right_bounds = window1->GetBoundsInScreen();

  split_view_controller()->SwapWindows(
      SplitViewController::SwapWindowsSource::kDoubleTap);
  EXPECT_EQ(split_view_controller()->primary_window(), window1.get());
  EXPECT_EQ(split_view_controller()->secondary_window(), window2.get());
  EXPECT_EQ(left_bounds, window1->GetBoundsInScreen());
  EXPECT_EQ(right_bounds, window2->GetBoundsInScreen());

  // Perform a double click on the divider center.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->set_current_screen_location(divider_center);
  GetEventGenerator()->DoubleClickLeftButton();

  EXPECT_EQ(split_view_controller()->primary_window(), window2.get());
  EXPECT_EQ(split_view_controller()->secondary_window(), window1.get());
  EXPECT_EQ(left_bounds, window2->GetBoundsInScreen());
  EXPECT_EQ(right_bounds, window1->GetBoundsInScreen());
}

// Verify the left and right windows get swapped when the divider is double
// tapped. SwapWindows() contains a long code comment that shows it is worth
// having separate tests for double clicking and double tapping the divider.
TEST_F(SplitViewControllerTest, DoubleTapDivider) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  ASSERT_EQ(split_view_controller()->primary_window(), window1.get());
  ASSERT_EQ(split_view_controller()->secondary_window(), window2.get());

  gfx::Rect left_bounds = window1->GetBoundsInScreen();
  gfx::Rect right_bounds = window2->GetBoundsInScreen();

  // Perform a double tap on the divider center.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->GestureTapAt(divider_center);
  GetEventGenerator()->GestureTapAt(divider_center);

  EXPECT_EQ(split_view_controller()->primary_window(), window2.get());
  EXPECT_EQ(split_view_controller()->secondary_window(), window1.get());
  EXPECT_EQ(left_bounds, window2->GetBoundsInScreen());
  EXPECT_EQ(right_bounds, window1->GetBoundsInScreen());
}

// Verify the left and right windows do not get swapped when the divider is
// dragged and double clicked.
TEST_F(SplitViewControllerTest, DragAndDoubleClickDivider) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  ASSERT_EQ(split_view_controller()->primary_window(), window1.get());
  ASSERT_EQ(split_view_controller()->secondary_window(), window2.get());

  // Drag the divider and double click it before the snap animation moves it.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->set_current_screen_location(divider_center);
  GetEventGenerator()->DragMouseBy(20, 0);
  GetEventGenerator()->DoubleClickLeftButton();

  EXPECT_EQ(split_view_controller()->primary_window(), window1.get());
  EXPECT_EQ(split_view_controller()->secondary_window(), window2.get());
}

// Verify the left and right windows do not get swapped when the divider is
// dragged and double tapped.
TEST_F(SplitViewControllerTest, DragAndDoubleTapDivider) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  ASSERT_EQ(split_view_controller()->primary_window(), window1.get());
  ASSERT_EQ(split_view_controller()->secondary_window(), window2.get());

  // Drag the divider and double tap it before the snap animation moves it.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->set_current_screen_location(divider_center);
  const gfx::Point drag_destination = divider_center + gfx::Vector2d(20, 0);
  GetEventGenerator()->DragMouseTo(drag_destination);
  GetEventGenerator()->GestureTapAt(drag_destination);
  GetEventGenerator()->GestureTapAt(drag_destination);

  EXPECT_EQ(split_view_controller()->primary_window(), window1.get());
  EXPECT_EQ(split_view_controller()->secondary_window(), window2.get());
}

// Verify overview does not steal focus from a split view window when trading
// places with it.
TEST_F(SplitViewControllerTest, OverviewNotStealFocusOnSwapWindows) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  ToggleOverview();
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kPrimary);
  wm::ActivateWindow(window2.get());
  split_view_controller()->SwapWindows(
      SplitViewController::SwapWindowsSource::kDoubleTap);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

class SplitViewControllerFloatTest : public SplitViewControllerTest {
 public:
  SplitViewControllerFloatTest() = default;
  SplitViewControllerFloatTest(const SplitViewControllerFloatTest&) = delete;
  SplitViewControllerFloatTest& operator=(const SplitViewControllerFloatTest&) =
      delete;
  ~SplitViewControllerFloatTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      chromeos::wm::features::kWindowLayoutMenu};
};

// Tests that the floated window is not auto-snapped if it's on top of two
// snapped windows. It should only get snapped if it's activated from overview.
TEST_F(SplitViewControllerFloatTest, DontAutosnapFloatedWindow) {
  // Create 2 normal windows and 1 floated window.
  std::unique_ptr<aura::Window> window1(CreateAppWindow());
  std::unique_ptr<aura::Window> window2(CreateAppWindow());
  std::unique_ptr<aura::Window> floated_window(CreateAppWindow());
  Shell::Get()->float_controller()->ToggleFloat(floated_window.get());
  ASSERT_TRUE(WindowState::Get(floated_window.get())->IsFloated());

  // Snap `window1` so that Overview is open.
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  auto* overview_controller = Shell::Get()->overview_controller();
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  auto* overview_session = overview_controller->overview_session();
  ASSERT_TRUE(overview_session->IsWindowInOverview(window2.get()));
  ASSERT_TRUE(overview_session->IsWindowInOverview(floated_window.get()));

  // Activate `window2` from Overview. Test that it gets snapped in splitview,
  // and `floated_window` remains floated.
  wm::ActivateWindow(window2.get());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));
  wm::ActivateWindow(floated_window.get());
  EXPECT_FALSE(
      split_view_controller()->IsWindowInSplitView(floated_window.get()));
  EXPECT_TRUE(WindowState::Get(floated_window.get())->IsFloated());

  // Snap `window1` again, then activate `floated_window` from Overview. Test
  // that it gets snapped in splitview.
  EndSplitView();
  EXPECT_TRUE(WindowState::Get(floated_window.get())->IsFloated());
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  overview_session = overview_controller->overview_session();
  EXPECT_TRUE(overview_session->IsWindowInOverview(floated_window.get()));
  wm::ActivateWindow(floated_window.get());
  EXPECT_TRUE(
      split_view_controller()->IsWindowInSplitView(floated_window.get()));
  EXPECT_FALSE(WindowState::Get(floated_window.get())->IsFloated());
}

// Verify that you cannot start dragging the divider during its snap animation.
TEST_F(SplitViewControllerTest, StartDraggingDividerDuringSnapAnimation) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  ASSERT_EQ(split_view_controller()->primary_window(), window1.get());
  ASSERT_EQ(split_view_controller()->secondary_window(), window2.get());

  // Drag the divider and then try to start dragging it again without waiting
  // for the snap animation.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->set_current_screen_location(divider_center);
  GetEventGenerator()->DragMouseBy(20, 0);
  GetEventGenerator()->PressLeftButton();
  EXPECT_FALSE(split_view_controller()->is_resizing_with_divider());
  GetEventGenerator()->ReleaseLeftButton();
}

TEST_F(SplitViewControllerTest, LongPressEntersSplitView) {
  // Tests that with no active windows, split view does not get activated.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  wm::ActivateWindow(window1.get());

  // Tests that with split view gets activated with an active window.
  LongPressOnOverivewButtonTray();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
}

// Verify that when in split view mode with either one snapped or two snapped
// windows, split view mode gets exited when the overview button gets a long
// press event.
TEST_F(SplitViewControllerTest, LongPressExitsSplitView) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  // Snap |window1| to the left.
  ToggleOverview();
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());

  // Verify that by long pressing on the overview button tray with left snapped
  // window, split view mode gets exited and the left window (|window1|) is the
  // current active window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(window1.get(), window_util::GetActiveWindow());

  // Snap |window1| to the right.
  ToggleOverview();
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kSecondary);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());

  // Verify that by long pressing on the overview button tray with right snapped
  // window, split view mode gets exited and the right window (|window1|) is the
  // current active window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(window1.get(), window_util::GetActiveWindow());

  // Snap two windows and activate the left window, |window1|.
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  wm::ActivateWindow(window1.get());
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());

  // Verify that by long pressing on the overview button tray with two snapped
  // windows, split view mode gets exited.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(window1.get(), window_util::GetActiveWindow());

  // Snap two windows and activate the right window, |window2|.
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  wm::ActivateWindow(window2.get());
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());

  // Verify that by long pressing on the overview button tray with two snapped
  // windows, split view mode gets exited, and the activated window in splitview
  // is the current active window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(window2.get(), window_util::GetActiveWindow());
}

// Verify that if a window with a transient child which is not snappable is
// activated, and the the overview tray is long pressed, we will enter splitview
// with the transient parent snapped.
TEST_F(SplitViewControllerTest, LongPressEntersSplitViewWithTransientChild) {
  // Add two windows with one being a transient child of the first.
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> parent(CreateWindow(bounds));
  std::unique_ptr<aura::Window> child(
      CreateWindow(bounds, aura::client::WINDOW_TYPE_POPUP));
  wm::AddTransientChild(parent.get(), child.get());
  wm::ActivateWindow(parent.get());
  wm::ActivateWindow(child.get());

  // Verify that long press will snap the focused transient child's parent.
  LongPressOnOverivewButtonTray();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->GetDefaultSnappedWindow(), parent.get());
}

TEST_F(SplitViewControllerTest, LongPressExitsSplitViewWithTransientChild) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> left_window(CreateWindow(bounds));
  std::unique_ptr<aura::Window> right_window(CreateWindow(bounds));
  wm::ActivateWindow(left_window.get());
  wm::ActivateWindow(right_window.get());

  ToggleOverview();
  split_view_controller()->SnapWindow(
      left_window.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      right_window.get(), SplitViewController::SnapPosition::kSecondary);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());

  // Add a transient child to |right_window|, and activate it.
  aura::Window* transient_child =
      aura::test::CreateTestWindowWithId(0, right_window.get());
  ::wm::AddTransientChild(right_window.get(), transient_child);
  wm::ActivateWindow(transient_child);

  // Verify that by long pressing on the overview button tray, split view mode
  // gets exited and the window which contained |transient_child| is the
  // current active window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(right_window.get(), window_util::GetActiveWindow());
}

#if defined(NDEBUG)
#define MAYBE_LongPressInOverviewMode LongPressInOverviewMode
#else
// TODO(b/264848385): Flaky on dbg configuration
#define MAYBE_LongPressInOverviewMode DISABLED_LongPressInOverviewMode
#endif
// Verify that split view mode get activated when long pressing on the overview
// button while in overview mode iff we have at least one window.
TEST_F(SplitViewControllerTest, MAYBE_LongPressInOverviewMode) {
  ui::ScopedAnimationDurationScaleMode anmatin_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  ToggleOverview();
  WaitForOverviewEnterAnimation();
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  ASSERT_FALSE(split_view_controller()->InSplitViewMode());
  CheckOverviewEnterExitHistogram("EnterInTablet", {0, 0}, {0, 0});

  // Nothing happens if there are no windows.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Verify that with a window, a long press on the overview button tray will
  // enter splitview.
  std::unique_ptr<aura::Window> window(CreateWindow(gfx::Rect(0, 0, 400, 400)));
  wm::ActivateWindow(window.get());
  CheckOverviewEnterExitHistogram("ExitByActivation", {0, 0}, {0, 0});

  ToggleOverview();
  WaitForOverviewEnterAnimation();
  CheckOverviewEnterExitHistogram("EnterInTablet2", {1, 0}, {0, 0});
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  ASSERT_FALSE(split_view_controller()->InSplitViewMode());

  LongPressOnOverivewButtonTray();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(window.get(), split_view_controller()->primary_window());
  // This scenario should not trigger animation.
  CheckOverviewEnterExitHistogram("NoTransition", {1, 0}, {0, 0});
}

TEST_F(SplitViewControllerTest, LongPressWithUnsnappableWindow) {
  // Add an unsnappable window and a regular window.
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> unsnappable_window(
      CreateNonSnappableWindow(bounds));
  ASSERT_FALSE(split_view_controller()->InSplitViewMode());
  std::unique_ptr<aura::Window> regular_window(CreateWindow(bounds));
  wm::ActivateWindow(regular_window.get());
  wm::ActivateWindow(unsnappable_window.get());
  ASSERT_EQ(unsnappable_window.get(), window_util::GetActiveWindow());

  // Verify split view is not activated when long press occurs when active
  // window is unsnappable.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // Verify split view is not activated when long press occurs in overview mode
  // and the most recent window is unsnappable.
  ToggleOverview();
  ASSERT_TRUE(Shell::Get()
                  ->mru_window_tracker()
                  ->BuildWindowForCycleList(kActiveDesk)
                  .size() > 0);
  ASSERT_EQ(unsnappable_window.get(),
            Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(
                kActiveDesk)[0]);
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Tests that long press works even if the window is minimized.
TEST_F(SplitViewControllerTest, LongPressWithMinimizedWindow) {
  std::unique_ptr<aura::Window> window(CreateWindow(gfx::Rect(400, 400)));
  WindowState::Get(window.get())->Minimize();

  LongPressOnOverivewButtonTray();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
}

// Test the rotation functionalities in split view mode.
TEST_F(SplitViewControllerTest, RotationTest) {
  UpdateDisplay("807x407");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  // Set the screen orientation to LANDSCAPE_PRIMARY.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);

  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  gfx::Rect bounds_window1 = window1->GetBoundsInScreen();
  gfx::Rect bounds_window2 = window2->GetBoundsInScreen();
  gfx::Rect bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test |window1|, divider and |window2| are aligned horizontally.
  // |window1| is on the left, then the divider, and then |window2|.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());
  EXPECT_EQ(bounds_window1.height(), bounds_divider.height());
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());

  // Rotate the screen by 270 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);

  bounds_window1 = window1->GetBoundsInScreen();
  bounds_window2 = window2->GetBoundsInScreen();
  bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1|, divider, |window2| are now aligned vertically.
  // |window1| is on the top, then the divider, and then |window2|.
  EXPECT_EQ(bounds_divider.y(), bounds_window1.y() + bounds_window1.height());
  EXPECT_EQ(bounds_window2.y(), bounds_divider.y() + bounds_divider.height());
  EXPECT_EQ(bounds_window1.width(), bounds_divider.width());
  EXPECT_EQ(bounds_window1.width(), bounds_window2.width());

  // Rotate the screen by 180 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapeSecondary);

  bounds_window1 = window1->GetBoundsInScreen();
  bounds_window2 = window2->GetBoundsInScreen();
  bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1|, divider, |window2| are now aligned horizontally.
  // |window2| is on the left, then the divider, and then |window1|.
  EXPECT_EQ(bounds_divider.x(), bounds_window2.x() + bounds_window2.width());
  EXPECT_EQ(bounds_window1.x(), bounds_divider.x() + bounds_divider.width());
  EXPECT_EQ(bounds_window1.height(), bounds_divider.height());
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());

  // Rotate the screen by 90 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitSecondary);
  bounds_window1 = window1->GetBoundsInScreen();
  bounds_window2 = window2->GetBoundsInScreen();
  bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1|, divider, |window2| are now aligned vertically.
  // |window2| is on the top, then the divider, and then |window1|.
  EXPECT_EQ(bounds_divider.y(), bounds_window2.y() + bounds_window2.height());
  EXPECT_EQ(bounds_window1.y(), bounds_divider.y() + bounds_divider.height());
  EXPECT_EQ(bounds_window1.width(), bounds_divider.width());
  EXPECT_EQ(bounds_window1.width(), bounds_window2.width());

  // Rotate the screen back to 0 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);
  bounds_window1 = window1->GetBoundsInScreen();
  bounds_window2 = window2->GetBoundsInScreen();
  bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test |window1|, divider and |window2| are aligned horizontally.
  // |window1| is on the left, then the divider, and then |window2|.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());
  EXPECT_EQ(bounds_window1.height(), bounds_divider.height());
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());
}

// Test that if the split view mode is active when exiting tablet mode, we
// should also end split view mode.
TEST_F(SplitViewControllerTest, ExitTabletModeEndSplitView) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Test that |SplitViewController::CanSnapWindow| checks that the minimum size
// of the window fits into the left or top, with the default divider position.
// (If the work area length is odd, then the right or bottom will be one pixel
// larger.)
TEST_F(SplitViewControllerTest, SnapWindowWithMinimumSizeTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(window1.get()));

  UpdateDisplay("800x600");
  aura::test::TestWindowDelegate* delegate =
      static_cast<aura::test::TestWindowDelegate*>(window1->delegate());
  delegate->set_minimum_size(gfx::Size(396, 0));
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(window1.get()));
  delegate->set_minimum_size(gfx::Size(397, 0));
  EXPECT_FALSE(split_view_controller()->CanSnapWindow(window1.get()));

  UpdateDisplay("799x600");
  delegate->set_minimum_size(gfx::Size(395, 0));
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(window1.get()));
  delegate->set_minimum_size(gfx::Size(396, 0));
  EXPECT_FALSE(split_view_controller()->CanSnapWindow(window1.get()));
}

// Test that |SplitViewController::CanSnapWindow| property checks that the
// unresizable snapping condition.
TEST_F(SplitViewControllerTest, CanSnapWindowWithUnresizableSnapProperty) {
  UpdateDisplay("800x600");
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorNone);
  EXPECT_FALSE(split_view_controller()->CanSnapWindow(window.get()));

  // Clamshell mode supports unresizable snapping.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  window->SetProperty(kUnresizableSnappedSizeKey, new gfx::Size(300, 0));
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(window.get()));

  // Tablet mode doesn't support unresizable snapping.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_FALSE(split_view_controller()->CanSnapWindow(window.get()));

  // If the display is too small for the unresizable snapping, it can't be
  // snapped.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  UpdateDisplay("200x100");
  EXPECT_FALSE(split_view_controller()->CanSnapWindow(window.get()));
}

// Tests that the snapped window can not be moved outside of work area when its
// minimum size is larger than its current desired resizing bounds.
TEST_F(SplitViewControllerTest, ResizingSnappedWindowWithMinimumSizeTest) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  const gfx::Rect bounds(0, 0, 300, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate1 =
      static_cast<aura::test::TestWindowDelegate*>(window1->delegate());

  // Set the screen orientation to LANDSCAPE_PRIMARY
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);

  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());
  ToggleOverview();
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(window1.get()));
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  delegate1->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.4f, display_bounds.height()));
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());

  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResizeWithDivider(divider_bounds.CenterPoint());
  gfx::Point resize_point(display_bounds.width() * 0.33f, 0);
  split_view_controller()->ResizeWithDivider(resize_point);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.SingleWindow", 1);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.SingleWindow",
      0);

  gfx::Rect snapped_window_bounds =
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::SnapPosition::kPrimary, window1.get());
  // The snapped window bounds can't be pushed outside of the display area.
  EXPECT_EQ(snapped_window_bounds.x(), display_bounds.x());
  EXPECT_EQ(snapped_window_bounds.width(),
            window1->delegate()->GetMinimumSize().width());
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  split_view_controller()->EndResizeWithDivider(resize_point);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.SingleWindow", 1);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.SingleWindow",
      1);

  SkipDividerSnapAnimation();
  EndSplitView();

  // Rotate the screen by 270 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);

  display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());
  delegate1->set_minimum_size(
      gfx::Size(display_bounds.width(), display_bounds.height() * 0.4f));
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(window1.get()));
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResizeWithDivider(divider_bounds.CenterPoint());
  resize_point.SetPoint(0, display_bounds.height() * 0.33f);
  split_view_controller()->ResizeWithDivider(resize_point);

  snapped_window_bounds =
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::SnapPosition::kPrimary, window1.get());
  EXPECT_EQ(snapped_window_bounds.y(), display_bounds.y());
  EXPECT_EQ(snapped_window_bounds.height(),
            window1->delegate()->GetMinimumSize().height());
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  split_view_controller()->EndResizeWithDivider(resize_point);
  SkipDividerSnapAnimation();
  EndSplitView();

  // Rotate the screen by 180 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapeSecondary);

  display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());
  delegate1->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.4f, display_bounds.height()));
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(window1.get()));
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());

  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResizeWithDivider(divider_bounds.CenterPoint());
  resize_point.SetPoint(display_bounds.width() * 0.33f, 0);
  split_view_controller()->ResizeWithDivider(resize_point);

  snapped_window_bounds =
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::SnapPosition::kSecondary, window1.get());
  EXPECT_EQ(snapped_window_bounds.x(), display_bounds.x());
  EXPECT_EQ(snapped_window_bounds.width(),
            window1->delegate()->GetMinimumSize().width());
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  split_view_controller()->EndResizeWithDivider(resize_point);
  SkipDividerSnapAnimation();
  EndSplitView();

  // Rotate the screen by 90 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitSecondary);

  display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());
  delegate1->set_minimum_size(
      gfx::Size(display_bounds.width(), display_bounds.height() * 0.4f));
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(window1.get()));
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());

  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResizeWithDivider(divider_bounds.CenterPoint());
  resize_point.SetPoint(0, display_bounds.height() * 0.33f);
  split_view_controller()->ResizeWithDivider(resize_point);

  snapped_window_bounds =
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::SnapPosition::kSecondary, window1.get());
  EXPECT_EQ(snapped_window_bounds.y(), display_bounds.y());
  EXPECT_EQ(snapped_window_bounds.height(),
            window1->delegate()->GetMinimumSize().height());
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  split_view_controller()->EndResizeWithDivider(resize_point);
  SkipDividerSnapAnimation();
  EndSplitView();
}

// Tests that the divider should not be moved to a position that is smaller than
// the snapped window's minimum size after resizing.
TEST_F(SplitViewControllerTest,
       DividerPositionOnResizingSnappedWindowWithMinimumSizeTest) {
  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate1 =
      static_cast<aura::test::TestWindowDelegate*>(window1->delegate());
  ui::test::EventGenerator* generator = GetEventGenerator();
  EXPECT_EQ(chromeos::OrientationType::kLandscapePrimary,
            GetCurrentScreenOrientation());
  gfx::Rect workarea_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());

  // Snap the divider to one third position when there is only left window with
  // minimum size larger than one third of the display's width. The divider
  // should be snapped to the middle position after dragging.
  ToggleOverview();
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  delegate1->set_minimum_size(
      gfx::Size(workarea_bounds.width() * 0.4f, workarea_bounds.height()));
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.33f, 0));
  SkipDividerSnapAnimation();
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());

  // Snap the divider to two third position, it should be kept at there after
  // dragging.
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.67f, 0));
  SkipDividerSnapAnimation();
  EXPECT_GT(divider_position(), 0.5f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.67f * workarea_bounds.width());
  EndSplitView();

  // Snap the divider to two third position when there is only right window with
  // minium size larger than one third of the display's width. The divider
  // should be snapped to the middle position after dragging.
  delegate1->set_minimum_size(
      gfx::Size(workarea_bounds.width() * 0.4f, workarea_bounds.height()));
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kSecondary);
  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.67f, 0));
  SkipDividerSnapAnimation();
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());

  // Snap the divider to one third position, it should be kept at there after
  // dragging.
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.33f, 0));
  SkipDividerSnapAnimation();
  EXPECT_GT(divider_position(), 0);
  EXPECT_LE(divider_position(), 0.33f * workarea_bounds.width());
  EndSplitView();

  // Snap the divider to one third position when there are both left and right
  // snapped windows with the same minimum size larger than one third of the
  // display's width. The divider should be snapped to the middle position after
  // dragging.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate2 =
      static_cast<aura::test::TestWindowDelegate*>(window2->delegate());
  delegate2->set_minimum_size(
      gfx::Size(workarea_bounds.width() * 0.4f, workarea_bounds.height()));
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.33f, 0));
  SkipDividerSnapAnimation();
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());

  // Snap the divider to two third position, it should be snapped to the middle
  // position after dragging.
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.67f, 0));
  SkipDividerSnapAnimation();
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());
  EndSplitView();
}

// Tests that the divider and snapped windows bounds should be updated if
// snapping a new window with minimum size, which is larger than the bounds
// of its snap position.
TEST_F(SplitViewControllerTest,
       DividerPositionWithWindowMinimumSizeOnSnapTest) {
  const gfx::Rect bounds(0, 0, 200, 300);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  const gfx::Rect workarea_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());

  // Divider should be moved to the middle at the beginning.
  ToggleOverview();
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  ASSERT_TRUE(split_view_divider());
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());

  // Drag the divider to two-third position.
  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.67f, 0));
  SkipDividerSnapAnimation();
  EXPECT_GT(divider_position(), 0.5f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.67f * workarea_bounds.width());

  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate2 =
      static_cast<aura::test::TestWindowDelegate*>(window2->delegate());
  delegate2->set_minimum_size(
      gfx::Size(workarea_bounds.width() * 0.4f, workarea_bounds.height()));
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());
}

// Test that if display configuration changes in lock screen, the split view
// mode doesn't end.
TEST_F(SplitViewControllerTest, DoNotEndSplitViewInLockScreen) {
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  UpdateDisplay("800x400");
  const gfx::Rect bounds(0, 0, 200, 300);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);

  // Now lock the screen.
  GetSessionControllerClient()->LockScreen();
  // Change display configuration. Split view mode is still active.
  UpdateDisplay("400x800");
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);

  // Now unlock the screen.
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
}

// Test that when split view and overview are both active when a new window is
// added to the window hierarchy, overview is not ended.
TEST_F(SplitViewControllerTest, NewWindowTest) {
  const gfx::Rect bounds(0, 0, 200, 300);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  ToggleOverview();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Now new a window. Test it won't end the overview mode
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that when split view ends because of a transition from tablet mode to
// laptop mode during a resize operation, drags are properly completed.
TEST_F(SplitViewControllerTest, ExitTabletModeDuringResizeCompletesDrags) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  auto* w1_state = WindowState::Get(window1.get());
  auto* w2_state = WindowState::Get(window2.get());

  // Setup delegates
  auto* window_state_delegate1 = new TestWindowStateDelegate();
  auto* window_state_delegate2 = new TestWindowStateDelegate();
  w1_state->SetDelegate(base::WrapUnique(window_state_delegate1));
  w2_state->SetDelegate(base::WrapUnique(window_state_delegate2));

  // Set up windows.
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);

  // Start a drag but don't release the mouse button.
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParent(window1.get()).width();
  GetEventGenerator()->set_current_screen_location(
      divider_bounds.CenterPoint());
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseTo(screen_width * 0.67f, 0);

  // Drag is started for both windows.
  EXPECT_TRUE(window_state_delegate1->drag_in_progress());
  EXPECT_TRUE(window_state_delegate2->drag_in_progress());
  EXPECT_NE(nullptr, w1_state->drag_details());
  EXPECT_NE(nullptr, w2_state->drag_details());

  // End tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);

  // Drag is ended for both windows.
  EXPECT_EQ(nullptr, w1_state->drag_details());
  EXPECT_EQ(nullptr, w2_state->drag_details());
  EXPECT_FALSE(window_state_delegate1->drag_in_progress());
  EXPECT_FALSE(window_state_delegate2->drag_in_progress());
}

// Tests that when a single window is present in split view mode is minimized
// during a resize operation, then drags are properly completed.
TEST_F(SplitViewControllerTest,
       MinimizeSingleWindowDuringResizeCompletesDrags) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  auto* w1_state = WindowState::Get(window1.get());

  // Setup delegate
  auto* window_state_delegate1 = new TestWindowStateDelegate();
  w1_state->SetDelegate(base::WrapUnique(window_state_delegate1));

  // Set up window.
  ToggleOverview();
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);

  // Start a drag but don't release the mouse button.
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParentForActiveDeskContainer(
          window1.get())
          .width();
  GetEventGenerator()->set_current_screen_location(
      divider_bounds.CenterPoint());
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseTo(screen_width * 0.67f, 0);

  // Drag is started.
  EXPECT_TRUE(window_state_delegate1->drag_in_progress());
  EXPECT_NE(nullptr, w1_state->drag_details());

  // Minimize the window.
  WMEvent minimize_event(WM_EVENT_MINIMIZE);
  WindowState::Get(window1.get())->OnWMEvent(&minimize_event);

  // Drag is ended.
  EXPECT_FALSE(window_state_delegate1->drag_in_progress());
  EXPECT_EQ(nullptr, w1_state->drag_details());
}

// Tests that when two windows are present in split view mode and one of them
// is minimized during a resize, then drags are properly completed.
TEST_F(SplitViewControllerTest,
       MinimizeOneOfTwoWindowsDuringResizeCompletesDrags) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  auto* w1_state = WindowState::Get(window1.get());
  auto* w2_state = WindowState::Get(window2.get());

  // Setup delegates
  auto* window_state_delegate1 = new TestWindowStateDelegate();
  auto* window_state_delegate2 = new TestWindowStateDelegate();
  w1_state->SetDelegate(base::WrapUnique(window_state_delegate1));
  w2_state->SetDelegate(base::WrapUnique(window_state_delegate2));

  // Set up windows.
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);

  // Start a drag but don't release the mouse button.
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParent(window1.get()).width();
  GetEventGenerator()->set_current_screen_location(
      divider_bounds.CenterPoint());
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseTo(screen_width * 0.67f, 0);

  // Drag is started for both windows.
  EXPECT_TRUE(window_state_delegate1->drag_in_progress());
  EXPECT_TRUE(window_state_delegate2->drag_in_progress());
  EXPECT_NE(nullptr, w1_state->drag_details());
  EXPECT_NE(nullptr, w2_state->drag_details());

  // Minimize the left window.
  WMEvent minimize_event(WM_EVENT_MINIMIZE);
  WindowState::Get(window1.get())->OnWMEvent(&minimize_event);

  // Drag is ended as the window is detached from splitview.
  EXPECT_FALSE(window_state_delegate1->drag_in_progress());
  EXPECT_FALSE(window_state_delegate2->drag_in_progress());
  EXPECT_EQ(nullptr, w1_state->drag_details());
  EXPECT_EQ(nullptr, w2_state->drag_details());
}

// Test that when a snapped window's resizablity property change from resizable
// to unresizable, the split view mode is ended.
TEST_F(SplitViewControllerTest, ResizabilityChangeTest) {
  const gfx::Rect bounds(0, 0, 200, 300);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  window1->SetProperty(aura::client::kResizeBehaviorKey,
                       aura::client::kResizeBehaviorNone);
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Tests that shadows on windows disappear when the window is snapped, and
// reappear when unsnapped.
TEST_F(SplitViewControllerTest, ShadowDisappearsWhenSnapped) {
  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ::wm::ShadowController* shadow_controller = Shell::Get()->shadow_controller();
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window1.get()));
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window2.get()));
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window3.get()));

  // Snap |window1| to the left. Its shadow should disappear.
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window1.get()));
  auto* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_session = overview_controller->overview_session();
  EXPECT_TRUE(overview_session->IsWindowInOverview(window2.get()));
  EXPECT_TRUE(overview_session->IsWindowInOverview(window3.get()));
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window2.get()));
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window3.get()));

  // Snap |window2| to the right. Its shadow should also disappear.
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window1.get()));
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window2.get()));
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window3.get()));

  // Snap |window3| to the right. Its shadow should disappear and |window2|'s
  // shadow should reappear.
  split_view_controller()->SnapWindow(
      window3.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window1.get()));
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window2.get()));
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window3.get()));
}

// Tests that if snapping a window causes overview to end (e.g., select two
// windows in overview mode to snap to both side of the screen), or toggle
// overview to end overview causes a window to snap, we should not have the
// exiting animation.
TEST_F(SplitViewControllerTest, OverviewExitAnimationTest) {
  ui::ScopedAnimationDurationScaleMode anmatin_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  // 1) For normal toggle overview case, we should have animation when
  // exiting overview.
  std::unique_ptr<OverviewStatesObserver> overview_observer =
      std::make_unique<OverviewStatesObserver>(window1->GetRootWindow());
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  ToggleOverview();
  WaitForOverviewExitAnimation();
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_TRUE(overview_observer->overview_animate_when_exiting());
  CheckOverviewEnterExitHistogram("NormalEnterExit", {1, 0}, {1, 0});

  // 2) If overview is ended because of activating a window:
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  // It will end overview.
  wm::ActivateWindow(window1.get());
  WaitForOverviewExitAnimation();
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_TRUE(overview_observer->overview_animate_when_exiting());
  CheckOverviewEnterExitHistogram("EnterExitByActivation", {2, 0}, {2, 0});

  // 3) If overview is ended because of snapping a window:
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  WaitForOverviewEnterAnimation();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  // Reset the observer as we'll need the OverviewStatesObserver to be added to
  // to ShellObserver list after SplitViewController.
  overview_observer =
      std::make_unique<OverviewStatesObserver>(window1->GetRootWindow());
  // Test |overview_animate_when_exiting_| has been properly reset.
  EXPECT_TRUE(overview_observer->overview_animate_when_exiting());
  CheckOverviewEnterExitHistogram("EnterInSplitView", {2, 1}, {2, 0});

  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  WaitForOverviewExitAnimation();
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_FALSE(overview_observer->overview_animate_when_exiting());
  CheckOverviewEnterExitHistogram("ExitBySnap", {2, 1}, {2, 1});

  // 4) If ending overview causes a window to snap:
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  // Test |overview_animate_when_exiting_| has been properly reset.
  EXPECT_TRUE(overview_observer->overview_animate_when_exiting());
  CheckOverviewEnterExitHistogram("EnterInSplitView2", {2, 2}, {2, 1});

  ToggleOverview();
  WaitForOverviewExitAnimation();
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_FALSE(overview_observer->overview_animate_when_exiting());
  CheckOverviewEnterExitHistogram("ExitInSplitView", {2, 2}, {2, 2});
}

// Test the window state is normally maximized on splitview end, except when we
// end it from home launcher.
TEST_F(SplitViewControllerTest, WindowStateOnExit) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  using svc = SplitViewController::SnapPosition;
  // Tests that normally, window will maximize on splitview ended.
  split_view_controller()->SnapWindow(window1.get(), svc::kPrimary);
  split_view_controller()->SnapWindow(window2.get(), svc::kSecondary);
  split_view_controller()->EndSplitView();
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMaximized());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsMaximized());

  // Tests that if we end splitview from home launcher, the windows do not get
  // maximized.
  split_view_controller()->SnapWindow(window1.get(), svc::kPrimary);
  split_view_controller()->SnapWindow(window2.get(), svc::kSecondary);
  split_view_controller()->EndSplitView(
      SplitViewController::EndReason::kHomeLauncherPressed);
  EXPECT_FALSE(WindowState::Get(window1.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(window2.get())->IsMaximized());
}

// Test that if overview and splitview are both active at the same time,
// activiate an unsnappable window should end both overview and splitview mode.
TEST_F(SplitViewControllerTest, ActivateNonSnappableWindow) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateNonSnappableWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  wm::ActivateWindow(window3.get());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that if a snapped window has a bubble transient child, the bubble's
// bounds should always align with the snapped window's bounds.
TEST_F(SplitViewControllerTest, AdjustTransientChildBounds) {
  std::unique_ptr<views::Widget> widget(CreateTestWidget());
  aura::Window* window = widget->GetNativeWindow();
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanResize |
                          aura::client::kResizeBehaviorCanMaximize);
  split_view_controller()->SnapWindow(
      window, SplitViewController::SnapPosition::kPrimary);
  const gfx::Rect window_bounds = window->GetBoundsInScreen();

  // Create a bubble widget that's anchored to |widget|.
  views::Widget* bubble_widget = views::BubbleDialogDelegateView::CreateBubble(
      new TestBubbleDialogDelegateView(widget->GetContentsView()));
  aura::Window* bubble_window = bubble_widget->GetNativeWindow();
  EXPECT_TRUE(::wm::HasTransientAncestor(bubble_window, window));
  // Test that the bubble is created inside its anchor widget.
  EXPECT_TRUE(window_bounds.Contains(bubble_window->GetBoundsInScreen()));

  // Now try to manually move the bubble out of the snapped window.
  bubble_window->SetBoundsInScreen(
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::SnapPosition::kSecondary, window),
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
  // Test that the bubble can't be moved outside of its anchor widget.
  EXPECT_TRUE(window_bounds.Contains(bubble_window->GetBoundsInScreen()));
  EndSplitView();
}

// Tests the divider closest position ratio if work area is not starts from the
// top of the display.
TEST_F(SplitViewControllerTest, DividerClosestRatioOnWorkArea) {
  UpdateDisplay("1200x800");
  // Docked magnifier will put a view port window on the top of the display.
  Shell::Get()->docked_magnifier_controller()->SetEnabled(true);

  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  ui::test::EventGenerator* generator = GetEventGenerator();
  ASSERT_EQ(chromeos::OrientationType::kLandscapePrimary,
            test_api.GetCurrentOrientation());

  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  ToggleOverview();
  split_view_controller()->SnapWindow(
      window.get(), SplitViewController::SnapPosition::kPrimary);

  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(chromeos::OrientationType::kPortraitSecondary,
            test_api.GetCurrentOrientation());
  EXPECT_EQ(divider_closest_ratio(), chromeos::kDefaultSnapRatio);

  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(chromeos::OrientationType::kLandscapePrimary,
            test_api.GetCurrentOrientation());
  EXPECT_EQ(divider_closest_ratio(), chromeos::kDefaultSnapRatio);
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  gfx::Rect workarea_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window.get());
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  // Drag the divider to one third position of the work area's width.
  generator->DragMouseTo(
      gfx::Point(workarea_bounds.width() * chromeos::kOneThirdSnapRatio,
                 workarea_bounds.y()));
  SkipDividerSnapAnimation();
  EXPECT_EQ(divider_closest_ratio(), chromeos::kOneThirdSnapRatio);

  // Divider closest position ratio changed from one third to two thirds if
  // left/top window changes.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(chromeos::OrientationType::kPortraitSecondary,
            test_api.GetCurrentOrientation());
  EXPECT_EQ(divider_closest_ratio(), chromeos::kTwoThirdSnapRatio);

  // Divider closest position ratio is kept as one third if left/top window
  // doesn't changes.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(chromeos::OrientationType::kPortraitPrimary,
            test_api.GetCurrentOrientation());
  EXPECT_EQ(divider_closest_ratio(), chromeos::kOneThirdSnapRatio);
}

// Tests that the divider closest position ratio is properly updated for display
// rotation after a clamshell/tablet transition that does not trigger a call to
// |SplitViewController::OnDisplayMetricsChanged|. The point here is that if
// |SplitViewController::is_previous_layout_right_side_up_| is only ever updated
// in |SplitViewController::OnDisplayMetricsChanged|, then a clamshell/tablet
// transition can leave it with a stale value which can cause broken behavior.
TEST_F(SplitViewControllerTest,
       DividerClosestRatioUpdatedForClamshellTabletTransition) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  // Set the display orientation to landscape secondary (upside down).
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  // Switch to clamshell mode.
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->SetEnabledForTest(false);
  // Set the display orientation to landscape secondary (upside down).
  Shell::Get()->display_manager()->SetDisplayRotation(
      display_id, display::Display::ROTATE_180,
      display::Display::RotationSource::ACTIVE);
  // Switch to tablet mode.
  tablet_mode_controller->SetEnabledForTest(true);
  // Enter split view.
  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  ToggleOverview();
  split_view_controller()->SnapWindow(
      window.get(), SplitViewController::SnapPosition::kPrimary);
  // Drag the divider so that the snapped window spans only one third of the way
  // across the work area.
  ui::test::EventGenerator* generator = GetEventGenerator();
  const gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  const gfx::Rect workarea_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window.get());
  generator->DragMouseTo(
      gfx::Point(workarea_bounds.width() * chromeos::kOneThirdSnapRatio,
                 workarea_bounds.y()));
  SkipDividerSnapAnimation();
  // Expect that the divider closest position ratio is two thirds with the
  // display upside down.
  EXPECT_EQ(divider_closest_ratio(), chromeos::kTwoThirdSnapRatio);
  // Set the display orientation to landscape primary (right side up).
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  // Expect that the divider closest position ratio is updated to one third.
  EXPECT_EQ(divider_closest_ratio(), chromeos::kOneThirdSnapRatio);
}

// Test that pinning a window ends split view mode.
TEST_F(SplitViewControllerTest, PinningWindowEndsSplitView) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  window_util::PinWindow(window1.get(), true);
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Test that split view mode is disallowed while we're in pinned mode (there is
// a pinned window).
TEST_F(SplitViewControllerTest, PinnedWindowDisallowsSplitView) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));

  EXPECT_TRUE(ShouldAllowSplitView());

  window_util::PinWindow(window1.get(), true);
  EXPECT_FALSE(ShouldAllowSplitView());
}

// Test that if split view ends while the divider is dragged to where a snapped
// window is sliding off the screen because it has reached minimum size, then
// the offset is cleared.
TEST_F(SplitViewControllerTest, EndSplitViewWhileResizingBeyondMinimum) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  const gfx::Rect bounds(0, 0, 300, 200);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate =
      static_cast<aura::test::TestWindowDelegate*>(window->delegate());

  // Set the screen orientation to LANDSCAPE_PRIMARY
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);

  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window.get());
  ToggleOverview();
  split_view_controller()->SnapWindow(
      window.get(), SplitViewController::SnapPosition::kPrimary);
  delegate->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.4f, display_bounds.height()));

  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResizeWithDivider(divider_bounds.CenterPoint());
  gfx::Point resize_point(display_bounds.width() * 0.33f, 0);
  split_view_controller()->ResizeWithDivider(resize_point);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.SingleWindow", 1);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.SingleWindow",
      0);

  ASSERT_FALSE(window->layer()->GetTargetTransform().IsIdentity());
  EndSplitView();
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.SingleWindow", 1);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.SingleWindow",
      1);

  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
}

// Test if presentation time is recorded for multi window resizing and resizing
// with overview.
TEST_F(SplitViewControllerTest, ResizeTwoWindows) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  const gfx::Rect bounds(0, 0, 300, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);

  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResizeWithDivider(divider_bounds.CenterPoint());
  gfx::Point resize_point(display_bounds.width() * chromeos::kOneThirdSnapRatio,
                          0);
  split_view_controller()->ResizeWithDivider(resize_point);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.MultiWindow", 1);
  split_view_controller()->ResizeWithDivider(
      gfx::Point(resize_point.x(), resize_point.y() + 1));
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.MultiWindow", 2);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.MultiWindow",
      0);

  split_view_controller()->EndResizeWithDivider(resize_point);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.MultiWindow", 2);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.MultiWindow",
      1);

  ToggleOverview();

  split_view_controller()->StartResizeWithDivider(divider_bounds.CenterPoint());
  split_view_controller()->ResizeWithDivider(resize_point);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.WithOverview", 1);
  split_view_controller()->ResizeWithDivider(
      gfx::Point(resize_point.x(), resize_point.y() + 1));
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.WithOverview", 2);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.WithOverview",
      0);
  split_view_controller()->EndResizeWithDivider(resize_point);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.WithOverview", 2);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.WithOverview",
      1);
}

// Test that if split view ends during the divider snap animation while a
// snapped window is sliding off the screen because it has reached minimum size,
// then the animation is ended and the window offset is cleared.
TEST_F(SplitViewControllerTest, EndSplitViewDuringDividerSnapAnimation) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  const gfx::Rect bounds(0, 0, 300, 200);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate =
      static_cast<aura::test::TestWindowDelegate*>(window->delegate());

  // Set the screen orientation to LANDSCAPE_PRIMARY
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);

  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window.get());
  ToggleOverview();
  split_view_controller()->SnapWindow(
      window.get(), SplitViewController::SnapPosition::kPrimary);
  delegate->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.4f, display_bounds.height()));

  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResizeWithDivider(divider_bounds.CenterPoint());
  gfx::Point resize_point((int)(display_bounds.width() * 0.33f) + 20, 0);
  split_view_controller()->ResizeWithDivider(resize_point);
  split_view_controller()->EndResizeWithDivider(resize_point);
  ASSERT_TRUE(IsDividerAnimating());
  ASSERT_FALSE(window->layer()->GetTargetTransform().IsIdentity());
  EndSplitView();
  EXPECT_FALSE(IsDividerAnimating());
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
}

// TestOverviewObserver which tracks how many overview items there are when
// overview mode is about to end.
class TestOverviewItemsOnOverviewModeEndObserver : public OverviewObserver {
 public:
  TestOverviewItemsOnOverviewModeEndObserver() {
    Shell::Get()->overview_controller()->AddObserver(this);
  }

  TestOverviewItemsOnOverviewModeEndObserver(
      const TestOverviewItemsOnOverviewModeEndObserver&) = delete;
  TestOverviewItemsOnOverviewModeEndObserver& operator=(
      const TestOverviewItemsOnOverviewModeEndObserver&) = delete;

  ~TestOverviewItemsOnOverviewModeEndObserver() override {
    Shell::Get()->overview_controller()->RemoveObserver(this);
  }
  void OnOverviewModeEnding(OverviewSession* overview_session) override {
    items_on_last_overview_end_ = overview_session->num_items();
  }
  int items_on_last_overview_end() const { return items_on_last_overview_end_; }

 private:
  int items_on_last_overview_end_ = 0;
};

TEST_F(SplitViewControllerTest, ItemsRemovedFromOverviewOnSnap) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  ASSERT_EQ(
      2u, Shell::Get()->overview_controller()->overview_session()->num_items());
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(
      1u, Shell::Get()->overview_controller()->overview_session()->num_items());

  // Create |observer| after splitview is entered so that it gets notified after
  // splitview does, and so will notice the changes splitview made to overview
  // on overview end.
  TestOverviewItemsOnOverviewModeEndObserver observer;
  ToggleOverview();
  EXPECT_EQ(0, observer.items_on_last_overview_end());
}

// Test that resizing ends properly if split view ends during divider dragging.
TEST_F(SplitViewControllerTest, EndSplitViewWhileDragging) {
  // Enter split view mode.
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(
      window.get(), SplitViewController::SnapPosition::kPrimary);

  // Start resizing.
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);

  split_view_controller()->StartResizeWithDivider(divider_bounds.CenterPoint());

  // Verify the setup.
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  ASSERT_TRUE(split_view_controller()->is_resizing_with_divider());

  gfx::Point resize_point(divider_bounds.CenterPoint());
  resize_point.Offset(100, 0);

  split_view_controller()->ResizeWithDivider(resize_point);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.SingleWindow", 1);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.SingleWindow",
      0);

  // End split view and check that resizing has ended properly.
  split_view_controller()->EndSplitView();
  EXPECT_FALSE(split_view_controller()->is_resizing_with_divider());
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.SingleWindow", 1);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.SingleWindow",
      1);
}

// Tests that auto snapping is properly triggered if a window is going to
// unminimized (visible but minimized) in tablet split view mode.
TEST_F(SplitViewControllerTest, AutoSnapFromMinimizedState) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateNonSnappableWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  // Nothing should happen in clamshell mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  WindowState::Get(window1.get())->Minimize();
  window1->Show();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window1.get()));

  // Nothing should happen not in tablet split view mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  WindowState::Get(window1.get())->Minimize();
  window1->Show();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window1.get()));

  // Nothing should happen for a non-snappable window.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  WindowState::Get(window2.get())->Minimize();
  window2->Show();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window2.get()));

  // Nothing should happen for transient visibility changing due to dragging.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  split_view_controller()->SnapWindow(
      window3.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_controller()->InTabletSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window3.get()));
  EXPECT_EQ(split_view_controller()->GetPositionOfSnappedWindow(window3.get()),
            SplitViewController::SnapPosition::kPrimary);

  WindowState::Get(window1.get())->Minimize();
  window1->SetProperty(kHideDuringWindowDragging, true);
  window1->Show();
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window1.get()));

  window1->ClearProperty(kHideDuringWindowDragging);

  // Should performs auto snapping when showing a snappable window in table
  // split view mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  split_view_controller()->SnapWindow(
      window3.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_controller()->InTabletSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window3.get()));
  EXPECT_EQ(split_view_controller()->GetPositionOfSnappedWindow(window3.get()),
            SplitViewController::SnapPosition::kPrimary);

  WindowState::Get(window1.get())->Minimize();
  window1->Show();
  EXPECT_TRUE(split_view_controller()->InTabletSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_EQ(split_view_controller()->GetPositionOfSnappedWindow(window1.get()),
            SplitViewController::SnapPosition::kSecondary);

  EndSplitView();
}

// Test that if the transient parent window is no longer snapped in split view,
// split view divider should no longer observe the transient child window.
TEST_F(SplitViewControllerTest, DoNotObserveTransientIfNotInSplitview) {
  // Create two normal window.
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  // Add another two windows with one being a bubble transient child of the
  // other.
  std::unique_ptr<views::Widget> widget(CreateTestWidget());
  aura::Window* parent = widget->GetNativeWindow();
  parent->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanResize |
                          aura::client::kResizeBehaviorCanMaximize);
  views::Widget* bubble_widget = views::BubbleDialogDelegateView::CreateBubble(
      new TestBubbleDialogDelegateView(widget->GetContentsView()));
  aura::Window* bubble_transient = bubble_widget->GetNativeWindow();
  EXPECT_TRUE(::wm::HasTransientAncestor(bubble_transient, parent));

  ToggleOverview();
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      parent, SplitViewController::SnapPosition::kSecondary);
  EXPECT_TRUE(bubble_transient->HasObserver(split_view_divider()));

  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);
  EXPECT_FALSE(bubble_transient->HasObserver(split_view_divider()));
}

// Test that if a snapped window is destroyed during resizing, we should end
// resizing.
TEST_F(SplitViewControllerTest, WindowDestroyedDuringResize) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);

  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResizeWithDivider(divider_bounds.CenterPoint());
  split_view_controller()->ResizeWithDivider(gfx::Point(100, 100));

  window1.reset();
  EXPECT_FALSE(split_view_controller()->is_resizing_with_divider());
}

TEST_F(SplitViewControllerTest, WMSnapEvent) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // Test the functionalities in tablet mode.
  // Sending WM_EVENT_SNAP_SECONDARY to |window1| will snap to left.
  WMEvent wm_left_snap_event(WM_EVENT_SNAP_PRIMARY);
  WindowState::Get(window1.get())->OnWMEvent(&wm_left_snap_event);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->primary_window(), window1.get());
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window2.get()));
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  OverviewSession* overview_session = overview_controller->overview_session();
  EXPECT_TRUE(overview_session->IsWindowInOverview(window2.get()));

  // Sending WM_EVENT_SNAP_SECONDARY to |window1| will snap to right.
  WMEvent wm_right_snap_event(WM_EVENT_SNAP_SECONDARY);
  WindowState::Get(window1.get())->OnWMEvent(&wm_right_snap_event);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->secondary_window(), window1.get());
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window2.get()));
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_session->IsWindowInOverview(window2.get()));

  // Sending WM_EVENT_SNAP_SECONDARY to |window2| will replace |window1|.
  WindowState::Get(window2.get())->OnWMEvent(&wm_right_snap_event);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->secondary_window(), window2.get());
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_session->IsWindowInOverview(window1.get()));

  // Sending WM_EVENT_SNAP_PRIMARY to |window1| to snap |window1|.
  WindowState::Get(window1.get())->OnWMEvent(&wm_left_snap_event);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->primary_window(), window1.get());
  EXPECT_EQ(split_view_controller()->secondary_window(), window2.get());
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Sending WM_EVENT_SNAP_SECONDARY to |window1| will replace |window2| and put
  // |window2| in overview.
  WindowState::Get(window1.get())->OnWMEvent(&wm_right_snap_event);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->secondary_window(), window1.get());
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window2.get()));
  EXPECT_TRUE(overview_controller->InOverviewSession());
  overview_session = overview_controller->overview_session();
  EXPECT_TRUE(overview_session->IsWindowInOverview(window2.get()));
  ToggleOverview();
  EndSplitView();

  // Test the functionalities in clamshell mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  // Sending WM_EVENT_SNAP_PRIMARY to |window1| will snap to left but won't put
  // |window1| in splitview.
  WindowState::Get(window1.get())->OnWMEvent(&wm_left_snap_event);
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());

  ToggleOverview();
  // Sending WM_EVENT_SNAP_PRIMARY to |window1| to snap to left while overview
  // is active will put |window1| in splitview and |window2| in overview.
  WindowState::Get(window1.get())->OnWMEvent(&wm_left_snap_event);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  overview_session = overview_controller->overview_session();
  EXPECT_TRUE(overview_session->IsWindowInOverview(window2.get()));

  // Sending WM_EVENT_SNAP_SECONDARY to |window1| to snap to right while
  // overview is active will put |window1| to snap to the right in splitview and
  // |window2| remains in overview.
  WindowState::Get(window1.get())->OnWMEvent(&wm_right_snap_event);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->secondary_window(), window1.get());
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window2.get()));
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_session->IsWindowInOverview(window2.get()));
}

// Tests that the split view divider observers the snapped windows when the
// tablet mode split view starts.
TEST_F(SplitViewControllerTest, SplitViewDividerObserveSnappedWindow) {
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  // Exit tablet mode.
  tablet_mode_controller->SetEnabledForTest(false);
  EXPECT_FALSE(tablet_mode_controller->InTabletMode());

  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> left_window(CreateWindow(bounds));
  std::unique_ptr<aura::Window> right_window(CreateWindow(bounds));

  // Snap the left and right window.
  split_view_controller()->SnapWindow(
      left_window.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      right_window.get(), SplitViewController::SnapPosition::kSecondary);

  // Entering tablet mode will start tablet mode split view and the split view
  // divider will be created.
  tablet_mode_controller->SetEnabledForTest(true);
  EXPECT_TRUE(tablet_mode_controller->InTabletMode());
  EXPECT_TRUE(split_view_controller()->InTabletSplitViewMode());
  EXPECT_TRUE(split_view_divider());

  // The left and right windows are observed by split view divider.
  aura::Window::Windows observed_windows =
      split_view_divider()->observed_windows_for_testing();
  EXPECT_TRUE(base::Contains(observed_windows, left_window.get()));
  EXPECT_TRUE(base::Contains(observed_windows, right_window.get()));
}

// Tests that snap between different ratios in the same position works as
// intended.
TEST_F(SplitViewControllerTest, SnapBetweenDifferentRatios) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  // Snap `window1` to primary position and `window2` to secondary position,
  // both with default snap ratios.
  WMEvent snap_primary_default(WM_EVENT_SNAP_PRIMARY);
  WindowState::Get(window1.get())->OnWMEvent(&snap_primary_default);
  WMEvent snap_secondary_default(WM_EVENT_SNAP_SECONDARY);
  WindowState::Get(window2.get())->OnWMEvent(&snap_secondary_default);

  // Test that both window bounds are at half the work area width and that the
  // divider is positioned at half of the work area width minus the
  // `divider_delta`.
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  int divider_origin_x = split_view_divider()
                             ->GetDividerBoundsInScreen(
                                 /*is_dragging=*/false)
                             .x();
  const int divider_delta = kSplitviewDividerShortSideLength / 2;
  EXPECT_EQ(
      divider_origin_x,
      work_area_bounds.width() * chromeos::kDefaultSnapRatio - divider_delta);
  EXPECT_EQ(work_area_bounds.width() * chromeos::kDefaultSnapRatio,
            window1->bounds().width() + divider_delta);
  EXPECT_EQ(work_area_bounds.width() * chromeos::kDefaultSnapRatio,
            window2->bounds().width() + divider_delta);

  // Snap `window1`, still in primary position, but with two thirds snap ratio.
  WMEvent snap_primary_two_third(WM_EVENT_SNAP_PRIMARY,
                                 chromeos::kTwoThirdSnapRatio);
  WindowState::Get(window1.get())->OnWMEvent(&snap_primary_two_third);

  // Test that the window bounds have updated to two thirds and one third of the
  // work area width respectively. The the divider is positioned at two thirds
  // of the work area width minus the `divider_delta`.
  divider_origin_x = split_view_divider()
                         ->GetDividerBoundsInScreen(
                             /*is_dragging=*/false)
                         .x();
  EXPECT_EQ(divider_origin_x, std::round(work_area_bounds.width() *
                                         chromeos::kTwoThirdSnapRatio) -
                                  divider_delta);
  EXPECT_EQ(std::round(work_area_bounds.width() * chromeos::kTwoThirdSnapRatio),
            window1->bounds().width() + divider_delta);
  EXPECT_EQ(std::round(work_area_bounds.width() * chromeos::kOneThirdSnapRatio),
            window2->bounds().width() + divider_delta);
}

// Tests that swap partial windows keeps the window sizes.
TEST_F(SplitViewControllerTest, SwapPartialWindows) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  // Snap `window1` to primary with 2/3 width and `window2` to secondary with
  // 1/3 width. Verify the divider is at 2/3 of the work area.
  WMEvent snap_primary_two_third(WM_EVENT_SNAP_PRIMARY,
                                 chromeos::kTwoThirdSnapRatio);
  WindowState::Get(window1.get())->OnWMEvent(&snap_primary_two_third);
  WMEvent snap_secondary_one_third(WM_EVENT_SNAP_SECONDARY,
                                   chromeos::kOneThirdSnapRatio);
  WindowState::Get(window2.get())->OnWMEvent(&snap_secondary_one_third);
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  int divider_origin_x = split_view_divider()
                             ->GetDividerBoundsInScreen(
                                 /*is_dragging=*/false)
                             .x();
  const int divider_delta = kSplitviewDividerShortSideLength / 2;
  EXPECT_EQ(divider_origin_x, std::round(work_area_bounds.width() *
                                         chromeos::kTwoThirdSnapRatio) -
                                  divider_delta);
  EXPECT_EQ(std::round(work_area_bounds.width() * chromeos::kTwoThirdSnapRatio),
            window1->bounds().width() + divider_delta);
  EXPECT_EQ(std::round(work_area_bounds.width() * chromeos::kOneThirdSnapRatio),
            window2->bounds().width() + divider_delta);

  // Verify that after swapping windows, the window widths remain the same, and
  // the divider is now at 1/3 of the work area.
  split_view_controller()->SwapWindows(
      SplitViewController::SwapWindowsSource::kDoubleTap);
  EXPECT_EQ(WindowState::Get(window1.get())->GetStateType(),
            chromeos::WindowStateType::kSecondarySnapped);
  EXPECT_EQ(WindowState::Get(window2.get())->GetStateType(),
            chromeos::WindowStateType::kPrimarySnapped);
  divider_origin_x = split_view_divider()
                         ->GetDividerBoundsInScreen(
                             /*is_dragging=*/false)
                         .x();
  // TODO(sammiequon): Investigate why these are off by 1 pixel but were not
  // earlier.
  EXPECT_NEAR(
      divider_origin_x,
      std::round(work_area_bounds.width() * chromeos::kOneThirdSnapRatio) -
          divider_delta,
      1);
  EXPECT_NEAR(
      std::round(work_area_bounds.width() * chromeos::kTwoThirdSnapRatio),
      window1->bounds().width() + divider_delta, 1);
  EXPECT_NEAR(
      std::round(work_area_bounds.width() * chromeos::kOneThirdSnapRatio),
      window2->bounds().width() + divider_delta, 1);
}

// Tests that we can snap two thirds even when one half is not available.
TEST_F(SplitViewControllerTest, SnapTwoThirdPartialWindow) {
  UpdateDisplay("800x600");

  // Create a window that has a minimum width such that it cannot be snapped one
  // half, but can be snapped two thirds.
  aura::test::TestWindowDelegate window_delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &window_delegate, /*id=*/-1, gfx::Rect(500, 500)));
  window_delegate.set_minimum_size(gfx::Size(500, 500));
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(AppType::BROWSER));

  WMEvent snap_primary(WM_EVENT_SNAP_PRIMARY, chromeos::kTwoThirdSnapRatio);
  WindowState::Get(window.get())->OnWMEvent(&snap_primary);
  EXPECT_TRUE(WindowState::Get(window.get())->IsSnapped());
}

// Tests that selecting a window that cannot be one third snapped from overview
// will maximize it and exit splitview. Regression test for b/278921341.
TEST_F(SplitViewControllerTest, SelectWindowCannotOneThirdSnap) {
  UpdateDisplay("900x600");

  // The first window can be snapped 2/3, but not 1/2 or 1/3.
  aura::test::TestWindowDelegate window_delegate1;
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithDelegate(
      &window_delegate1, /*id=*/-1, gfx::Rect(500, 500)));
  window_delegate1.set_minimum_size(gfx::Size(500, 500));
  window1->SetProperty(aura::client::kAppType,
                       static_cast<int>(AppType::BROWSER));

  // The second window can be snapped 1/2 but not 1/3.
  aura::test::TestWindowDelegate window_delegate2;
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithDelegate(
      &window_delegate2, /*id=*/-1, gfx::Rect(500, 500)));
  window_delegate2.set_minimum_size(gfx::Size(400, 400));
  window2->SetProperty(aura::client::kAppType,
                       static_cast<int>(AppType::BROWSER));

  // Snap `window1` 2/3 to the left.
  wm::ActivateWindow(window1.get());
  WMEvent snap_primary(WM_EVENT_SNAP_PRIMARY, chromeos::kTwoThirdSnapRatio);
  WindowState::Get(window1.get())->OnWMEvent(&snap_primary);
  ASSERT_EQ(chromeos::kTwoThirdSnapRatio,
            WindowState::Get(window1.get())->snap_ratio());
  ASSERT_TRUE(WindowState::Get(window1.get())->IsSnapped());
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Select `window2`. Test that both windows are maximized and we have exited
  // splitview.
  wm::ActivateWindow(window2.get());
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMaximized());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsMaximized());
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
}

// Tests that, if two windows are snapped and one window has min size, trying to
// partial split the other window opens Overview and updates bounds correctly.
TEST_F(SplitViewControllerTest, SnapWindowWithMinSizeOpensOverview) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  // Snap 2 windows in split view. Set `window2` min length to be 0.4 of
  // the work area so it can't fit in 1/3 split.
  gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());
  aura::test::TestWindowDelegate* delegate2 =
      static_cast<aura::test::TestWindowDelegate*>(window2->delegate());
  delegate2->set_minimum_size(
      gfx::Size(work_area_bounds.width() * 0.4f, work_area_bounds.height()));
  split_view_controller()->SnapWindow(
      window1.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      window2.get(), SplitViewController::SnapPosition::kSecondary);

  // Snap `window1` to 2/3. Since `window2` can't fit in 1/3, test that we open
  // Overview instead.
  WMEvent snap_primary_two_third(WM_EVENT_SNAP_PRIMARY,
                                 chromeos::kTwoThirdSnapRatio);
  WindowState::Get(window1.get())->OnWMEvent(&snap_primary_two_third);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Activate `window2`, i.e. from Overview. Test that `window2` gets pushed to
  // 1/2 and `window1` also gets updated to 1/2.
  wm::ActivateWindow(window2.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  const int divider_delta = kSplitviewDividerShortSideLength / 2;
  EXPECT_EQ(work_area_bounds.width() * 0.5f,
            window1->bounds().width() + divider_delta);
  EXPECT_EQ(work_area_bounds.width() * 0.5f,
            window2->bounds().width() + divider_delta);
}

// Tests that auto-snap for partial windows works correctly.
TEST_F(SplitViewControllerTest, AutoSnapPartialWindows) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  // 1. Test without min size. Snap `window1` to 2/3.
  WMEvent snap_primary_two_third(WM_EVENT_SNAP_PRIMARY,
                                 chromeos::kTwoThirdSnapRatio);
  WindowState::Get(window1.get())->OnWMEvent(&snap_primary_two_third);
  // Activate `window2`. Test that `window2` gets auto-snapped to 1/3.
  wm::ActivateWindow(window2.get());
  gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  const int divider_delta = kSplitviewDividerShortSideLength / 2;
  EXPECT_EQ(std::round(work_area_bounds.width() * chromeos::kTwoThirdSnapRatio),
            window1->bounds().width() + divider_delta);
  EXPECT_EQ(std::round(work_area_bounds.width() * chromeos::kOneThirdSnapRatio),
            window2->bounds().width() + divider_delta);
  EndSplitView();

  // 2. Test with min size. Set `window2` min length so that it can't fit in 1/3
  // split. Snap `window1` to primary 2/3.
  aura::test::TestWindowDelegate* delegate2 =
      static_cast<aura::test::TestWindowDelegate*>(window2->delegate());
  delegate2->set_minimum_size(
      gfx::Size(work_area_bounds.width() * 0.4f, work_area_bounds.height()));
  WindowState::Get(window1.get())->OnWMEvent(&snap_primary_two_third);
  // Activate `window2`. Test that `window2` gets auto-snapped but pushed to 1/2
  // and `window1` also gets updated to 1/2.
  wm::ActivateWindow(window2.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(work_area_bounds.width() * chromeos::kDefaultSnapRatio,
            window1->bounds().width() + divider_delta);
  EXPECT_EQ(work_area_bounds.width() * chromeos::kDefaultSnapRatio,
            window2->bounds().width() + divider_delta);
}

// Tests that the split view divider will be stacked above the two observed
// windows in split view. On window drag started, the divider will be placed
// below the dragged window. On window drag ended, the divider will be placed
// back on top of the two observed windows.
TEST_F(SplitViewControllerTest, StackingOrderWithDivider) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SplitViewController* controller = split_view_controller();
  controller->SnapWindow(w1.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_EQ(split_view_controller()->primary_window(), w1.get());
  split_view_controller()->SnapWindow(
      w2.get(), SplitViewController::SnapPosition::kSecondary);

  EXPECT_EQ(controller->state(), SplitViewController::State::kBothSnapped);
  SplitViewDivider* divider = split_view_divider();
  ASSERT_TRUE(divider);
  aura::Window* divider_widget_native_window =
      divider->divider_widget()->GetNativeWindow();
  EXPECT_TRUE(IsStackedBelow(w1.get(), divider_widget_native_window));
  EXPECT_TRUE(IsStackedBelow(w2.get(), divider_widget_native_window));

  controller->OnWindowDragStarted(w1.get());
  EXPECT_TRUE(IsStackedBelow(divider_widget_native_window, w1.get()));

  controller->OnWindowDragCanceled();
  EXPECT_TRUE(IsStackedBelow(w1.get(), divider_widget_native_window));
  EXPECT_TRUE(IsStackedBelow(w2.get(), divider_widget_native_window));
}

// Tests that windows with different containers can be snapped properly with no
// crash. The stacking order and parent of the split view divider will be
// updated correctly with window activation and dragging operations.
TEST_F(SplitViewControllerTest, SnapWindowsWithDifferentParentContainers) {
  std::unique_ptr<aura::Window> always_on_top_window(CreateTestWindow());
  always_on_top_window->SetProperty(aura::client::kZOrderingKey,
                                    ui::ZOrderLevel::kFloatingWindow);
  std::unique_ptr<aura::Window> normal_window(CreateTestWindow());
  SplitViewController* controller = split_view_controller();
  controller->SnapWindow(always_on_top_window.get(),
                         SplitViewController::SnapPosition::kPrimary);
  controller->SnapWindow(normal_window.get(),
                         SplitViewController::SnapPosition::kSecondary);
  EXPECT_EQ(controller->state(), SplitViewController::State::kBothSnapped);

  SplitViewDivider* divider = split_view_divider();
  ASSERT_TRUE(divider);
  aura::Window* divider_widget_native_window =
      divider->divider_widget()->GetNativeWindow();
  EXPECT_EQ(divider_widget_native_window->parent(),
            always_on_top_window->parent());
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            always_on_top_window->GetProperty(aura::client::kZOrderingKey));
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            normal_window->GetProperty(aura::client::kZOrderingKey));

  wm::ActivateWindow(always_on_top_window.get());
  EXPECT_EQ(controller->state(), SplitViewController::State::kBothSnapped);
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            always_on_top_window->GetProperty(aura::client::kZOrderingKey));
  EXPECT_TRUE(
      IsStackedBelow(always_on_top_window.get(), divider_widget_native_window));

  wm::ActivateWindow(normal_window.get());
  EXPECT_EQ(controller->state(), SplitViewController::State::kBothSnapped);
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            always_on_top_window->GetProperty(aura::client::kZOrderingKey));
  EXPECT_TRUE(
      IsStackedBelow(always_on_top_window.get(), divider_widget_native_window));

  // The split view divider will be stacked below the dragged window i.e.
  // `normal_window` temporarily during dragging. The divider will also be
  // reparented to be sibling of `normal_window` while dragging.
  controller->OnWindowDragStarted(normal_window.get());
  EXPECT_EQ(divider_widget_native_window->parent(), normal_window->parent());
  EXPECT_TRUE(
      IsStackedBelow(divider_widget_native_window, normal_window.get()));

  // On drag ended, the split view divider will be stacked back on top of the
  // above window i.e. the `always_on_top_window`. The divider will also be
  // reparented to be sibling of `always_on_top_window`.
  controller->OnWindowDragCanceled();
  EXPECT_EQ(divider_widget_native_window->parent(),
            always_on_top_window->parent());
  EXPECT_TRUE(
      IsStackedBelow(always_on_top_window.get(), divider_widget_native_window));
}

TEST_F(SplitViewControllerTest, WMSnapEventDeviceOrientationMetricsInTablet) {
  UpdateDisplay("800x600");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  ASSERT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);

  constexpr char kDeviceOrientationTablet[] =
      "Ash.SplitView.DeviceOrientation.TabletMode";
  constexpr char kDeviceOrientationEntryPoint[] =
      "Ash.SplitView.EntryPoint.DeviceOrientation";
  constexpr char kDeviceOrientationInSplitView[] =
      "Ash.SplitView.OrientationInSplitView";
  base::HistogramTester histogram_tester;
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // 1. Test landscape orientation.
  // Snap |window1| to the left to enter split view overview in tablet mode.
  WMEvent wm_left_snap_event(WM_EVENT_SNAP_PRIMARY);
  WindowState::Get(window1.get())->OnWMEvent(&wm_left_snap_event);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  histogram_tester.ExpectBucketCount(
      kDeviceOrientationTablet,
      SplitViewMetricsController::DeviceOrientation::kLandscape, 1);
  histogram_tester.ExpectBucketCount(
      kDeviceOrientationEntryPoint,
      SplitViewMetricsController::DeviceOrientation::kLandscape, 1);

  // 2. Test portrait orientation.
  // Rotate the screen by 270 degrees to portrait primary orientation.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  ASSERT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);
  histogram_tester.ExpectBucketCount(
      kDeviceOrientationTablet,
      SplitViewMetricsController::DeviceOrientation::kPortrait, 1);
  histogram_tester.ExpectBucketCount(
      kDeviceOrientationInSplitView,
      SplitViewMetricsController::DeviceOrientation::kPortrait, 1);
}

TEST_F(SplitViewControllerTest,
       WMSnapEventDeviceOrientationMetricsInClamshell) {
  UpdateDisplay("800x600/l");
  base::HistogramTester histogram_tester;
  constexpr char kDeviceOrientationClamshell[] =
      "Ash.SplitView.DeviceOrientation.ClamshellMode";
  constexpr char kDeviceOrientationEntryPoint[] =
      "Ash.SplitView.EntryPoint.DeviceOrientation";
  constexpr char kDeviceOrientationInSplitView[] =
      "Ash.SplitView.OrientationInSplitView";
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  wm::ActivateWindow(window1.get());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  const WMEvent wm_left_snap_event(WM_EVENT_SNAP_PRIMARY);
  const WMEvent wm_right_snap_event(WM_EVENT_SNAP_SECONDARY);
  const WMEvent fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);

  // 1. Test portrait orientation.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  // Snap |window1| to the left.
  WindowState::Get(window1.get())->OnWMEvent(&wm_left_snap_event);
  histogram_tester.ExpectBucketCount(
      kDeviceOrientationClamshell,
      SplitViewMetricsController::DeviceOrientation::kPortrait, 0);

  // Snap |window2| to the right. With windows snapped to both side, split view
  // metric controller should start recording metrics.
  wm::ActivateWindow(window2.get());
  WindowState::Get(window2.get())->OnWMEvent(&wm_right_snap_event);
  histogram_tester.ExpectBucketCount(
      kDeviceOrientationClamshell,
      SplitViewMetricsController::DeviceOrientation::kPortrait, 1);
  histogram_tester.ExpectBucketCount(
      kDeviceOrientationEntryPoint,
      SplitViewMetricsController::DeviceOrientation::kPortrait, 1);

  // 2. Test landscape orientation.
  histogram_tester.ExpectBucketCount(
      kDeviceOrientationClamshell,
      SplitViewMetricsController::DeviceOrientation::kLandscape, 0);
  histogram_tester.ExpectBucketCount(
      kDeviceOrientationInSplitView,
      SplitViewMetricsController::DeviceOrientation::kLandscape, 0);
  // Update display to landscape and check that the counts for orientation
  // metrics increase except the count for orientation entry point.
  UpdateDisplay("800x600");
  histogram_tester.ExpectBucketCount(
      kDeviceOrientationClamshell,
      SplitViewMetricsController::DeviceOrientation::kLandscape, 1);
  histogram_tester.ExpectBucketCount(
      kDeviceOrientationInSplitView,
      SplitViewMetricsController::DeviceOrientation::kLandscape, 1);
  histogram_tester.ExpectBucketCount(
      kDeviceOrientationEntryPoint,
      SplitViewMetricsController::DeviceOrientation::kLandscape, 0);

  // Unsnap |window1| by making it fullscreen and snap back to the left to
  // trigger recording split view metrics.
  WindowState::Get(window1.get())->OnWMEvent(&fullscreen_event);
  WindowState::Get(window1.get())->OnWMEvent(&wm_left_snap_event);
  histogram_tester.ExpectBucketCount(
      kDeviceOrientationClamshell,
      SplitViewMetricsController::DeviceOrientation::kLandscape, 2);
  histogram_tester.ExpectBucketCount(
      kDeviceOrientationInSplitView,
      SplitViewMetricsController::DeviceOrientation::kLandscape, 1);
  histogram_tester.ExpectBucketCount(
      kDeviceOrientationEntryPoint,
      SplitViewMetricsController::DeviceOrientation::kLandscape, 1);
}

// The test class that enables the feature flag of portrait mode split view
// virtual keyboard improvement and the virtual keyboard.
class SplitViewKeyboardTest : public SplitViewControllerTest {
 public:
  SplitViewKeyboardTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kAdjustSplitViewForVK);
  }

  SplitViewKeyboardTest(const SplitViewKeyboardTest&) = delete;
  SplitViewKeyboardTest& operator=(const SplitViewKeyboardTest&) = delete;

  ~SplitViewKeyboardTest() override = default;

  // SplitViewControllerTest:
  void SetUp() override {
    SplitViewControllerTest::SetUp();
    SetVirtualKeyboardEnabled(true);
  }

  keyboard::KeyboardUIController* keyboard_controller() {
    return keyboard::KeyboardUIController::Get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that when the input field in the bottom window is blocked by the
// virtual keyboard (the bottom of the caret is less than
// `kMinCaretKeyboardDist` above the virtual keyboard), the bottom window will
// be pushed above the virtual keyboard.
TEST_F(SplitViewKeyboardTest, PushUpBottomWindow) {
  UpdateDisplay("1200x800");

  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  ASSERT_EQ(chromeos::OrientationType::kLandscapePrimary,
            test_api.GetCurrentOrientation());

  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> bottom_window(CreateWindow(bounds));
  auto bottom_client =
      std::make_unique<TestTextInputClient>(bottom_window.get());
  split_view_controller()->SnapWindow(
      bottom_window.get(), SplitViewController::SnapPosition::kSecondary);

  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(chromeos::OrientationType::kPortraitPrimary,
            test_api.GetCurrentOrientation());
  EXPECT_FALSE(split_view_controller()->IsPhysicalLeftOrTop(
      SplitViewController::SnapPosition::kSecondary, bottom_window.get()));

  const gfx::Rect keyboard_bounds =
      keyboard_controller()->GetKeyboardWindow()->GetBoundsInScreen();
  const gfx::Rect orig_bottom_bounds = bottom_window->GetBoundsInScreen();
  const gfx::Rect orig_divider_bounds = split_view_controller()
                                            ->split_view_divider()
                                            ->divider_widget()
                                            ->GetWindowBoundsInScreen();

  // Set the caret position in bottom window above the upper bounds of the
  // virtual keyboard. When the virtual keyboard is enabled, the bottom window
  // will not shift.
  bottom_client->set_caret_bounds(gfx::Rect(
      keyboard_bounds.top_center() +
          gfx::Vector2d(0, -kMinCaretKeyboardDist - kCaretHeightForTest - 10),
      gfx::Size(0, kCaretHeightForTest)));
  bottom_client->Focus();
  EXPECT_TRUE(keyboard_controller()->IsKeyboardVisible());
  EXPECT_EQ(orig_bottom_bounds, bottom_window->GetBoundsInScreen());
  // The split view divider is adjustable and not moved.
  EXPECT_EQ(orig_divider_bounds, split_view_controller()
                                     ->split_view_divider()
                                     ->divider_widget()
                                     ->GetWindowBoundsInScreen());
  EXPECT_TRUE(split_view_controller()->split_view_divider()->IsAdjustable());

  // Disable the keyboard.
  bottom_client->UnFocus();
  EXPECT_FALSE(keyboard_controller()->IsKeyboardVisible());

  const gfx::Rect shift_bottom_bounds(
      keyboard_bounds.origin() + gfx::Vector2d(0, -orig_bottom_bounds.height()),
      orig_bottom_bounds.size());
  const gfx::Rect shift_divider_bounds(
      shift_bottom_bounds.origin() +
          gfx::Vector2d(0, -orig_divider_bounds.height()),
      orig_divider_bounds.size());
  // Set the caret position in bottom window below the upper bounds of the
  // virtual keyboard. When the virtual keyboard is enabled, the bottom window
  // will shift above the virtual keyboard.
  bottom_client->set_caret_bounds(
      gfx::Rect(keyboard_bounds.top_center() + gfx::Vector2d(0, 10),
                gfx::Size(0, kCaretHeightForTest)));
  bottom_client->Focus();
  EXPECT_TRUE(keyboard_controller()->IsKeyboardVisible());
  EXPECT_EQ(shift_bottom_bounds, bottom_window->GetBoundsInScreen());
  // The split view divider will also be shifted and become unadjustable.
  EXPECT_EQ(shift_divider_bounds, split_view_controller()
                                      ->split_view_divider()
                                      ->divider_widget()
                                      ->GetWindowBoundsInScreen());
  EXPECT_FALSE(split_view_controller()->split_view_divider()->IsAdjustable());

  // Disable the keyboard. The bottom window will restore to original bounds.
  // The split view divider will also be adjustable and restore to original
  // bounds.
  bottom_client->UnFocus();
  EXPECT_FALSE(keyboard_controller()->IsKeyboardVisible());
  EXPECT_EQ(orig_bottom_bounds, bottom_window->GetBoundsInScreen());
  EXPECT_EQ(orig_divider_bounds, split_view_controller()
                                     ->split_view_divider()
                                     ->divider_widget()
                                     ->GetWindowBoundsInScreen());
  EXPECT_TRUE(split_view_controller()->split_view_divider()->IsAdjustable());
}

// When the bottom window is pushed up due to the virtual keyboard and the
// shifted window position cannot exceed `1 - kMinDividerPositionRatio` of the
// screen height.
TEST_F(SplitViewKeyboardTest, PushUpBottomWindowLimitHeight) {
  UpdateDisplay("1200x800");

  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  ASSERT_EQ(chromeos::OrientationType::kLandscapePrimary,
            test_api.GetCurrentOrientation());

  gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> bottom_window(CreateWindow(bounds));
  auto bottom_client =
      std::make_unique<TestTextInputClient>(bottom_window.get());
  split_view_controller()->SnapWindow(
      bottom_window.get(), SplitViewController::SnapPosition::kSecondary);

  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(chromeos::OrientationType::kPortraitPrimary,
            test_api.GetCurrentOrientation());
  EXPECT_FALSE(split_view_controller()->IsPhysicalLeftOrTop(
      SplitViewController::SnapPosition::kSecondary, bottom_window.get()));

  const gfx::Rect keyboard_bounds =
      keyboard_controller()->GetKeyboardWindow()->GetBoundsInScreen();
  const gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  const gfx::Rect screen_bounds =
      screen_util::GetDisplayWorkAreaBoundsInParent(bottom_window.get());
  const int screen_height = screen_bounds.height();
  const int limit_y = screen_height * kMinDividerPositionRatio;

  // Resize divider to a position that when the bottom window is pushed up, its
  // position will exceeds `1-kMinDividerPositionRatio` of screen height.
  split_view_controller()->StartResizeWithDivider(divider_bounds.CenterPoint());
  split_view_controller()->ResizeWithDivider(
      gfx::Point(0, screen_height * 0.15f));

  const gfx::Rect orig_bottom_bounds = bottom_window->GetBoundsInScreen();
  EXPECT_LT(keyboard_bounds.y() - orig_bottom_bounds.height(), limit_y);

  const gfx::Rect orig_divider_bounds = split_view_controller()
                                            ->split_view_divider()
                                            ->divider_widget()
                                            ->GetWindowBoundsInScreen();

  // Set the caret position in bottom window below the upper bounds of the
  // virtual keyboard. When the virtual keyboard is enabled, the bottom window
  // will shift above the virtual keyboard but the upper bounds will be limited
  // to `kMinDividerPositionRatio` of the screen height.
  const gfx::Rect shift_bottom_bounds(0, limit_y, keyboard_bounds.width(),
                                      keyboard_bounds.y() - limit_y);
  const gfx::Rect shift_divider_bounds(
      shift_bottom_bounds.origin() +
          gfx::Vector2d(0, -orig_divider_bounds.height()),
      orig_divider_bounds.size());

  bottom_client->set_caret_bounds(gfx::Rect(keyboard_bounds.top_center(),
                                            gfx::Size(0, kCaretHeightForTest)));
  bottom_client->Focus();
  EXPECT_TRUE(keyboard_controller()->IsKeyboardVisible());
  EXPECT_EQ(shift_bottom_bounds, bottom_window->GetBoundsInScreen());
  // The split view divider will also be shifted and become unadjustable.
  EXPECT_EQ(shift_divider_bounds, split_view_controller()
                                      ->split_view_divider()
                                      ->divider_widget()
                                      ->GetWindowBoundsInScreen());
  EXPECT_FALSE(split_view_controller()->split_view_divider()->IsAdjustable());

  // Disable the keyboard. The bottom window will restore to original bounds.
  // The split view divider will also be adjustable and restore to original
  // bounds.
  bottom_client->UnFocus();
  EXPECT_FALSE(keyboard_controller()->IsKeyboardVisible());
  EXPECT_EQ(orig_bottom_bounds, bottom_window->GetBoundsInScreen());
  EXPECT_EQ(orig_divider_bounds, split_view_controller()
                                     ->split_view_divider()
                                     ->divider_widget()
                                     ->GetWindowBoundsInScreen());
  EXPECT_TRUE(split_view_controller()->split_view_divider()->IsAdjustable());
}

// Tests that when the bottom window is pushed up due to the virtual keyboard
// and the top window is activated, then the bottom window should restore to the
// original layout.
TEST_F(SplitViewKeyboardTest, RestoreByActivatingTopWindow) {
  UpdateDisplay("1200x800");

  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  ASSERT_EQ(chromeos::OrientationType::kLandscapePrimary,
            test_api.GetCurrentOrientation());

  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> top_window(CreateWindow(bounds));
  std::unique_ptr<aura::Window> bottom_window(CreateWindow(bounds));
  auto top_client = std::make_unique<TestTextInputClient>(top_window.get());
  auto bottom_client =
      std::make_unique<TestTextInputClient>(bottom_window.get());
  split_view_controller()->SnapWindow(
      top_window.get(), SplitViewController::SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(
      bottom_window.get(), SplitViewController::SnapPosition::kSecondary);

  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(chromeos::OrientationType::kPortraitPrimary,
            test_api.GetCurrentOrientation());
  EXPECT_TRUE(split_view_controller()->IsPhysicalLeftOrTop(
      SplitViewController::SnapPosition::kPrimary, top_window.get()));

  const gfx::Rect keyboard_bounds =
      keyboard_controller()->GetKeyboardWindow()->GetBoundsInScreen();
  const gfx::Rect orig_bottom_bounds = bottom_window->GetBoundsInScreen();
  const gfx::Rect shift_bottom_bounds(
      keyboard_bounds.origin() + gfx::Vector2d(0, -orig_bottom_bounds.height()),
      orig_bottom_bounds.size());
  const gfx::Rect orig_divider_bounds = split_view_controller()
                                            ->split_view_divider()
                                            ->divider_widget()
                                            ->GetWindowBoundsInScreen();
  const gfx::Rect shift_divider_bounds(
      shift_bottom_bounds.origin() +
          gfx::Vector2d(0, -orig_divider_bounds.height()),
      orig_divider_bounds.size());

  // Set the caret position in bottom window below the upper bounds of the
  // virtual keyboard. When the virtual keyboard is enabled, the bottom window
  // will shift.
  bottom_client->set_caret_bounds(
      gfx::Rect(keyboard_bounds.top_center() + gfx::Vector2d(0, 10),
                gfx::Size(0, kCaretHeightForTest)));
  bottom_client->Focus();
  EXPECT_TRUE(keyboard_controller()->IsKeyboardVisible());
  EXPECT_EQ(shift_bottom_bounds, bottom_window->GetBoundsInScreen());
  // The split view divider will also be shifted and become unadjustable.
  EXPECT_EQ(shift_divider_bounds, split_view_controller()
                                      ->split_view_divider()
                                      ->divider_widget()
                                      ->GetWindowBoundsInScreen());
  EXPECT_FALSE(split_view_controller()->split_view_divider()->IsAdjustable());

  // Activate the top window. The bottom window will restore to original bounds.
  top_client->Focus();
  EXPECT_TRUE(keyboard_controller()->IsKeyboardVisible());
  EXPECT_EQ(orig_bottom_bounds, bottom_window->GetBoundsInScreen());
  EXPECT_EQ(orig_divider_bounds, split_view_controller()
                                     ->split_view_divider()
                                     ->divider_widget()
                                     ->GetWindowBoundsInScreen());
  EXPECT_TRUE(split_view_controller()->split_view_divider()->IsAdjustable());
}

// Tests that when there is no activated input field in the bottom window,
// showing keyboard (on-screen keyboard) will not change the split view layout.
TEST_F(SplitViewKeyboardTest, NoInputField) {
  UpdateDisplay("1200x800");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  ASSERT_EQ(chromeos::OrientationType::kLandscapePrimary,
            test_api.GetCurrentOrientation());

  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> bottom_window(CreateWindow(bounds));

  split_view_controller()->SnapWindow(
      bottom_window.get(), SplitViewController::SnapPosition::kSecondary);

  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(chromeos::OrientationType::kPortraitPrimary,
            test_api.GetCurrentOrientation());
  EXPECT_FALSE(split_view_controller()->IsPhysicalLeftOrTop(
      SplitViewController::SnapPosition::kSecondary, bottom_window.get()));

  const gfx::Rect orig_bottom_bounds = bottom_window->GetBoundsInScreen();
  const gfx::Rect orig_divider_bounds = split_view_controller()
                                            ->split_view_divider()
                                            ->divider_widget()
                                            ->GetWindowBoundsInScreen();
  // Enable keyboard. The bottom window and divider will not move since there is
  // no input field.
  keyboard_controller()->ShowKeyboard(/*lock=*/false);
  EXPECT_TRUE(keyboard_controller()->IsKeyboardVisible());
  EXPECT_EQ(orig_bottom_bounds, bottom_window->GetBoundsInScreen());
  EXPECT_EQ(orig_divider_bounds, split_view_controller()
                                     ->split_view_divider()
                                     ->divider_widget()
                                     ->GetWindowBoundsInScreen());
  EXPECT_TRUE(split_view_controller()->split_view_divider()->IsAdjustable());
}

// Tests that in the split view with Overview enabled, the snapped window bounds
// will be updated when the on-screen keyboard is enabled and disabled.
TEST_F(SplitViewKeyboardTest, ShowHideOnScreenKeyboardWithOverviewEnabled) {
  UpdateDisplay("1200x800");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  std::unique_ptr<aura::Window> right_window(
      CreateWindow(gfx::Rect(0, 0, 400, 400)));
  for (auto rotation :
       {display::Display::ROTATE_0, display::Display::ROTATE_270}) {
    EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
    test_api.SetDisplayRotation(rotation,
                                display::Display::RotationSource::ACTIVE);
    // Cache the original work area.
    const gfx::Rect origin_work_area =
        screen_util::GetDisplayWorkAreaBoundsInParent(right_window.get());

    // Enable an on-screen virtual keyboard. The display work area should shrink
    // the size of intersection between on-screen keyboard and original work
    // area.
    keyboard_controller()->ShowKeyboard(/*lock=*/true);
    const gfx::Rect shrink_work_area =
        screen_util::GetDisplayWorkAreaBoundsInParent(right_window.get());
    const gfx::Rect keyboard_bounds =
        keyboard_controller()->GetKeyboardWindow()->GetBoundsInScreen();
    EXPECT_EQ(origin_work_area.height() - shrink_work_area.height(),
              gfx::IntersectRects(keyboard_bounds, origin_work_area).height());

    // Snapping the window will enable Overview, the window's bottom is equal to
    // the shrunk work area bottom.
    split_view_controller()->SnapWindow(
        right_window.get(), SplitViewController::SnapPosition::kSecondary);
    EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
    EXPECT_EQ(right_window->bounds().bottom(), shrink_work_area.bottom());

    // Dismiss on-screen keyboard, the window's bottom is equal to the original
    // work area bottom.
    keyboard_controller()->HideKeyboardByUser();
    EXPECT_EQ(right_window->bounds().bottom(), origin_work_area.bottom());
    EndSplitView();
    ExitOverview();
  }
}

}  // namespace ash
