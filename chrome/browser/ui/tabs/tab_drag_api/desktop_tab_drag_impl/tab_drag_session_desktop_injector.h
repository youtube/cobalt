// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_DESKTOP_INJECTOR_H_
#define CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_DESKTOP_INJECTOR_H_

#include <memory>

#include "components/browser_apis/tab_drag/sessions/tab_drag_session_injector.h"

namespace tabs_api {

class TabDragSessionInputAdapter;
class TabDragSessionListener;
class TabDragEventRouter;
class DropTargetRegistry;
class TabDragWindowRegistry;

class TabDragSessionDesktopInjector : public TabDragSessionInjector {
 public:
  TabDragSessionDesktopInjector();
  TabDragSessionDesktopInjector(const TabDragSessionDesktopInjector&&) = delete;
  TabDragSessionDesktopInjector operator=(
      const TabDragSessionDesktopInjector&) = delete;
  ~TabDragSessionDesktopInjector() override;

  // TabDragSessionInjector:
  TabDragWindowRegistry* GetWindowRegistry() override;
  TabDragSessionInputAdapter& GetInputAdapter() override;
  TabDragSessionListener& GetSessionListener() override;
  DropTargetRegistry& GetDropTargetRegistry() override;

 private:
  std::unique_ptr<TabDragWindowRegistry> window_registry_;
  std::unique_ptr<TabDragSessionInputAdapter> adapter_;
  std::unique_ptr<DropTargetRegistry> registry_;
  std::unique_ptr<TabDragEventRouter> event_router_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_SESSION_DESKTOP_INJECTOR_H_
