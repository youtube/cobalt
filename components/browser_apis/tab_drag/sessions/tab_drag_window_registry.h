// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_WINDOW_REGISTRY_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_WINDOW_REGISTRY_H_

#include <map>

#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"

namespace tabs_api {

class TabDragWindowRegistry {
 public:
  TabDragWindowRegistry();
  ~TabDragWindowRegistry();
  TabDragWindowRegistry(const TabDragWindowRegistry&) = delete;
  TabDragWindowRegistry& operator=(const TabDragWindowRegistry&) = delete;

  // Registers a window and returns a unique ID.
  TabDragWindowId Register(TabDragWindowAdapter* window);

  // Unregisters a window.
  void Unregister(TabDragWindowId id);

  // Retrieves the window associated with the ID, or nullptr if not found.
  TabDragWindowAdapter* Get(TabDragWindowId id) const;

  // Finds the window ID associated with the given native window handle.
  // Returns an invalid ID if not found.
  TabDragWindowId FindWindowIdByNativeWindow(
      gfx::NativeWindow native_window) const;

 private:
  std::map<TabDragWindowId, TabDragWindowAdapter*> windows_;
  TabDragWindowId::Generator id_generator_;
};

}  // namespace tabs_api
#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_WINDOW_REGISTRY_H_
