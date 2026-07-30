// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/drop_target_registry_impl.h"

#include <functional>
#include <memory>

#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "ui/gfx/geometry/rect.h"

namespace tabs_api {

class DropTargetRegistrationMojoImpl : public mojom::DropTargetRegistration {
 public:
  DropTargetRegistrationMojoImpl(
      base::WeakPtr<DropTargetRegistryImpl> registry,
      base::WeakPtr<TabDragWindowAdapter> window_adapter)
      : registry_(registry), window_adapter_(window_adapter) {}

  ~DropTargetRegistrationMojoImpl() override {
    if (registry_ && window_adapter_) {
      registry_->UnregisterDropTarget(window_adapter_.get());
    }
  }

 private:
  base::WeakPtr<DropTargetRegistryImpl> registry_;
  base::WeakPtr<TabDragWindowAdapter> window_adapter_;
};

DropTargetRegistryImpl::DropTargetRegistryImpl() = default;
DropTargetRegistryImpl::~DropTargetRegistryImpl() = default;

void DropTargetRegistryImpl::RegisterDropTarget(
    TabDragWindowAdapter* window_adapter,
    mojo::PendingAssociatedRemote<mojom::DropTarget> target,
    mojo::PendingAssociatedReceiver<mojom::DropTargetRegistration>
        registration) {
  drop_targets_[window_adapter] =
      mojo::AssociatedRemote<mojom::DropTarget>(std::move(target));

  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<DropTargetRegistrationMojoImpl>(
          AsWeakPtr(), window_adapter->AsWeakPtr()),
      std::move(registration));
}

void DropTargetRegistryImpl::UnregisterDropTarget(
    TabDragWindowAdapter* window_adapter) {
  drop_targets_.erase(window_adapter);
}

std::optional<std::reference_wrapper<TabDragWindowAdapter>>
DropTargetRegistryImpl::FindTargetWindow(
    const gfx::Point& screen_point,
    TabDragWindowAdapter* exclude_window) const {
  for (const auto& [window_adapter, drop_target] : drop_targets_) {
    if (window_adapter == exclude_window) {
      continue;
    }
    if (window_adapter->GetBoundsInScreen().Contains(screen_point)) {
      return std::ref(*window_adapter);
    }
  }
  return std::nullopt;
}

std::optional<std::reference_wrapper<mojom::DropTarget>>
DropTargetRegistryImpl::GetDropTarget(
    TabDragWindowAdapter* window_adapter) const {
  auto it = drop_targets_.find(window_adapter);
  if (it != drop_targets_.end()) {
    mojom::DropTarget* target = it->second.get();
    if (target) {
      return std::ref(*target);
    }
  }
  return std::nullopt;
}

}  // namespace tabs_api
