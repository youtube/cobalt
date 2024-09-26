// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_node_info_data_wrapper.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_info_data_wrapper.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_test_util.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_util.h"
#include "chrome/browser/ash/arc/accessibility/ax_tree_source_arc.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/wm_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {
class TestAccessibilityInfoDataWrapper : public AccessibilityInfoDataWrapper {
 public:
  explicit TestAccessibilityInfoDataWrapper(AXTreeSourceArc* tree_source_)
      : AccessibilityInfoDataWrapper(tree_source_) {}

  // AccessibilityInfoDataWrapper overrides:
  bool IsNode() const override { return false; }
  mojom::AccessibilityNodeInfoData* GetNode() const override { return nullptr; }
  mojom::AccessibilityWindowInfoData* GetWindow() const override {
    return nullptr;
  }
  int32_t GetId() const override { return id_; }
  const gfx::Rect GetBounds() const override { return bounds_; }
  bool IsVisibleToUser() const override { return true; }
  bool IsVirtualNode() const override { return false; }
  bool IsIgnored() const override { return false; }
  bool IsImportantInAndroid() const override { return true; }
  bool IsFocusableInFullFocusMode() const override { return true; }
  bool IsAccessibilityFocusableContainer() const override { return true; }
  void PopulateAXRole(ui::AXNodeData* out_data) const override {}
  void PopulateAXState(ui::AXNodeData* out_data) const override {}
  std::string ComputeAXName(bool do_recursive) const override { return ""; }
  void GetChildren(
      std::vector<AccessibilityInfoDataWrapper*>* children) const override {}
  int32_t GetWindowId() const override { return 1; }

  int32_t id_ = 1;
  gfx::Rect bounds_;
};

class TestTreeSource : public AXTreeSourceArc {
 public:
  explicit TestTreeSource(aura::Window* window)
      : AXTreeSourceArc(nullptr, window) {}
  AccessibilityInfoDataWrapper* GetRoot() const override { return root_; }
  raw_ptr<AccessibilityInfoDataWrapper, ExperimentalAsh> root_;
};

}  // namespace

class AccessibilityInfoDataWrapperTest : public ash::AshTestBase {
 public:
  AccessibilityInfoDataWrapperTest() = default;

  std::unique_ptr<exo::WMHelper> wm_helper = std::make_unique<exo::WMHelper>();
};

TEST_F(AccessibilityInfoDataWrapperTest, NonRootNodeBounds) {
  auto shell_surface = exo::test::ShellSurfaceBuilder({200, 200})
                           .SetGeometry(gfx::Rect(10, 10, 200, 200))
                           .BuildClientControlledShellSurface();

  TestTreeSource tree_source(shell_surface->GetWidget()->GetNativeWindow());
  TestAccessibilityInfoDataWrapper root(&tree_source);
  root.bounds_ = gfx::Rect(10, 10, 200, 200);
  tree_source.root_ = &root;

  TestAccessibilityInfoDataWrapper data(&tree_source);
  data.id_ = 2;
  data.bounds_ = gfx::Rect(20, 20, 30, 40);

  ui::AXNodeData out_data;
  data.Serialize(&out_data);

  EXPECT_EQ(gfx::RectF(10, 10, 30, 40), out_data.relative_bounds.bounds);
}

TEST_F(AccessibilityInfoDataWrapperTest, RootNodeBounds) {
  UpdateDisplay("400x400");

  auto shell_surface = exo::test::ShellSurfaceBuilder({200, 200})
                           .SetGeometry(gfx::Rect(10, 10, 200, 200))
                           .BuildClientControlledShellSurface();

  TestTreeSource tree_source(shell_surface->GetWidget()->GetNativeWindow());
  TestAccessibilityInfoDataWrapper data(&tree_source);
  data.bounds_ = gfx::Rect(10, 10, 200, 200);
  tree_source.root_ = &data;

  ui::AXNodeData out_data;
  data.Serialize(&out_data);

  EXPECT_EQ(gfx::RectF(0, 0, 200, 200), out_data.relative_bounds.bounds);
}

TEST_F(AccessibilityInfoDataWrapperTest, RootNodeBoundsOnExternalDisplay) {
  UpdateDisplay("400x400,500x500");

  auto shell_surface = exo::test::ShellSurfaceBuilder({200, 200})
                           .SetGeometry(gfx::Rect(410, 10, 200, 200))
                           .BuildClientControlledShellSurface();

  TestTreeSource tree_source(shell_surface->GetWidget()->GetNativeWindow());
  TestAccessibilityInfoDataWrapper data(&tree_source);
  data.bounds_ = gfx::Rect(10, 10, 200, 200);
  tree_source.root_ = &data;

  ui::AXNodeData out_data;
  data.Serialize(&out_data);

  EXPECT_EQ(gfx::RectF(0, 0, 200, 200), out_data.relative_bounds.bounds);
}

TEST_F(AccessibilityInfoDataWrapperTest, BoundsScalingPiArc) {
  UpdateDisplay("400x400*2");  // 2x device scale factor.

  // With default_scale_cancellation, Android has default (1x) scale factor.
  wm_helper->SetDefaultScaleCancellation(true);
  auto shell_surface = exo::test::ShellSurfaceBuilder()
                           .SetGeometry(gfx::Rect(10, 10, 100, 100))
                           .EnableDefaultScaleCancellation()
                           .BuildClientControlledShellSurface();

  TestTreeSource tree_source(shell_surface->GetWidget()->GetNativeWindow());
  TestAccessibilityInfoDataWrapper data(&tree_source);
  data.bounds_ = gfx::Rect(10, 10, 100, 100);  // PX in Android.
  tree_source.root_ = &data;

  ui::AXNodeData out_data;
  data.Serialize(&out_data);

  EXPECT_EQ(gfx::RectF(0, 0, 200, 200), out_data.relative_bounds.bounds);
}

TEST_F(AccessibilityInfoDataWrapperTest, BoundsScalingFromRvcArcAndLater) {
  UpdateDisplay("400x400*2");  // 2x device scale factor.

  // Without default_scale_cancellation, Android use the same (2x) scale factor.
  wm_helper->SetDefaultScaleCancellation(false);
  auto shell_surface = exo::test::ShellSurfaceBuilder({200, 200})
                           .SetGeometry(gfx::Rect(10, 10, 200, 200))
                           .BuildClientControlledShellSurface();

  TestTreeSource tree_source(shell_surface->GetWidget()->GetNativeWindow());
  TestAccessibilityInfoDataWrapper data(&tree_source);
  data.bounds_ = gfx::Rect(20, 20, 200, 200);  // PX in Android.
  tree_source.root_ = &data;

  ui::AXNodeData out_data;
  data.Serialize(&out_data);

  EXPECT_EQ(gfx::RectF(0, 0, 200, 200), out_data.relative_bounds.bounds);
}

}  // namespace arc
