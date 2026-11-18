// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_session.h"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_injector.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_listener.h"
#include "components/browser_apis/tab_drag/testing/toy_tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/testing/toy_tab_drag_session_listener.h"
#include "components/browser_apis/tab_drag/testing/toy_tab_drag_window_adapter.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace tabs_api {

class TabDragSessionTest : public ::testing::Test {
 protected:
  TabDragSessionTest() : dummy_window_(gfx::Rect(0, 0, 100, 100)) {}
  ~TabDragSessionTest() override = default;

  ToyTabDragWindowAdapter dummy_window_;
};

class ToyDropTargetRegistry : public DropTargetRegistry {
 public:
  void RegisterDropTarget(
      TabDragWindowAdapter* window_adapter,
      mojo::PendingAssociatedRemote<mojom::DropTarget> target,
      mojo::PendingAssociatedReceiver<mojom::DropTargetRegistration>
          registration) override {}
  void UnregisterDropTarget(TabDragWindowAdapter* window_adapter) override {}

  std::optional<std::reference_wrapper<TabDragWindowAdapter>> FindTargetWindow(
      const gfx::Point& screen_point,
      TabDragWindowAdapter* exclude_window) const override {
    return target_window_ ? std::make_optional(std::ref(*target_window_))
                          : std::nullopt;
  }

  std::optional<std::reference_wrapper<mojom::DropTarget>> GetDropTarget(
      TabDragWindowAdapter* window_adapter) const override {
    return std::nullopt;
  }

  void set_target_window(TabDragWindowAdapter* window) {
    target_window_ = window;
  }

 private:
  raw_ptr<TabDragWindowAdapter> target_window_ = nullptr;
};

TEST_F(TabDragSessionTest, StartAndReleaseCapture) {
  ToyTabDragSessionInputAdapter toy_adapter;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry dummy_registry;
  ToyTabDragSessionInjector injector(toy_adapter, listener, dummy_registry);
  base::MockOnceClosure end_callback;
  ToyTabDragWindowAdapter toy_window(gfx::Rect(0, 0, 100, 100));

  EXPECT_FALSE(toy_adapter.capture_started());
  EXPECT_FALSE(toy_adapter.capture_released());
  EXPECT_FALSE(toy_window.HasCapture());

  {
    TabDragSessionParams params{
        .source_window = &toy_window,
        .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
        .start_point = gfx::Point(),
        .end_callback = end_callback.Get()};
    TabDragSession session(std::move(params), &injector);
    EXPECT_FALSE(toy_adapter.capture_started());
    EXPECT_TRUE(session.Start().has_value());
    EXPECT_TRUE(toy_adapter.capture_started());
    EXPECT_FALSE(toy_adapter.capture_released());
    EXPECT_TRUE(toy_window.HasCapture());
  }

  EXPECT_TRUE(toy_adapter.capture_released());
  EXPECT_FALSE(toy_window.HasCapture());
}

TEST_F(TabDragSessionTest, InputEventCancelled) {
  ToyTabDragSessionInputAdapter toy_adapter;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry dummy_registry;
  ToyTabDragSessionInjector injector(toy_adapter, listener, dummy_registry);
  base::MockOnceClosure end_callback;

  TabDragSessionParams params{
      .source_window = &dummy_window_,
      .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
      .start_point = gfx::Point(),
      .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);
  EXPECT_TRUE(session.Start().has_value());

  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kCancelled);
}

TEST_F(TabDragSessionTest, InputEventDropped) {
  ToyTabDragSessionInputAdapter toy_adapter;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry dummy_registry;
  ToyTabDragSessionInjector injector(toy_adapter, listener, dummy_registry);
  base::MockOnceClosure end_callback;

  TabDragSessionParams params{
      .source_window = &dummy_window_,
      .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
      .start_point = gfx::Point(),
      .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);
  EXPECT_TRUE(session.Start().has_value());

  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kDropped);
}

TEST_F(TabDragSessionTest, CoordinateTracking) {
  ToyTabDragSessionInputAdapter toy_adapter;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry dummy_registry;
  ToyTabDragSessionInjector injector(toy_adapter, listener, dummy_registry);
  base::MockOnceClosure end_callback;

  gfx::Point start_point(10, 10);
  TabDragSessionParams params{
      .source_window = &dummy_window_,
      .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
      .start_point = start_point,
      .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);
  EXPECT_TRUE(session.Start().has_value());

  EXPECT_EQ(session.start_point_in_screen(), start_point);
  EXPECT_EQ(session.last_mouse_screen_point(), start_point);
  EXPECT_EQ(session.delta(), gfx::Vector2d(0, 0));

  // Move mouse
  gfx::Point move_point(15, 20);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kMoved, move_point);

  EXPECT_EQ(session.last_mouse_screen_point(), move_point);
  EXPECT_EQ(session.delta(), gfx::Vector2d(5, 10));

  // Drop mouse
  gfx::Point drop_point(25, 30);
  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kDropped, drop_point);

  EXPECT_EQ(session.last_mouse_screen_point(), drop_point);
  EXPECT_EQ(session.delta(), gfx::Vector2d(15, 20));
}

TEST_F(TabDragSessionTest, ListenerNotification) {
  ToyTabDragSessionInputAdapter toy_adapter;
  base::MockOnceClosure end_callback;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry registry;
  ToyTabDragSessionInjector injector(toy_adapter, listener, registry);

  std::vector<tabs_api::NodeId> tab_ids = {
      NodeId(NodeId::Type::kContent, "tab1")};
  TabDragSessionParams params{.source_window = &dummy_window_,
                              .source_tab_ids = tab_ids,
                              .start_point = gfx::Point(),
                              .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);

  ASSERT_EQ(listener.events().size(), 0u);
  EXPECT_TRUE(session.Start().has_value());
  ASSERT_EQ(listener.events().size(), 1u);
  EXPECT_EQ(listener.events()[0].type,
            ToyTabDragSessionListener::Event::Type::kStarted);
  EXPECT_EQ(listener.events()[0].dragged_tabs, tab_ids);
  EXPECT_EQ(listener.events()[0].window, &dummy_window_);

  // Move to a target window
  ToyTabDragWindowAdapter target_window(gfx::Rect(0, 0, 100, 100));
  registry.set_target_window(&target_window);
  gfx::Point move_point1(10, 20);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kMoved, move_point1);

  ASSERT_EQ(listener.events().size(), 2u);
  EXPECT_EQ(listener.events()[1].type,
            ToyTabDragSessionListener::Event::Type::kTargetChanged);
  EXPECT_EQ(listener.events()[1].window, &target_window);
  EXPECT_EQ(listener.events()[1].point, move_point1);

  // Move within the target window
  gfx::Point move_point2(15, 25);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kMoved, move_point2);

  ASSERT_EQ(listener.events().size(), 3u);
  EXPECT_EQ(listener.events()[2].type,
            ToyTabDragSessionListener::Event::Type::kMoved);
  EXPECT_EQ(listener.events()[2].point, move_point2);

  // Drop
  gfx::Point drop_point(30, 40);
  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kDropped, drop_point);

  // Since target didn't change at drop, we just get OnSessionDropped
  ASSERT_EQ(listener.events().size(), 4u);
  EXPECT_EQ(listener.events()[3].type,
            ToyTabDragSessionListener::Event::Type::kDropped);
  EXPECT_EQ(listener.events()[3].point, drop_point);
}

TEST_F(TabDragSessionTest, CaptureLostExternally) {
  ToyTabDragSessionInputAdapter toy_adapter;
  ToyTabDragSessionListener listener;
  ToyDropTargetRegistry registry;
  ToyTabDragSessionInjector injector(toy_adapter, listener, registry);
  base::MockOnceClosure end_callback;
  ToyTabDragWindowAdapter toy_window(gfx::Rect(0, 0, 100, 100));

  TabDragSessionParams params{
      .source_window = &toy_window,
      .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
      .start_point = gfx::Point(),
      .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);
  EXPECT_TRUE(session.Start().has_value());
  EXPECT_TRUE(toy_window.HasCapture());

  // Simulate external capture loss.
  toy_window.ReleaseCapture();
  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kCaptureChanged);
}

}  // namespace tabs_api
