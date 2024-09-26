// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_aura.h"

#include <memory>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/transient_window_manager.h"

namespace views {
namespace {

NativeWidgetAura* Init(aura::Window* parent, Widget* widget) {
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.parent = parent;
  widget->Init(std::move(params));
  return static_cast<NativeWidgetAura*>(widget->native_widget());
}

// TestFocusRules is intended to provide a way to manually set a window's
// activatability so that the focus rules can be tested.
class TestFocusRules : public wm::BaseFocusRules {
 public:
  TestFocusRules() = default;

  TestFocusRules(const TestFocusRules&) = delete;
  TestFocusRules& operator=(const TestFocusRules&) = delete;

  ~TestFocusRules() override = default;

  void set_can_activate(bool can_activate) { can_activate_ = can_activate; }

  // wm::BaseFocusRules overrides:
  bool SupportsChildActivation(const aura::Window* window) const override {
    return true;
  }

  bool CanActivateWindow(const aura::Window* window) const override {
    return can_activate_;
  }

 private:
  bool can_activate_ = true;
};

class NativeWidgetAuraTest : public ViewsTestBase {
 public:
  NativeWidgetAuraTest() = default;

  NativeWidgetAuraTest(const NativeWidgetAuraTest&) = delete;
  NativeWidgetAuraTest& operator=(const NativeWidgetAuraTest&) = delete;

  ~NativeWidgetAuraTest() override = default;

  TestFocusRules* test_focus_rules() { return test_focus_rules_; }

  // testing::Test overrides:
  void SetUp() override {
    ViewsTestBase::SetUp();
    test_focus_rules_ = new TestFocusRules;
    focus_controller_ =
        std::make_unique<wm::FocusController>(test_focus_rules_);
    wm::SetActivationClient(root_window(), focus_controller_.get());
    host()->SetBoundsInPixels(gfx::Rect(640, 480));
  }

 private:
  std::unique_ptr<wm::FocusController> focus_controller_;
  raw_ptr<TestFocusRules> test_focus_rules_;
};

TEST_F(NativeWidgetAuraTest, CenterWindowLargeParent) {
  // Make a parent window larger than the host represented by
  // WindowEventDispatcher.
  auto parent = std::make_unique<aura::Window>(nullptr);
  parent->Init(ui::LAYER_NOT_DRAWN);
  parent->SetBounds(gfx::Rect(0, 0, 1024, 800));
  UniqueWidgetPtr widget = std::make_unique<Widget>();
  NativeWidgetAura* window = Init(parent.get(), widget.get());

  window->CenterWindow(gfx::Size(100, 100));
  EXPECT_EQ(gfx::Rect((640 - 100) / 2, (480 - 100) / 2, 100, 100),
            window->GetNativeWindow()->bounds());
}

TEST_F(NativeWidgetAuraTest, CenterWindowSmallParent) {
  // Make a parent window smaller than the host represented by
  // WindowEventDispatcher.
  auto parent = std::make_unique<aura::Window>(nullptr);
  parent->Init(ui::LAYER_NOT_DRAWN);
  parent->SetBounds(gfx::Rect(0, 0, 480, 320));
  UniqueWidgetPtr widget = std::make_unique<Widget>();
  NativeWidgetAura* window = Init(parent.get(), widget.get());

  window->CenterWindow(gfx::Size(100, 100));
  EXPECT_EQ(gfx::Rect((480 - 100) / 2, (320 - 100) / 2, 100, 100),
            window->GetNativeWindow()->bounds());
}

// Verifies CenterWindow() constrains to parent size.
TEST_F(NativeWidgetAuraTest, CenterWindowSmallParentNotAtOrigin) {
  // Make a parent window smaller than the host represented by
  // WindowEventDispatcher and offset it slightly from the origin.
  auto parent = std::make_unique<aura::Window>(nullptr);
  parent->Init(ui::LAYER_NOT_DRAWN);
  parent->SetBounds(gfx::Rect(20, 40, 480, 320));
  UniqueWidgetPtr widget = std::make_unique<Widget>();
  NativeWidgetAura* window = Init(parent.get(), widget.get());
  window->CenterWindow(gfx::Size(500, 600));

  // |window| should be no bigger than |parent|.
  EXPECT_EQ("20,40 480x320", window->GetNativeWindow()->bounds().ToString());
}

// View which handles both mouse and gesture events.
class EventHandlingView : public View {
 public:
  EventHandlingView() = default;
  EventHandlingView(const EventHandlingView&) = delete;
  EventHandlingView& operator=(const EventHandlingView&) = delete;
  ~EventHandlingView() override = default;

  // Returns whether an event specified by `type_to_query` has been handled.
  bool HandledEventBefore(ui::EventType type_to_query) const {
    return handled_gestures_set_.find(type_to_query) !=
           handled_gestures_set_.cend();
  }

  // View:
  const char* GetClassName() const override { return "EventHandlingView"; }
  void OnMouseEvent(ui::MouseEvent* event) override { event->SetHandled(); }
  void OnGestureEvent(ui::GestureEvent* event) override {
    // Record the handled gesture event.
    const ui::EventType event_type = event->type();
    if (handled_gestures_set_.find(event_type) ==
        handled_gestures_set_.cend()) {
      EXPECT_TRUE(handled_gestures_set_.insert(event->type()).second);
    } else {
      // Only ET_GESTURE_SCROLL_UPDATE events can be received more than once.
      EXPECT_EQ(ui::ET_GESTURE_SCROLL_UPDATE, event->type());
    }

    event->SetHandled();
  }

 private:
  std::set<ui::EventType> handled_gestures_set_;
};

// Verifies that when the mouse click interrupts the gesture scroll, the view
// where the gesture scroll starts should receive the scroll end event.
TEST_F(NativeWidgetAuraTest, MouseClickInterruptsGestureScroll) {
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params.bounds = gfx::Rect(100, 100);
  UniqueWidgetPtr widget = std::make_unique<Widget>();
  widget->Init(std::move(init_params));
  widget->Show();

  auto* contents_view = widget->SetContentsView(std::make_unique<View>());
  auto* child_view =
      contents_view->AddChildView(std::make_unique<EventHandlingView>());
  child_view->SetBoundsRect(gfx::Rect(gfx::Size{50, 50}));

  auto scroll_callback = [](ui::test::EventGenerator* event_generator,
                            int* step_count, ui::EventType event_type,
                            const gfx::Vector2dF& offset) {
    if (event_type != ui::ET_GESTURE_SCROLL_UPDATE)
      return;

    *step_count -= 1;
    if (*step_count)
      return;

    // Do not interrupt the gesture scroll until the last gesture update event
    // is handled.

    DCHECK_EQ(0, *step_count);
    event_generator->MoveMouseTo(event_generator->current_screen_location());
    event_generator->ClickLeftButton();
  };

  const gfx::Point center_point = child_view->GetBoundsInScreen().CenterPoint();
  gfx::Point target_point = center_point;
  target_point.Offset(0, 20);
  int step_count = 10;
  ui::test::EventGenerator generator(widget->GetNativeView()->GetRootWindow());
  generator.GestureScrollSequenceWithCallback(
      center_point, target_point,
      /*duration=*/base::Milliseconds(100), step_count,
      base::BindRepeating(scroll_callback, &generator, &step_count));

  // Verify that `child_view` receives gesture end events.
  EXPECT_TRUE(child_view->HandledEventBefore(ui::ET_GESTURE_SCROLL_END));
  EXPECT_TRUE(child_view->HandledEventBefore(ui::ET_GESTURE_END));
}

TEST_F(NativeWidgetAuraTest, CreateMinimized) {
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.parent = nullptr;
  params.context = root_window();
  params.show_state = ui::SHOW_STATE_MINIMIZED;
  params.bounds.SetRect(0, 0, 1024, 800);
  UniqueWidgetPtr widget = std::make_unique<Widget>();
  widget->Init(std::move(params));
  widget->Show();

  EXPECT_TRUE(widget->IsMinimized());
}

// Tests that GetRestoreBounds returns the window bounds even if the window is
// transformed.
TEST_F(NativeWidgetAuraTest, RestoreBounds) {
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.parent = nullptr;
  params.context = root_window();
  params.bounds.SetRect(0, 0, 400, 400);
  UniqueWidgetPtr widget = std::make_unique<Widget>();
  widget->Init(std::move(params));
  widget->Show();
  EXPECT_EQ(gfx::Rect(400, 400), widget->GetRestoredBounds());

  gfx::Transform transform;
  transform.Translate(100.f, 100.f);
  widget->GetNativeWindow()->SetTransform(transform);
  EXPECT_EQ(gfx::Rect(400, 400), widget->GetRestoredBounds());
}

TEST_F(NativeWidgetAuraTest, GetWorkspace) {
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.parent = nullptr;
  params.context = root_window();
  params.show_state = ui::SHOW_STATE_MINIMIZED;
  params.bounds.SetRect(0, 0, 1024, 800);
  UniqueWidgetPtr widget = std::make_unique<Widget>();
  widget->Init(std::move(params));
  widget->Show();

  widget->GetNativeWindow()->SetProperty(
      aura::client::kWindowWorkspaceKey,
      aura::client::kWindowWorkspaceUnassignedWorkspace);
  EXPECT_EQ("", widget->GetWorkspace());

  widget->GetNativeWindow()->SetProperty(
      aura::client::kWindowWorkspaceKey,
      aura::client::kWindowWorkspaceVisibleOnAllWorkspaces);
  EXPECT_EQ("", widget->GetWorkspace());

  const int desk_index = 1;
  widget->GetNativeWindow()->SetProperty(aura::client::kWindowWorkspaceKey,
                                         desk_index);
  EXPECT_EQ(base::NumberToString(desk_index), widget->GetWorkspace());
}

// A WindowObserver that counts kShowStateKey property changes.
class TestWindowObserver : public aura::WindowObserver {
 public:
  explicit TestWindowObserver(gfx::NativeWindow window) : window_(window) {
    window_->AddObserver(this);
  }

  TestWindowObserver(const TestWindowObserver&) = delete;
  TestWindowObserver& operator=(const TestWindowObserver&) = delete;

  ~TestWindowObserver() override { window_->RemoveObserver(this); }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key != aura::client::kShowStateKey)
      return;
    count_++;
    state_ = window_->GetProperty(aura::client::kShowStateKey);
  }

  int count() const { return count_; }
  ui::WindowShowState state() const { return state_; }
  void Reset() { count_ = 0; }

 private:
  gfx::NativeWindow window_;
  int count_ = 0;
  ui::WindowShowState state_ = ui::WindowShowState::SHOW_STATE_DEFAULT;
};

// Tests that window transitions from normal to minimized and back do not
// involve extra show state transitions.
TEST_F(NativeWidgetAuraTest, ToggleState) {
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.parent = nullptr;
  params.context = root_window();
  params.show_state = ui::SHOW_STATE_NORMAL;
  params.bounds.SetRect(0, 0, 1024, 800);
  UniqueWidgetPtr widget = std::make_unique<Widget>();
  widget->Init(std::move(params));
  auto observer =
      std::make_unique<TestWindowObserver>(widget->GetNativeWindow());
  widget->Show();
  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_EQ(0, observer->count());
  EXPECT_EQ(ui::WindowShowState::SHOW_STATE_DEFAULT, observer->state());

  widget->Minimize();
  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_EQ(1, observer->count());
  EXPECT_EQ(ui::WindowShowState::SHOW_STATE_MINIMIZED, observer->state());
  observer->Reset();

  widget->Show();
  widget->Restore();
  EXPECT_EQ(1, observer->count());
  EXPECT_EQ(ui::WindowShowState::SHOW_STATE_NORMAL, observer->state());

  observer.reset();
  EXPECT_FALSE(widget->IsMinimized());
}

class TestLayoutManagerBase : public aura::LayoutManager {
 public:
  TestLayoutManagerBase() = default;

  TestLayoutManagerBase(const TestLayoutManagerBase&) = delete;
  TestLayoutManagerBase& operator=(const TestLayoutManagerBase&) = delete;

  ~TestLayoutManagerBase() override = default;

  // aura::LayoutManager:
  void OnWindowResized() override {}
  void OnWindowAddedToLayout(aura::Window* child) override {}
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {}
};

// Used by ShowMaximizedDoesntBounceAround. See it for details.
class MaximizeLayoutManager : public TestLayoutManagerBase {
 public:
  MaximizeLayoutManager() = default;

  MaximizeLayoutManager(const MaximizeLayoutManager&) = delete;
  MaximizeLayoutManager& operator=(const MaximizeLayoutManager&) = delete;

  ~MaximizeLayoutManager() override = default;

 private:
  // aura::LayoutManager:
  void OnWindowAddedToLayout(aura::Window* child) override {
    // This simulates what happens when adding a maximized window.
    SetChildBoundsDirect(child, gfx::Rect(0, 0, 300, 300));
  }
};

// This simulates BrowserView, which creates a custom RootView so that
// OnNativeWidgetSizeChanged that is invoked during Init matters.
class TestWidget : public Widget {
 public:
  TestWidget() = default;

  TestWidget(const TestWidget&) = delete;
  TestWidget& operator=(const TestWidget&) = delete;

  // Returns true if the size changes to a non-empty size, and then to another
  // size.
  bool did_size_change_more_than_once() const {
    return did_size_change_more_than_once_;
  }

  void OnNativeWidgetSizeChanged(const gfx::Size& new_size) override {
    if (last_size_.IsEmpty())
      last_size_ = new_size;
    else if (!did_size_change_more_than_once_ && new_size != last_size_)
      did_size_change_more_than_once_ = true;
    Widget::OnNativeWidgetSizeChanged(new_size);
  }

 private:
  bool did_size_change_more_than_once_ = false;
  gfx::Size last_size_;
};

// Verifies the size of the widget doesn't change more than once during Init if
// the window ends up maximized. This is important as otherwise
// RenderWidgetHostViewAura ends up getting resized during construction, which
// leads to noticable flashes.
TEST_F(NativeWidgetAuraTest, ShowMaximizedDoesntBounceAround) {
  root_window()->SetBounds(gfx::Rect(0, 0, 640, 480));
  root_window()->SetLayoutManager(std::make_unique<MaximizeLayoutManager>());
  UniqueWidgetPtr widget = std::make_unique<TestWidget>();
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.parent = nullptr;
  params.context = root_window();
  params.show_state = ui::SHOW_STATE_MAXIMIZED;
  params.bounds = gfx::Rect(10, 10, 100, 200);
  widget->Init(std::move(params));
  EXPECT_FALSE(
      static_cast<TestWidget*>(widget.get())->did_size_change_more_than_once());
}

class PropertyTestLayoutManager : public TestLayoutManagerBase {
 public:
  PropertyTestLayoutManager() = default;

  PropertyTestLayoutManager(const PropertyTestLayoutManager&) = delete;
  PropertyTestLayoutManager& operator=(const PropertyTestLayoutManager&) =
      delete;

  ~PropertyTestLayoutManager() override = default;

  bool added() const { return added_; }

 private:
  // aura::LayoutManager:
  void OnWindowAddedToLayout(aura::Window* child) override {
    EXPECT_EQ(aura::client::kResizeBehaviorCanResize |
                  aura::client::kResizeBehaviorCanMaximize |
                  aura::client::kResizeBehaviorCanMinimize,
              child->GetProperty(aura::client::kResizeBehaviorKey));
    added_ = true;
  }

  bool added_ = false;
};

// Verifies the resize behavior when added to the layout manager.
TEST_F(NativeWidgetAuraTest, TestPropertiesWhenAddedToLayout) {
  root_window()->SetBounds(gfx::Rect(0, 0, 640, 480));
  PropertyTestLayoutManager* layout_manager = root_window()->SetLayoutManager(
      std::make_unique<PropertyTestLayoutManager>());
  UniqueWidgetPtr widget = std::make_unique<TestWidget>();
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.delegate = new WidgetDelegate();
  params.delegate->SetOwnedByWidget(true);
  params.delegate->SetHasWindowSizeControls(true);
  params.parent = nullptr;
  params.context = root_window();
  widget->Init(std::move(params));
  EXPECT_TRUE(layout_manager->added());
}

TEST_F(NativeWidgetAuraTest, GetClientAreaScreenBounds) {
  // Create a widget.
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.context = root_window();
  params.bounds.SetRect(10, 20, 300, 400);
  UniqueWidgetPtr widget = std::make_unique<Widget>();
  widget->Init(std::move(params));

  // For Aura, client area bounds match window bounds.
  gfx::Rect client_bounds = widget->GetClientAreaBoundsInScreen();
  EXPECT_EQ(10, client_bounds.x());
  EXPECT_EQ(20, client_bounds.y());
  EXPECT_EQ(300, client_bounds.width());
  EXPECT_EQ(400, client_bounds.height());
}

// View subclass that tracks whether it has gotten a gesture event.
class GestureTrackingView : public View {
 public:
  GestureTrackingView() = default;

  GestureTrackingView(const GestureTrackingView&) = delete;
  GestureTrackingView& operator=(const GestureTrackingView&) = delete;

  void set_consume_gesture_event(bool value) { consume_gesture_event_ = value; }

  void clear_got_gesture_event() { got_gesture_event_ = false; }
  bool got_gesture_event() const { return got_gesture_event_; }

  // View overrides:
  void OnGestureEvent(ui::GestureEvent* event) override {
    got_gesture_event_ = true;
    if (consume_gesture_event_)
      event->StopPropagation();
  }

 private:
  // Was OnGestureEvent() invoked?
  bool got_gesture_event_ = false;

  // Dictates what OnGestureEvent() returns.
  bool consume_gesture_event_ = true;
};

// Verifies a capture isn't set on touch press and that the view that gets
// the press gets the release.
TEST_F(NativeWidgetAuraTest, DontCaptureOnGesture) {
  // Create two views (both sized the same). |child| is configured not to
  // consume the gesture event.
  auto content_view = std::make_unique<GestureTrackingView>();
  GestureTrackingView* child = new GestureTrackingView();
  child->set_consume_gesture_event(false);
  content_view->SetLayoutManager(std::make_unique<FillLayout>());
  content_view->AddChildView(child);
  UniqueWidgetPtr widget = std::make_unique<TestWidget>();
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.context = root_window();
  params.bounds = gfx::Rect(0, 0, 100, 200);
  widget->Init(std::move(params));
  GestureTrackingView* view = widget->SetContentsView(std::move(content_view));
  widget->Show();

  ui::TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(41, 51),
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&press);
  ASSERT_FALSE(details.dispatcher_destroyed);
  // Both views should get the press.
  EXPECT_TRUE(view->got_gesture_event());
  EXPECT_TRUE(child->got_gesture_event());
  view->clear_got_gesture_event();
  child->clear_got_gesture_event();
  // Touch events should not automatically grab capture.
  EXPECT_FALSE(widget->HasCapture());

  // Release touch. Only |view| should get the release since that it consumed
  // the press.
  ui::TouchEvent release(ui::ET_TOUCH_RELEASED, gfx::Point(250, 251),
                         ui::EventTimeForNow(),
                         ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  details = GetEventSink()->OnEventFromSource(&release);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_TRUE(view->got_gesture_event());
  EXPECT_FALSE(child->got_gesture_event());
  view->clear_got_gesture_event();
}

// Verifies views with layers are targeted for events properly.
TEST_F(NativeWidgetAuraTest, PreferViewLayersToChildWindows) {
  // Create two widgets: |parent| and |child|. |child| is a child of |parent|.
  UniqueWidgetPtr parent = std::make_unique<Widget>();
  Widget::InitParams parent_params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  parent_params.context = root_window();
  parent->Init(std::move(parent_params));
  View* parent_root = parent->SetContentsView(std::make_unique<View>());
  parent->SetBounds(gfx::Rect(0, 0, 400, 400));
  parent->Show();

  UniqueWidgetPtr child = std::make_unique<Widget>();
  Widget::InitParams child_params(Widget::InitParams::TYPE_CONTROL);
  child_params.parent = parent->GetNativeWindow();
  child->Init(std::move(child_params));
  child->SetBounds(gfx::Rect(0, 0, 200, 200));
  child->Show();

  // Point is over |child|.
  EXPECT_EQ(
      child->GetNativeWindow(),
      parent->GetNativeWindow()->GetEventHandlerForPoint(gfx::Point(50, 50)));

  // Create a view with a layer and stack it at the bottom (below |child|).
  View* view_with_layer = new View;
  parent_root->AddChildView(view_with_layer);
  view_with_layer->SetBounds(0, 0, 50, 50);
  view_with_layer->SetPaintToLayer();

  // Make sure that |child| still gets the event.
  EXPECT_EQ(
      child->GetNativeWindow(),
      parent->GetNativeWindow()->GetEventHandlerForPoint(gfx::Point(20, 20)));

  // Move |view_with_layer| to the top and make sure it gets the
  // event when the point is within |view_with_layer|'s bounds.
  view_with_layer->layer()->parent()->StackAtTop(view_with_layer->layer());
  EXPECT_EQ(
      parent->GetNativeWindow(),
      parent->GetNativeWindow()->GetEventHandlerForPoint(gfx::Point(20, 20)));

  // Point is over |child|, it should get the event.
  EXPECT_EQ(
      child->GetNativeWindow(),
      parent->GetNativeWindow()->GetEventHandlerForPoint(gfx::Point(70, 70)));

  delete view_with_layer;
  view_with_layer = nullptr;

  EXPECT_EQ(
      child->GetNativeWindow(),
      parent->GetNativeWindow()->GetEventHandlerForPoint(gfx::Point(20, 20)));
}

// Verifies views with layers are targeted for events properly.
TEST_F(NativeWidgetAuraTest,
       ShouldDescendIntoChildForEventHandlingChecksVisibleBounds) {
  // Create two widgets: |parent| and |child|. |child| is a child of |parent|.
  UniqueWidgetPtr parent = std::make_unique<Widget>();
  Widget::InitParams parent_params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  parent_params.context = root_window();
  parent->Init(std::move(parent_params));
  View* parent_root_view = parent->SetContentsView(std::make_unique<View>());
  parent->SetBounds(gfx::Rect(0, 0, 400, 400));
  parent->Show();

  UniqueWidgetPtr child = std::make_unique<Widget>();
  Widget::InitParams child_params(Widget::InitParams::TYPE_CONTROL);
  child_params.parent = parent->GetNativeWindow();
  child->Init(std::move(child_params));
  child->SetBounds(gfx::Rect(0, 0, 200, 200));
  child->Show();

  // Point is over |child|.
  EXPECT_EQ(
      child->GetNativeWindow(),
      parent->GetNativeWindow()->GetEventHandlerForPoint(gfx::Point(50, 50)));

  View* parent_root_view_child =
      parent_root_view->AddChildView(std::make_unique<View>());
  parent_root_view_child->SetBounds(0, 0, 10, 10);

  // Create a View whose layer extends outside the bounds of its parent. Event
  // targeting should only consider the visible bounds.
  View* parent_root_view_child_child =
      parent_root_view_child->AddChildView(std::make_unique<View>());
  parent_root_view_child_child->SetBounds(0, 0, 100, 100);
  parent_root_view_child_child->SetPaintToLayer();
  parent_root_view_child_child->layer()->parent()->StackAtTop(
      parent_root_view_child_child->layer());

  // 20,20 is over |parent_root_view_child_child|'s layer, but not the visible
  // bounds of |parent_root_view_child_child|, so |child| should be the event
  // target.
  EXPECT_EQ(
      child->GetNativeWindow(),
      parent->GetNativeWindow()->GetEventHandlerForPoint(gfx::Point(20, 20)));
}

// Verifies views with layers that have SetCanProcessEventWithinSubtree(false)
// set are ignored for event targeting (i.e. the underlying child window can
// still be the target of those events).
TEST_F(
    NativeWidgetAuraTest,
    ShouldDescendIntoChildForEventHandlingIgnoresViewsThatDoNotProcessEvents) {
  // Create two widgets: `parent` and `child`. `child` is a child of `parent`.
  UniqueWidgetPtr parent = std::make_unique<Widget>();
  Widget::InitParams parent_params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  parent_params.context = root_window();
  parent->Init(std::move(parent_params));
  View* const parent_root_view =
      parent->SetContentsView(std::make_unique<View>());
  parent->SetBounds(gfx::Rect(0, 0, 400, 400));
  parent->Show();

  UniqueWidgetPtr child = std::make_unique<Widget>();
  Widget::InitParams child_params(Widget::InitParams::TYPE_CONTROL);
  child_params.parent = parent->GetNativeWindow();
  child->Init(std::move(child_params));
  child->SetBounds(gfx::Rect(0, 0, 200, 200));
  child->Show();

  // Point is over `child`.
  EXPECT_EQ(
      child->GetNativeWindow(),
      parent->GetNativeWindow()->GetEventHandlerForPoint(gfx::Point(50, 50)));

  View* const view_overlapping_child =
      parent_root_view->AddChildView(std::make_unique<View>());
  view_overlapping_child->SetBoundsRect(gfx::Rect(0, 0, 200, 200));
  view_overlapping_child->SetPaintToLayer();
  view_overlapping_child->layer()->parent()->StackAtTop(
      view_overlapping_child->layer());

  // While `view_overlapping_child` receives events, parent should be the event
  // handler as the view is on top of the child widget. This basically is used
  // to verify that the test setup is working (view with layer overlapping child
  // window receives events).
  EXPECT_EQ(
      parent->GetNativeWindow(),
      parent->GetNativeWindow()->GetEventHandlerForPoint(gfx::Point(50, 50)));

  // Events should not be routed to `parent` if the view overlapping `child`
  // does not process events.
  view_overlapping_child->SetCanProcessEventsWithinSubtree(false);
  EXPECT_EQ(
      child->GetNativeWindow(),
      parent->GetNativeWindow()->GetEventHandlerForPoint(gfx::Point(50, 50)));
}

// Verifies that widget->FlashFrame() sets aura::client::kDrawAttentionKey,
// and activating the window clears it.
TEST_F(NativeWidgetAuraTest, FlashFrame) {
  UniqueWidgetPtr widget = std::make_unique<Widget>();
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.context = root_window();
  widget->Init(std::move(params));
  aura::Window* window = widget->GetNativeWindow();
  EXPECT_FALSE(window->GetProperty(aura::client::kDrawAttentionKey));
  widget->FlashFrame(true);
  EXPECT_TRUE(window->GetProperty(aura::client::kDrawAttentionKey));
  widget->FlashFrame(false);
  EXPECT_FALSE(window->GetProperty(aura::client::kDrawAttentionKey));
  widget->FlashFrame(true);
  EXPECT_TRUE(window->GetProperty(aura::client::kDrawAttentionKey));
  widget->Activate();
  EXPECT_FALSE(window->GetProperty(aura::client::kDrawAttentionKey));
}

// Used to track calls to WidgetDelegate::OnWidgetMove().
class MoveTestWidgetDelegate : public WidgetDelegateView {
 public:
  MoveTestWidgetDelegate() = default;

  MoveTestWidgetDelegate(const MoveTestWidgetDelegate&) = delete;
  MoveTestWidgetDelegate& operator=(const MoveTestWidgetDelegate&) = delete;

  ~MoveTestWidgetDelegate() override = default;

  void ClearGotMove() { got_move_ = false; }
  bool got_move() const { return got_move_; }

  // WidgetDelegate overrides:
  void OnWidgetMove() override { got_move_ = true; }

 private:
  bool got_move_ = false;
};

// This test simulates what happens when a window is normally maximized. That
// is, it's layer is acquired for animation then the window is maximized.
// Acquiring the layer resets the bounds of the window. This test verifies the
// Widget is still notified correctly of a move in this case.
TEST_F(NativeWidgetAuraTest, OnWidgetMovedInvokedAfterAcquireLayer) {
  // |delegate| is owned by the Widget by default and is deleted when the widget
  // is destroyed.
  // See WidgetDelegateView::WidgetDelegateView();
  auto delegate = std::make_unique<MoveTestWidgetDelegate>();
  auto* delegate_ptr = delegate.get();
  UniqueWidgetPtr widget = base::WrapUnique(Widget::CreateWindowWithContext(
      std::move(delegate), root_window(), gfx::Rect(10, 10, 100, 200)));
  widget->Show();
  delegate_ptr->ClearGotMove();
  // Simulate a maximize with animation.
  widget->GetNativeView()->RecreateLayer();
  widget->SetBounds(gfx::Rect(0, 0, 500, 500));
  EXPECT_TRUE(delegate_ptr->got_move());
}

// Tests that if a widget has a view which should be initially focused when the
// widget is shown, this view should not get focused if the associated window
// can not be activated.
TEST_F(NativeWidgetAuraTest, PreventFocusOnNonActivableWindow) {
  test_focus_rules()->set_can_activate(false);
  test::TestInitialFocusWidgetDelegate delegate(root_window());
  delegate.GetWidget()->Show();
  EXPECT_FALSE(delegate.view()->HasFocus());

  test_focus_rules()->set_can_activate(true);
  test::TestInitialFocusWidgetDelegate delegate2(root_window());
  delegate2.GetWidget()->Show();
  EXPECT_TRUE(delegate2.view()->HasFocus());
}

// Tests that the transient child bubble window is only visible if the parent is
// visible.
TEST_F(NativeWidgetAuraTest, VisibilityOfChildBubbleWindow) {
  // Create a parent window.
  UniqueWidgetPtr parent = std::make_unique<Widget>();
  Widget::InitParams parent_params(Widget::InitParams::TYPE_WINDOW);
  parent_params.context = root_window();
  parent->Init(std::move(parent_params));
  parent->SetBounds(gfx::Rect(0, 0, 480, 320));

  // Add a child bubble window to the above parent window and show it.
  UniqueWidgetPtr child = std::make_unique<Widget>();
  Widget::InitParams child_params(Widget::InitParams::TYPE_BUBBLE);
  child_params.parent = parent->GetNativeWindow();
  child->Init(std::move(child_params));
  child->SetBounds(gfx::Rect(0, 0, 200, 200));
  child->Show();

  // Check that the bubble window is added as the transient child and it is
  // hidden because parent window is hidden.
  wm::TransientWindowManager* manager =
      wm::TransientWindowManager::GetOrCreate(child->GetNativeWindow());
  EXPECT_EQ(parent->GetNativeWindow(), manager->transient_parent());
  EXPECT_FALSE(parent->IsVisible());
  EXPECT_FALSE(child->IsVisible());

  // Show the parent window should make the transient child bubble visible.
  parent->Show();
  EXPECT_TRUE(parent->IsVisible());
  EXPECT_TRUE(child->IsVisible());
}

// Tests that for a child transient window, if its modal type is
// ui::MODAL_TYPE_WINDOW, then its visibility is controlled by its transient
// parent's visibility.
TEST_F(NativeWidgetAuraTest, TransientChildModalWindowVisibility) {
  // Create a parent window.
  UniqueWidgetPtr parent = std::make_unique<Widget>();
  Widget::InitParams parent_params(Widget::InitParams::TYPE_WINDOW);
  parent_params.context = root_window();
  parent->Init(std::move(parent_params));
  parent->SetBounds(gfx::Rect(0, 0, 400, 400));
  parent->Show();
  EXPECT_TRUE(parent->IsVisible());

  // Create a ui::MODAL_TYPE_WINDOW modal type transient child window.
  UniqueWidgetPtr child = std::make_unique<Widget>();
  Widget::InitParams child_params(Widget::InitParams::TYPE_WINDOW);
  child_params.parent = parent->GetNativeWindow();
  child_params.delegate = new WidgetDelegate;
  child_params.delegate->SetOwnedByWidget(true);
  child_params.delegate->SetModalType(ui::MODAL_TYPE_WINDOW);
  child->Init(std::move(child_params));
  child->SetBounds(gfx::Rect(0, 0, 200, 200));
  child->Show();
  EXPECT_TRUE(parent->IsVisible());
  EXPECT_TRUE(child->IsVisible());

  // Hide the parent window should also hide the child window.
  parent->Hide();
  EXPECT_FALSE(parent->IsVisible());
  EXPECT_FALSE(child->IsVisible());

  // The child window can't be shown if the parent window is hidden.
  child->Show();
  EXPECT_FALSE(parent->IsVisible());
  EXPECT_FALSE(child->IsVisible());

  parent->Show();
  EXPECT_TRUE(parent->IsVisible());
  EXPECT_TRUE(child->IsVisible());
}

// Tests that widgets that are created minimized have the correct restore
// bounds.
TEST_F(NativeWidgetAuraTest, MinimizedWidgetRestoreBounds) {
  const gfx::Rect restore_bounds(300, 300);

  UniqueWidgetPtr widget = std::make_unique<Widget>();
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.context = root_window();
  params.show_state = ui::SHOW_STATE_MINIMIZED;
  params.bounds = restore_bounds;

  widget->Init(std::move(params));
  widget->Show();

  aura::Window* window = widget->GetNativeWindow();
  EXPECT_EQ(ui::SHOW_STATE_MINIMIZED,
            window->GetProperty(aura::client::kShowStateKey));
  EXPECT_EQ(restore_bounds,
            *window->GetProperty(aura::client::kRestoreBoundsKey));

  widget->Restore();
  EXPECT_EQ(restore_bounds, window->bounds());
}

// NativeWidgetAura has a protected destructor.
// Use a test object that overrides the destructor for unit tests.
class TestNativeWidgetAura : public NativeWidgetAura {
 public:
  explicit TestNativeWidgetAura(internal::NativeWidgetDelegate* delegate)
      : NativeWidgetAura(delegate) {}

  TestNativeWidgetAura(const TestNativeWidgetAura&) = delete;
  TestNativeWidgetAura& operator=(const TestNativeWidgetAura&) = delete;

  ~TestNativeWidgetAura() override = default;
};

// Series of tests that verifies having a null NativeWidgetDelegate doesn't
// crash.
class NativeWidgetAuraWithNoDelegateTest : public NativeWidgetAuraTest {
 public:
  NativeWidgetAuraWithNoDelegateTest() = default;

  NativeWidgetAuraWithNoDelegateTest(
      const NativeWidgetAuraWithNoDelegateTest&) = delete;
  NativeWidgetAuraWithNoDelegateTest& operator=(
      const NativeWidgetAuraWithNoDelegateTest&) = delete;

  ~NativeWidgetAuraWithNoDelegateTest() override = default;

  // testing::Test overrides:
  void SetUp() override {
    NativeWidgetAuraTest::SetUp();
    Widget widget;
    native_widget_ = new TestNativeWidgetAura(&widget);
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::CLIENT_OWNS_WIDGET;
    params.native_widget = native_widget_;
    widget.Init(std::move(params));
    widget.Show();
    // Notify all widget observers that the widget is destroying so they can
    // unregister properly and clear the pointer to the widget.
    widget.OnNativeWidgetDestroying();
    // Widget will create a DefaultWidgetDelegate if no delegates are provided.
    // Call Widget::OnNativeWidgetDestroyed() to destroy
    // the WidgetDelegate properly.
    widget.OnNativeWidgetDestroyed();
  }

  void TearDown() override {
    native_widget_->CloseNow();
    ViewsTestBase::TearDown();
  }

  raw_ptr<TestNativeWidgetAura> native_widget_;
};

TEST_F(NativeWidgetAuraWithNoDelegateTest, GetHitTestMaskTest) {
  SkPath mask;
  native_widget_->GetHitTestMask(&mask);
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, GetMaximumSizeTest) {
  native_widget_->GetMaximumSize();
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, GetMinimumSizeTest) {
  native_widget_->GetMinimumSize();
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, GetNonClientComponentTest) {
  native_widget_->GetNonClientComponent(gfx::Point());
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, GetWidgetTest) {
  native_widget_->GetWidget();
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, HasHitTestMaskTest) {
  native_widget_->HasHitTestMask();
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, OnBoundsChangedTest) {
  native_widget_->OnCaptureLost();
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, OnCaptureLostTest) {
  native_widget_->OnBoundsChanged(gfx::Rect(), gfx::Rect());
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, OnGestureEventTest) {
  ui::GestureEvent gesture(0, 0, 0, ui::EventTimeForNow(),
                           ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));
  native_widget_->OnGestureEvent(&gesture);
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, OnKeyEventTest) {
  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_0, ui::EF_NONE);
  native_widget_->OnKeyEvent(&key);
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, OnMouseEventTest) {
  ui::MouseEvent move(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                      ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  native_widget_->OnMouseEvent(&move);
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, OnPaintTest) {
  native_widget_->OnPaint(ui::PaintContext(nullptr, 0, gfx::Rect(), false));
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, OnResizeLoopEndedTest) {
  native_widget_->OnResizeLoopEnded(nullptr);
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, OnResizeLoopStartedTest) {
  native_widget_->OnResizeLoopStarted(nullptr);
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, OnScrollEventTest) {
  ui::ScrollEvent scroll(ui::ET_SCROLL, gfx::Point(), ui::EventTimeForNow(), 0,
                         0, 0, 0, 0, 0);
  native_widget_->OnScrollEvent(&scroll);
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, OnTransientParentChangedTest) {
  native_widget_->OnTransientParentChanged(nullptr);
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, OnWindowAddedToRootWindowTest) {
  native_widget_->OnWindowAddedToRootWindow(nullptr);
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, OnWindowPropertyChangedTest) {
  native_widget_->OnWindowPropertyChanged(nullptr, nullptr, 0);
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, OnWindowRemovingFromRootWindowTest) {
  native_widget_->OnWindowRemovingFromRootWindow(nullptr, nullptr);
}

TEST_F(NativeWidgetAuraWithNoDelegateTest,
       OnWindowTargetVisibilityChangedTest) {
  native_widget_->OnWindowTargetVisibilityChanged(false);
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, OnWindowActivatedTest) {
  native_widget_->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      native_widget_->GetNativeView(), nullptr);
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, OnWindowFocusedTest) {
  native_widget_->OnWindowFocused(nullptr, nullptr);
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, ShouldActivateTest) {
  native_widget_->ShouldActivate();
}

TEST_F(NativeWidgetAuraWithNoDelegateTest,
       ShouldDescendIntoChildForEventHandlingTest) {
  native_widget_->ShouldDescendIntoChildForEventHandling(nullptr, gfx::Point());
}

TEST_F(NativeWidgetAuraWithNoDelegateTest, UpdateVisualStateTest) {
  native_widget_->UpdateVisualState();
}

}  // namespace
}  // namespace views
