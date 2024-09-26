// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_window_resizer.h"
#include "base/memory/raw_ptr.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/metrics/pip_uma.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/pip/pip_test_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

using ::chromeos::WindowStateType;

// WindowState based on a given initial state. Records the last resize bounds.
class FakeWindowState : public WindowState::State {
 public:
  explicit FakeWindowState(WindowStateType initial_state_type)
      : state_type_(initial_state_type) {}

  FakeWindowState(const FakeWindowState&) = delete;
  FakeWindowState& operator=(const FakeWindowState&) = delete;

  ~FakeWindowState() override = default;

  // WindowState::State overrides:
  void OnWMEvent(WindowState* window_state, const WMEvent* event) override {
    if (event->IsBoundsEvent()) {
      if (event->type() == WM_EVENT_SET_BOUNDS) {
        const auto* set_bounds_event =
            static_cast<const SetBoundsWMEvent*>(event);
        last_bounds_ = set_bounds_event->requested_bounds();
        last_window_state_ = window_state;
      }
    }
  }
  WindowStateType GetType() const override { return state_type_; }
  void AttachState(WindowState* window_state,
                   WindowState::State* previous_state) override {}
  void DetachState(WindowState* window_state) override {}

  const gfx::Rect& last_bounds() const { return last_bounds_; }
  WindowState* last_window_state() { return last_window_state_; }

 private:
  WindowStateType state_type_;
  gfx::Rect last_bounds_;
  raw_ptr<WindowState, ExperimentalAsh> last_window_state_ = nullptr;
};

}  // namespace

using Sample = base::HistogramBase::Sample;

class PipWindowResizerTest : public AshTestBase,
                             public ::testing::WithParamInterface<
                                 std::tuple<std::string, std::size_t>> {
 public:
  PipWindowResizerTest() = default;

  PipWindowResizerTest(const PipWindowResizerTest&) = delete;
  PipWindowResizerTest& operator=(const PipWindowResizerTest&) = delete;

  ~PipWindowResizerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    SetVirtualKeyboardEnabled(true);

    const std::string& display_string = std::get<0>(GetParam());
    const std::size_t root_window_index = std::get<1>(GetParam());
    UpdateWorkArea(display_string);
    ASSERT_LT(root_window_index, Shell::GetAllRootWindows().size());
    scoped_display_ = std::make_unique<display::ScopedDisplayForNewWindows>(
        Shell::GetAllRootWindows()[root_window_index]);
    ForceHideShelvesForTest();
  }

  void TearDown() override {
    scoped_display_.reset();
    SetVirtualKeyboardEnabled(false);
    AshTestBase::TearDown();
  }

 protected:
  views::Widget* widget() { return widget_.get(); }
  aura::Window* window() { return window_; }
  FakeWindowState* test_state() { return test_state_; }
  base::HistogramTester& histograms() { return histograms_; }

  std::unique_ptr<views::Widget> CreateWidgetForTest(const gfx::Rect& bounds) {
    auto* root_window = Shell::GetRootWindowForNewWindows();
    gfx::Rect screen_bounds = bounds;
    ::wm::ConvertRectToScreen(root_window, &screen_bounds);

    std::unique_ptr<views::Widget> widget(new views::Widget);
    views::Widget::InitParams params;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = screen_bounds;
    params.z_order = ui::ZOrderLevel::kFloatingWindow;
    params.context = root_window;
    widget->Init(std::move(params));
    widget->Show();
    return widget;
  }

  PipWindowResizer* CreateResizerForTest(int window_component) {
    return CreateResizerForTest(window_component, window(),
                                window()->bounds().CenterPoint());
  }

  PipWindowResizer* CreateResizerForTest(int window_component,
                                         const gfx::Point& point_in_parent) {
    return CreateResizerForTest(window_component, window(), point_in_parent);
  }

  PipWindowResizer* CreateResizerForTest(int window_component,
                                         aura::Window* window,
                                         const gfx::Point& point_in_parent) {
    WindowState* window_state = WindowState::Get(window);
    window_state->CreateDragDetails(gfx::PointF(point_in_parent),
                                    window_component,
                                    ::wm::WINDOW_MOVE_SOURCE_MOUSE);
    return new PipWindowResizer(window_state);
  }

  gfx::PointF CalculateDragPoint(const WindowResizer& resizer,
                                 int delta_x,
                                 int delta_y) const {
    gfx::PointF location = resizer.GetInitialLocation();
    location.set_x(location.x() + delta_x);
    location.set_y(location.y() + delta_y);
    return location;
  }

  void Fling(std::unique_ptr<WindowResizer> resizer,
             float velocity_x,
             float velocity_y) {
    aura::Window* target_window = resizer->GetTarget();
    base::TimeTicks timestamp = base::TimeTicks::Now();
    ui::GestureEventDetails details = ui::GestureEventDetails(
        ui::ET_SCROLL_FLING_START, velocity_x, velocity_y);
    ui::GestureEvent event = ui::GestureEvent(
        target_window->bounds().origin().x(),
        target_window->bounds().origin().y(), ui::EF_NONE, timestamp, details);
    ui::Event::DispatcherApi(&event).set_target(target_window);
    resizer->FlingOrSwipe(&event);
  }

  void PreparePipWindow(const gfx::Rect& bounds) {
    widget_ = CreateWidgetForTest(bounds);
    window_ = widget_->GetNativeWindow();
    test_state_ = new FakeWindowState(WindowStateType::kPip);
    WindowState::Get(window_)->SetStateObject(
        std::unique_ptr<WindowState::State>(test_state_));
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<aura::Window, ExperimentalAsh> window_;
  raw_ptr<FakeWindowState, ExperimentalAsh> test_state_;
  base::HistogramTester histograms_;
  std::unique_ptr<display::ScopedDisplayForNewWindows> scoped_display_;

  void UpdateWorkArea(const std::string& bounds) {
    UpdateDisplay(bounds);
    for (aura::Window* root : Shell::GetAllRootWindows()) {
      WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
          root, gfx::Rect(), gfx::Insets(), gfx::Insets());
    }
  }
};

TEST_P(PipWindowResizerTest, PipWindowCanDrag) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));

  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ(gfx::Rect(200, 210, 100, 100), test_state()->last_bounds());
}

TEST_P(PipWindowResizerTest, PipWindowCanResize) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTBOTTOM));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ(gfx::Rect(200, 200, 100, 110), test_state()->last_bounds());
}

TEST_P(PipWindowResizerTest, PipWindowDragIsRestrictedToWorkArea) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));
  // Specify point in parent as center so the drag point does not leave the
  // display. If the drag point is not in any display bounds, it causes the
  // window to be moved to the default display.
  auto landscape =
      display::Screen::GetScreen()->GetPrimaryDisplay().is_landscape();
  int right_x = landscape ? 392 : 292;
  int bottom_y = landscape ? 292 : 392;

  std::unique_ptr<PipWindowResizer> resizer(
      CreateResizerForTest(HTCAPTION, gfx::Point(250, 250)));
  ASSERT_TRUE(resizer.get());

  // Drag to the right.
  resizer->Drag(CalculateDragPoint(*resizer, 250, 0), 0);
  EXPECT_EQ(gfx::Rect(right_x, 200, 100, 100), test_state()->last_bounds());

  // Drag down.
  resizer->Drag(CalculateDragPoint(*resizer, 0, 250), 0);
  EXPECT_EQ(gfx::Rect(200, bottom_y, 100, 100), test_state()->last_bounds());

  // Drag to the left.
  resizer->Drag(CalculateDragPoint(*resizer, -250, 0), 0);
  EXPECT_EQ(gfx::Rect(8, 200, 100, 100), test_state()->last_bounds());

  // Drag up.
  resizer->Drag(CalculateDragPoint(*resizer, 0, -250), 0);
  EXPECT_EQ(gfx::Rect(200, 8, 100, 100), test_state()->last_bounds());
}

TEST_P(PipWindowResizerTest, PipWindowCanBeDraggedInTabletMode) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  PreparePipWindow(gfx::Rect(200, 200, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ(gfx::Rect(200, 210, 100, 100), test_state()->last_bounds());
}

TEST_P(PipWindowResizerTest, PipWindowCanBeResizedInTabletMode) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  PreparePipWindow(gfx::Rect(200, 200, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTBOTTOM));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ(gfx::Rect(200, 200, 100, 110), test_state()->last_bounds());
}

TEST_P(PipWindowResizerTest, ResizingPipWindowDoesNotTriggerFling) {
  PreparePipWindow(gfx::Rect(8, 8, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTBOTTOM));
  ASSERT_TRUE(resizer.get());

  Fling(std::move(resizer), 0.f, 4000.f);

  // Ensure that the PIP window isn't flung to the bottom edge during resize.
  EXPECT_EQ(gfx::Point(8, 8), test_state()->last_bounds().origin());
}

TEST_P(PipWindowResizerTest, PipWindowCanBeSwipeDismissed) {
  PreparePipWindow(gfx::Rect(8, 8, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Drag to the left.
  resizer->Drag(CalculateDragPoint(*resizer, -100, 0), 0);

  // Should be dismissed when the drag completes.
  resizer->CompleteDrag();
  EXPECT_TRUE(widget()->IsClosed());
}

TEST_P(PipWindowResizerTest, PipWindowPartiallySwipedDoesNotDismiss) {
  PreparePipWindow(gfx::Rect(8, 8, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Drag to the left, but only a little bit.
  resizer->Drag(CalculateDragPoint(*resizer, -30, 0), 0);

  // Should not be dismissed when the drag completes.
  resizer->CompleteDrag();
  EXPECT_FALSE(widget()->IsClosed());
  EXPECT_EQ(gfx::Rect(8, 8, 100, 100), test_state()->last_bounds());
}

TEST_P(PipWindowResizerTest, PipWindowInSwipeToDismissGestureLocksToAxis) {
  PreparePipWindow(gfx::Rect(8, 8, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(
      CreateResizerForTest(HTCAPTION, gfx::Point(50, 50)));
  ASSERT_TRUE(resizer.get());

  // Drag to the left, but only a little bit, to start a swipe-to-dismiss.
  resizer->Drag(CalculateDragPoint(*resizer, -30, 0), 0);
  EXPECT_EQ(gfx::Rect(-22, 8, 100, 100), test_state()->last_bounds());

  // Now try to drag down, it should be locked to the horizontal axis.
  resizer->Drag(CalculateDragPoint(*resizer, -30, 30), 0);
  EXPECT_EQ(gfx::Rect(-22, 8, 100, 100), test_state()->last_bounds());
}

TEST_P(PipWindowResizerTest,
       PipWindowMovedAwayFromScreenEdgeNoLongerCanSwipeToDismiss) {
  PreparePipWindow(gfx::Rect(8, 16, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Drag to the right and up a bit.
  resizer->Drag(CalculateDragPoint(*resizer, 30, -8), 0);
  EXPECT_EQ(gfx::Rect(38, 8, 100, 100), test_state()->last_bounds());

  // Now try to drag to the left start a swipe-to-dismiss. It should stop
  // at the edge of the work area.
  resizer->Drag(CalculateDragPoint(*resizer, -30, -8), 0);
  EXPECT_EQ(gfx::Rect(8, 8, 100, 100), test_state()->last_bounds());
}

TEST_P(PipWindowResizerTest, PipWindowAtCornerLocksToOneAxisOnSwipeToDismiss) {
  PreparePipWindow(gfx::Rect(8, 8, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Try dragging up and to the left. It should lock onto the axis with the
  // largest displacement.
  resizer->Drag(CalculateDragPoint(*resizer, -30, -40), 0);
  EXPECT_EQ(gfx::Rect(8, -32, 100, 100), test_state()->last_bounds());
}

TEST_P(
    PipWindowResizerTest,
    PipWindowMustBeDraggedMostlyInDirectionOfDismissToInitiateSwipeToDismiss) {
  PreparePipWindow(gfx::Rect(8, 8, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Try a lot downward and a bit to the left. Swiping should not be initiated.
  resizer->Drag(CalculateDragPoint(*resizer, -30, 50), 0);
  EXPECT_EQ(gfx::Rect(8, 58, 100, 100), test_state()->last_bounds());
}

TEST_P(PipWindowResizerTest,
       PipWindowDoesNotMoveUntilStatusOfSwipeToDismissGestureIsKnown) {
  PreparePipWindow(gfx::Rect(8, 8, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Move a small amount - this should not trigger any bounds change, since
  // we don't know whether a swipe will start or not.
  resizer->Drag(CalculateDragPoint(*resizer, -4, 0), 0);
  EXPECT_TRUE(test_state()->last_bounds().IsEmpty());
}

TEST_P(PipWindowResizerTest, PipWindowIsFlungToEdge) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));
  auto landscape =
      display::Screen::GetScreen()->GetPrimaryDisplay().is_landscape();

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
    Fling(std::move(resizer), 0.f, 4000.f);

    auto origin = landscape ? gfx::Point(200, 292) : gfx::Point(200, 392);

    // Flung downwards.
    EXPECT_EQ(origin, test_state()->last_bounds().origin());
  }

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 0, -10), 0);
    Fling(std::move(resizer), 0.f, -4000.f);

    // Flung upwards.
    EXPECT_EQ(gfx::Rect(200, 8, 100, 100), test_state()->last_bounds());
  }

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 10, 0), 0);
    Fling(std::move(resizer), 4000.f, 0.f);

    auto origin = landscape ? gfx::Point(392, 200) : gfx::Point(292, 200);
    // Flung to the right.
    EXPECT_EQ(origin, test_state()->last_bounds().origin());
  }

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, -10, 0), 0);
    Fling(std::move(resizer), -4000.f, 0.f);

    // Flung to the left.
    EXPECT_EQ(gfx::Rect(8, 200, 100, 100), test_state()->last_bounds());
  }
}

TEST_P(PipWindowResizerTest, PipWindowIsFlungDiagonally) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));
  auto landscape =
      display::Screen::GetScreen()->GetPrimaryDisplay().is_landscape();

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 10, 10), 0);
    Fling(std::move(resizer), 3000.f, 3000.f);

    // Flung downward and to the right, into the corner.
    EXPECT_EQ(gfx::Rect(292, 292, 100, 100), test_state()->last_bounds());
  }

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 3, 4), 0);
    Fling(std::move(resizer), 3000.f, 4000.f);
    gfx::Point origin = landscape ? gfx::Point(269, 292) : gfx::Point(292, 322);

    // Flung downward and to the right, but reaching the bottom edge first.
    EXPECT_EQ(origin, test_state()->last_bounds().origin());
  }
  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 4, 3), 0);
    Fling(std::move(resizer), 4000.f, 3000.f);
    gfx::Point origin = landscape ? gfx::Point(322, 292) : gfx::Point(292, 269);
    // Flung downward and to the right, but reaching the right edge first.
    EXPECT_EQ(origin, test_state()->last_bounds().origin());
  }

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, -3, -4), 0);
    Fling(std::move(resizer), -3000.f, -4000.f);

    // Flung upward and to the left, but reaching the top edge first.
    EXPECT_EQ(gfx::Point(56, 8), test_state()->last_bounds().origin());
  }

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, -4, -3), 0);
    Fling(std::move(resizer), -4000.f, -3000.f);

    // Flung upward and to the left, but reaching the left edge first.
    EXPECT_EQ(gfx::Rect(8, 56, 100, 100), test_state()->last_bounds());
  }

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 3, -9), 0);
    Fling(std::move(resizer), 3000.f, -9000.f);

    // Flung upward and to the right, but reaching the top edge first.
    EXPECT_EQ(gfx::Rect(264, 8, 100, 100), test_state()->last_bounds());
  }

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 3, -3), 0);
    Fling(std::move(resizer), 3000.f, -3000.f);

    gfx::Point origin = landscape ? gfx::Point(392, 8) : gfx::Point(292, 108);
    // Flung upward and to the right, but reaching the right edge first.
    EXPECT_EQ(origin, test_state()->last_bounds().origin());
  }

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, -3, 3), 0);
    Fling(std::move(resizer), -3000.f, 3000.f);

    gfx::Point origin = landscape ? gfx::Point(108, 292) : gfx::Point(8, 392);
    // Flung downward and to the left, but reaching the bottom edge first.
    EXPECT_EQ(origin, test_state()->last_bounds().origin());
  }

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, -9, 3), 0);
    Fling(std::move(resizer), -9000.f, 3000.f);

    // Flung downward and to the left, but reaching the left edge first.
    EXPECT_EQ(gfx::Rect(8, 264, 100, 100), test_state()->last_bounds());
  }
}

TEST_P(PipWindowResizerTest, PipWindowFlungAvoidsFloatingKeyboard) {
  PreparePipWindow(gfx::Rect(200, 200, 75, 75));

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->SetContainerType(keyboard::ContainerType::kFloating,
                                        gfx::Rect(0, 0, 1, 1),
                                        base::DoNothing());
  const display::Display display = WindowState::Get(window())->GetDisplay();
  keyboard_controller->ShowKeyboardInDisplay(display);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(8, 150, 100, 100));

  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Fling to the left - but don't intersect with the floating keyboard.
  resizer->Drag(CalculateDragPoint(*resizer, -10, 0), 0);
  Fling(std::move(resizer), -4000.f, 0.f);

  // Appear below the keyboard.
  EXPECT_EQ(gfx::Rect(8, 258, 75, 75), test_state()->last_bounds());
}

TEST_P(PipWindowResizerTest, PipWindowDoesNotChangeDisplayOnDrag) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));
  const display::Display display = WindowState::Get(window())->GetDisplay();
  gfx::Rect rect_in_screen = window()->bounds();
  ::wm::ConvertRectToScreen(window()->parent(), &rect_in_screen);
  EXPECT_TRUE(display.bounds().Contains(rect_in_screen));

  // Drag inside the display.
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 10, 10), 0);

  // Ensure the position is still in the display.
  EXPECT_EQ(gfx::Rect(210, 210, 100, 100), test_state()->last_bounds());
  EXPECT_EQ(display.id(), test_state()->last_window_state()->GetDisplay().id());
  rect_in_screen = window()->bounds();
  ::wm::ConvertRectToScreen(window()->parent(), &rect_in_screen);
  EXPECT_TRUE(display.bounds().Contains(rect_in_screen));
}

TEST_P(PipWindowResizerTest, PipRestoreBoundsSetOnFling) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 10, 10), 0);
    Fling(std::move(resizer), 3000.f, 3000.f);
  }

  WindowState* window_state = WindowState::Get(window());
  EXPECT_TRUE(PipPositioner::HasSnapFraction(window_state));
}

TEST_P(PipWindowResizerTest, PipStartAndFinishFreeResizeUmaMetrics) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTBOTTOM));
  ASSERT_TRUE(resizer.get());

  EXPECT_EQ(1, histograms().GetBucketCount(kAshPipEventsHistogramName,
                                           Sample(AshPipEvents::FREE_RESIZE)));
  histograms().ExpectTotalCount(kAshPipEventsHistogramName, 1);

  resizer->Drag(CalculateDragPoint(*resizer, 100, 0), 0);
  resizer->CompleteDrag();

  histograms().ExpectTotalCount(kAshPipEventsHistogramName, 1);
}

TEST_P(PipWindowResizerTest, DragDetailsAreDestroyed) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));
  WindowState* window_state = WindowState::Get(window());

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
    EXPECT_NE(nullptr, window_state->drag_details());

    resizer->CompleteDrag();
    EXPECT_NE(nullptr, window_state->drag_details());
  }
  EXPECT_EQ(nullptr, window_state->drag_details());
}

// TODO: UpdateDisplay() doesn't support different layouts of multiple displays.
// We should add some way to try multiple layouts.
INSTANTIATE_TEST_SUITE_P(All,
                         PipWindowResizerTest,
                         testing::Values(std::make_tuple("500x400", 0u),
                                         std::make_tuple("500x400/r", 0u),
                                         std::make_tuple("500x400/u", 0u),
                                         std::make_tuple("500x400/l", 0u),
                                         std::make_tuple("1000x800*2", 0u),
                                         std::make_tuple("500x400,500x400", 0u),
                                         std::make_tuple("500x400,500x400",
                                                         1u)));

}  // namespace ash
