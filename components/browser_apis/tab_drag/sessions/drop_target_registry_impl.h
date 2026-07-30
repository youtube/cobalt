// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_DROP_TARGET_REGISTRY_IMPL_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_DROP_TARGET_REGISTRY_IMPL_H_

#include <functional>
#include <map>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_injector.h"
#include "components/browser_apis/tab_drag/tab_drag_api.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace tabs_api {

class TabDragWindowAdapter;

class DropTargetRegistryImpl : public DropTargetRegistry {
 public:
  DropTargetRegistryImpl();
  DropTargetRegistryImpl(const DropTargetRegistryImpl&) = delete;
  DropTargetRegistryImpl& operator=(const DropTargetRegistryImpl&) = delete;
  ~DropTargetRegistryImpl() override;

  // DropTargetRegistry:
  void RegisterDropTarget(
      TabDragWindowAdapter* window_adapter,
      mojo::PendingAssociatedRemote<mojom::DropTarget> target,
      mojo::PendingAssociatedReceiver<mojom::DropTargetRegistration>
          registration) override;
  void UnregisterDropTarget(TabDragWindowAdapter* window_adapter) override;

  std::optional<std::reference_wrapper<TabDragWindowAdapter>> FindTargetWindow(
      const gfx::Point& screen_point,
      TabDragWindowAdapter* exclude_window) const override;

  std::optional<std::reference_wrapper<mojom::DropTarget>> GetDropTarget(
      TabDragWindowAdapter* window_adapter) const override;

  base::WeakPtr<DropTargetRegistryImpl> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  size_t drop_targets_count_for_testing() const { return drop_targets_.size(); }

 private:
  std::map<TabDragWindowAdapter*, mojo::AssociatedRemote<mojom::DropTarget>>
      drop_targets_;

  base::WeakPtrFactory<DropTargetRegistryImpl> weak_factory_{this};
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_DROP_TARGET_REGISTRY_IMPL_H_
