// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_DROP_TARGET_REGISTRY_IMPL_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_DROP_TARGET_REGISTRY_IMPL_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/sessions/drop_target_registry.h"
#include "components/browser_apis/tab_drag/tab_drag_api.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace tabs_api {

class DropTarget;

class DropTargetRegistryImpl : public DropTargetRegistry {
 public:
  DropTargetRegistryImpl();
  DropTargetRegistryImpl(const DropTargetRegistryImpl&) = delete;
  DropTargetRegistryImpl& operator=(const DropTargetRegistryImpl&) = delete;
  ~DropTargetRegistryImpl() override;

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

  base::WeakPtr<DropTargetRegistryImpl> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  size_t drop_targets_count_for_testing() const { return drop_targets_.size(); }

 private:
  std::map<DropTargetId, std::unique_ptr<DropTarget>> drop_targets_;
  DropTargetId::Generator id_generator_;

  base::WeakPtrFactory<DropTargetRegistryImpl> weak_factory_{this};
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_DROP_TARGET_REGISTRY_IMPL_H_
