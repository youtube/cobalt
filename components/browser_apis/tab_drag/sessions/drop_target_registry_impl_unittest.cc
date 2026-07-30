// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/drop_target_registry_impl.h"

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/browser_apis/tab_drag/sessions/drop_target.h"
#include "components/browser_apis/tab_drag/sessions/drop_target_registry.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_window_registry.h"
#include "components/browser_apis/tab_drag/testing/toy_drop_target.h"
#include "components/browser_apis/tab_drag/testing/toy_tab_drag_window_adapter.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"

namespace tabs_api {

class DropTargetRegistryImplTest : public ::testing::Test {
 protected:
  DropTargetRegistryImplTest() = default;
  ~DropTargetRegistryImplTest() override = default;

  base::test::SingleThreadTaskEnvironment task_environment_;
  TabDragWindowRegistry window_registry_;
  DropTargetRegistryImpl registry_;
};

TEST_F(DropTargetRegistryImplTest, RegisterAndUnregister) {
  ToyTabDragWindowAdapter window(gfx::Rect(0, 0, 100, 100), &window_registry_);
  ToyDropTarget target;

  mojo::AssociatedRemote<mojom::DropTarget> remote;
  mojo::AssociatedReceiver<mojom::DropTarget> bound_receiver(
      &target, remote.BindNewEndpointAndPassDedicatedReceiver());
  mojo::AssociatedRemote<mojom::DropTargetRegistration> registration;

  DropTargetId id = registry_.RegisterDropTarget(
      &window, gfx::NativeView(), remote.Unbind(),
      registration.BindNewEndpointAndPassDedicatedReceiver());

  EXPECT_EQ(1u, registry_.drop_targets_count_for_testing());
  EXPECT_NE(nullptr, registry_.GetDropTarget(id));

  // Unregister via registration destruction
  registration.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return registry_.drop_targets_count_for_testing() == 0u; }));
  EXPECT_EQ(nullptr, registry_.GetDropTarget(id));
}

TEST_F(DropTargetRegistryImplTest, FindTargetWindow) {
  ToyTabDragWindowAdapter window_a(gfx::Rect(0, 0, 100, 100),
                                   &window_registry_);
  ToyDropTarget target_a;
  mojo::AssociatedRemote<mojom::DropTarget> remote_a;
  mojo::AssociatedReceiver<mojom::DropTarget> bound_receiver_a(
      &target_a, remote_a.BindNewEndpointAndPassDedicatedReceiver());
  mojo::AssociatedRemote<mojom::DropTargetRegistration> reg_a;
  DropTargetId id_a = registry_.RegisterDropTarget(
      &window_a, gfx::NativeView(), remote_a.Unbind(),
      reg_a.BindNewEndpointAndPassDedicatedReceiver());

  ToyTabDragWindowAdapter window_b(gfx::Rect(200, 0, 100, 100),
                                   &window_registry_);
  ToyDropTarget target_b;
  mojo::AssociatedRemote<mojom::DropTarget> remote_b;
  mojo::AssociatedReceiver<mojom::DropTarget> bound_receiver_b(
      &target_b, remote_b.BindNewEndpointAndPassDedicatedReceiver());
  mojo::AssociatedRemote<mojom::DropTargetRegistration> reg_b;
  DropTargetId id_b = registry_.RegisterDropTarget(
      &window_b, gfx::NativeView(), remote_b.Unbind(),
      reg_b.BindNewEndpointAndPassDedicatedReceiver());

  // Push bounds for both targets to support the push model
  reg_a->OnBoundsChanged(gfx::Rect(0, 0, 100, 100));
  reg_b->OnBoundsChanged(gfx::Rect(0, 0, 100, 100));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return registry_.GetCachedBounds(id_a).has_value() &&
           registry_.GetCachedBounds(id_b).has_value();
  }));

  // Find inside A
  auto result_a =
      registry_.FindTargetAtPoint(gfx::Point(50, 50), DropTargetId());
  EXPECT_EQ(id_a, result_a);

  // Find inside B
  auto result_b =
      registry_.FindTargetAtPoint(gfx::Point(250, 50), DropTargetId());
  EXPECT_EQ(id_b, result_b);

  // Find outside
  EXPECT_EQ(DropTargetId(),
            registry_.FindTargetAtPoint(gfx::Point(150, 50), DropTargetId()));

  // Find inside A but exclude A
  EXPECT_EQ(DropTargetId(),
            registry_.FindTargetAtPoint(gfx::Point(50, 50), id_a));

  // Find target for window
  EXPECT_EQ(id_a, registry_.FindTargetForWindow(window_a.GetWindowId()));
  EXPECT_EQ(id_b, registry_.FindTargetForWindow(window_b.GetWindowId()));
  EXPECT_EQ(DropTargetId(), registry_.FindTargetForWindow(TabDragWindowId()));
}

}  // namespace tabs_api
