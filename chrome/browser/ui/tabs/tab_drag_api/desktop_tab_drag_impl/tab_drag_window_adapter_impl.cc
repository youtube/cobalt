// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_window_adapter_impl.h"

#include "base/notimplemented.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_manager.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_window_registry.h"
#include "ui/base/base_window.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

TabDragWindowAdapterImpl::TabDragWindowAdapterImpl(
    BrowserWindowInterface* browser_window)
    : browser_window_(browser_window), registry_(nullptr) {
  if (auto* manager =
          g_browser_process->GetFeatures()->tab_drag_session_manager()) {
    registry_ = manager->GetWindowRegistry();
    id_ = registry_->Register(this);
  }
}

TabDragWindowAdapterImpl::~TabDragWindowAdapterImpl() {
  if (registry_) {
    registry_->Unregister(id_);
  }
}

tabs_api::TabDragWindowId TabDragWindowAdapterImpl::GetWindowId() const {
  return id_;
}

gfx::Rect TabDragWindowAdapterImpl::GetBoundsInScreen() const {
  return browser_window_->GetWindow()->GetBounds();
}

namespace {

// Helper to recursively find the NativeViewHost for a given NativeView.
// Returning nullptr means it is not found.
views::View* FindNativeViewHost(views::View* root,
                                gfx::NativeView native_view) {
  if (auto* host = views::AsViewClass<views::NativeViewHost>(root)) {
    if (host->native_view() == native_view) {
      return host;
    }
  }
  for (views::View* child : root->children()) {
    if (auto* found = FindNativeViewHost(child, native_view)) {
      return found;
    }
  }
  return nullptr;
}

}  // namespace

gfx::Point TabDragWindowAdapterImpl::ConvertScreenPointToLocal(
    gfx::NativeView target_view,
    const gfx::Point& screen_point) const {
  gfx::Point local_point = screen_point;
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_window_->GetWindow()->GetNativeWindow());
  CHECK(widget);
  CHECK(widget->GetRootView());

  views::View* host = FindNativeViewHost(widget->GetRootView(), target_view);
  CHECK(host) << "Target view not found in window hierarchy";

  views::View::ConvertPointFromScreen(host, &local_point);
  return local_point;
}

void TabDragWindowAdapterImpl::SetCapture() {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_window_->GetWindow()->GetNativeWindow());
  if (widget) {
    widget->SetCapture(nullptr);
  }
}

void TabDragWindowAdapterImpl::ReleaseCapture() {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_window_->GetWindow()->GetNativeWindow());
  if (widget) {
    widget->ReleaseCapture();
  }
}

bool TabDragWindowAdapterImpl::HasCapture() const {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_window_->GetWindow()->GetNativeWindow());
  return widget && widget->HasCapture();
}
