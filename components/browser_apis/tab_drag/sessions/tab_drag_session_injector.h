// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_INJECTOR_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_INJECTOR_H_

namespace tabs_api {

class TabDragSessionInputAdapter;
class TabDragSessionListener;
class TabDragWindowRegistry;
class DropTargetRegistry;

class TabDragSessionInjector {
 public:
  virtual ~TabDragSessionInjector() = default;

  virtual TabDragWindowRegistry* GetWindowRegistry() = 0;
  virtual TabDragSessionInputAdapter& GetInputAdapter() = 0;
  virtual TabDragSessionListener& GetSessionListener() = 0;
  virtual DropTargetRegistry& GetDropTargetRegistry() = 0;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_INJECTOR_H_
