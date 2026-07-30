// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_DROP_TARGET_REGISTRY_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_DROP_TARGET_REGISTRY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/sessions/drop_target_id.h"
#include "components/browser_apis/tab_drag/sessions/drop_target_registry.h"

namespace gfx {
class Point;
}

namespace tabs_api {

class DropTarget;
class ToyTabDragWindowAdapter;

class ToyDropTargetRegistry : public DropTargetRegistry {
 public:
  ToyDropTargetRegistry();
  ~ToyDropTargetRegistry() override;

  // DropTargetRegistry:
  DropTargetId RegisterDropTarget(
      TabDragWindowAdapter* window,
      gfx::NativeView native_view,
      mojo::PendingAssociatedRemote<mojom::DropTarget> target,
      mojo::PendingAssociatedReceiver<mojom::DropTargetRegistration>
          registration) override;
  void UnregisterDropTarget(DropTargetId target_id) override;

  DropTargetId FindTargetAtPoint(const gfx::Point& screen_point,
                                 DropTargetId exclude_target) const override;

  DropTargetId FindTargetForWindow(TabDragWindowId window_id) const override;

  DropTarget* GetDropTarget(DropTargetId target_id) const override;

  std::optional<gfx::Rect> GetCachedBounds(
      DropTargetId target_id) const override;
  void UpdateTargetBounds(DropTargetId target_id,
                          const gfx::Rect& bounds) override;

  void set_target_window(ToyTabDragWindowAdapter* window);
  void set_source_window(ToyTabDragWindowAdapter* window);

  DropTargetId target_id() const { return target_id_; }
  DropTargetId source_id() const { return source_id_; }

 private:
  const DropTargetId target_id_;
  const DropTargetId source_id_{2};
  raw_ptr<ToyTabDragWindowAdapter> target_window_ = nullptr;
  raw_ptr<ToyTabDragWindowAdapter> source_window_ = nullptr;
  std::unique_ptr<DropTarget> target_object_;
  std::unique_ptr<DropTarget> source_object_;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_DROP_TARGET_REGISTRY_H_
