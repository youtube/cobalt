// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_window_registry.h"

#include "base/check.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"

namespace tabs_api {

TabDragWindowRegistry::TabDragWindowRegistry() = default;
TabDragWindowRegistry::~TabDragWindowRegistry() = default;

TabDragWindowId TabDragWindowRegistry::Register(TabDragWindowAdapter* window) {
  CHECK(window);
  TabDragWindowId id = id_generator_.GenerateNextId();
  windows_[id] = window;
  return id;
}

void TabDragWindowRegistry::Unregister(TabDragWindowId id) {
  windows_.erase(id);
}

TabDragWindowAdapter* TabDragWindowRegistry::Get(TabDragWindowId id) const {
  auto it = windows_.find(id);
  if (it != windows_.end()) {
    return it->second;
  }
  return nullptr;
}

TabDragWindowId TabDragWindowRegistry::FindWindowIdByNativeWindow(
    gfx::NativeWindow native_window) const {
  for (const auto& [id, adapter] : windows_) {
    if (adapter->GetNativeWindow() == native_window) {
      return id;
    }
  }
  return TabDragWindowId();
}

}  // namespace tabs_api
