// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/drop_target_registry_impl.h"

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/testing/toy_drop_target.h"
#include "components/browser_apis/tab_drag/testing/toy_tab_drag_window_adapter.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace tabs_api {

class DropTargetRegistryImplTest : public ::testing::Test {
 protected:
  DropTargetRegistryImplTest() = default;
  ~DropTargetRegistryImplTest() override = default;

  base::test::SingleThreadTaskEnvironment task_environment_;
  DropTargetRegistryImpl registry_;
};

TEST_F(DropTargetRegistryImplTest, RegisterAndUnregister) {
  ToyTabDragWindowAdapter window(gfx::Rect(0, 0, 100, 100));
  ToyDropTarget target;

  mojo::AssociatedRemote<mojom::DropTarget> remote;
  mojo::AssociatedReceiver<mojom::DropTarget> bound_receiver(
      &target, remote.BindNewEndpointAndPassDedicatedReceiver());
  mojo::AssociatedRemote<mojom::DropTargetRegistration> registration;

  registry_.RegisterDropTarget(
      &window, remote.Unbind(),
      registration.BindNewEndpointAndPassDedicatedReceiver());

  EXPECT_EQ(1u, registry_.drop_targets_count_for_testing());
  EXPECT_TRUE(registry_.GetDropTarget(&window).has_value());

  // Unregister via registration destruction
  registration.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return registry_.drop_targets_count_for_testing() == 0u; }));
  EXPECT_FALSE(registry_.GetDropTarget(&window).has_value());
}

TEST_F(DropTargetRegistryImplTest, FindTargetWindow) {
  ToyTabDragWindowAdapter window_a(gfx::Rect(0, 0, 100, 100));
  ToyDropTarget target_a;
  mojo::AssociatedRemote<mojom::DropTarget> remote_a;
  mojo::AssociatedReceiver<mojom::DropTarget> bound_receiver_a(
      &target_a, remote_a.BindNewEndpointAndPassDedicatedReceiver());
  mojo::AssociatedRemote<mojom::DropTargetRegistration> reg_a;
  registry_.RegisterDropTarget(&window_a, remote_a.Unbind(),
                               reg_a.BindNewEndpointAndPassDedicatedReceiver());

  ToyTabDragWindowAdapter window_b(gfx::Rect(200, 0, 100, 100));
  ToyDropTarget target_b;
  mojo::AssociatedRemote<mojom::DropTarget> remote_b;
  mojo::AssociatedReceiver<mojom::DropTarget> bound_receiver_b(
      &target_b, remote_b.BindNewEndpointAndPassDedicatedReceiver());
  mojo::AssociatedRemote<mojom::DropTargetRegistration> reg_b;
  registry_.RegisterDropTarget(&window_b, remote_b.Unbind(),
                               reg_b.BindNewEndpointAndPassDedicatedReceiver());

  // Find inside A
  auto result_a = registry_.FindTargetWindow(gfx::Point(50, 50), nullptr);
  ASSERT_TRUE(result_a.has_value());
  EXPECT_EQ(&window_a, &result_a->get());

  // Find inside B
  auto result_b = registry_.FindTargetWindow(gfx::Point(250, 50), nullptr);
  ASSERT_TRUE(result_b.has_value());
  EXPECT_EQ(&window_b, &result_b->get());

  // Find outside
  EXPECT_FALSE(
      registry_.FindTargetWindow(gfx::Point(150, 50), nullptr).has_value());

  // Find inside A but exclude A
  EXPECT_FALSE(
      registry_.FindTargetWindow(gfx::Point(50, 50), &window_a).has_value());
}

}  // namespace tabs_api
