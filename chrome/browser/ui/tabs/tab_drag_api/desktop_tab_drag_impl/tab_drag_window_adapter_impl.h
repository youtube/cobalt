// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_WINDOW_ADAPTER_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_WINDOW_ADAPTER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "ui/gfx/geometry/rect.h"

class BrowserWindowInterface;

namespace tabs_api {
class TabDragWindowRegistry;
}

class TabDragWindowAdapterImpl : public tabs_api::TabDragWindowAdapter {
 public:
  explicit TabDragWindowAdapterImpl(BrowserWindowInterface* browser_window);
  TabDragWindowAdapterImpl(const TabDragWindowAdapterImpl&) = delete;
  TabDragWindowAdapterImpl& operator=(const TabDragWindowAdapterImpl&) = delete;
  ~TabDragWindowAdapterImpl() override;

  // tabs_api::TabDragWindowAdapter:
  tabs_api::TabDragWindowId GetWindowId() const override;
  gfx::Rect GetBoundsInScreen() const override;
  gfx::Point ConvertScreenPointToLocal(
      gfx::NativeView target_view,
      const gfx::Point& screen_point) const override;

  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;

 private:
  raw_ptr<BrowserWindowInterface> browser_window_;
  raw_ptr<tabs_api::TabDragWindowRegistry> registry_;
  tabs_api::TabDragWindowId id_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_DRAG_API_DESKTOP_TAB_DRAG_IMPL_TAB_DRAG_WINDOW_ADAPTER_IMPL_H_
