// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_WRAPPER_IMPL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_WRAPPER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/host/shell_popup_wrapper.h"

namespace ui {

class XDGSurfaceWrapperImpl;
class WaylandConnection;
class WaylandWindow;

// Popup wrapper for xdg-shell stable
class XDGPopupWrapperImpl : public ShellPopupWrapper {
 public:
  XDGPopupWrapperImpl(std::unique_ptr<XDGSurfaceWrapperImpl> surface,
                      WaylandWindow* wayland_window,
                      WaylandConnection* connection);

  XDGPopupWrapperImpl(const XDGPopupWrapperImpl&) = delete;
  XDGPopupWrapperImpl& operator=(const XDGPopupWrapperImpl&) = delete;

  ~XDGPopupWrapperImpl() override;

  // ShellPopupWrapper overrides:
  bool Initialize(const ShellPopupParams& params) override;
  void AckConfigure(uint32_t serial) override;
  bool IsConfigured() override;
  bool SetBounds(const gfx::Rect& new_bounds) override;
  void SetWindowGeometry(const gfx::Rect& bounds) override;
  void Grab(uint32_t serial) override;
  bool SupportsDecoration() override;
  void Decorate() override;
  void SetScaleFactor(float scale_factor) override;
  XDGPopupWrapperImpl* AsXDGPopupWrapper() override;

 private:
  wl::Object<xdg_positioner> CreatePositioner();

  // xdg_popup_listener
  static void Configure(void* data,
                              struct xdg_popup* xdg_popup,
                              int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height);
  static void PopupDone(void* data, struct xdg_popup* xdg_popup);
  static void Repositioned(void* data,
                           struct xdg_popup* xdg_popup,
                           uint32_t token);

  XDGSurfaceWrapperImpl* xdg_surface_wrapper() const;

  // Non-owned WaylandWindow that uses this popup.
  const raw_ptr<WaylandWindow> wayland_window_;
  const raw_ptr<WaylandConnection> connection_;

  // Ground surface for this popup.
  std::unique_ptr<XDGSurfaceWrapperImpl> xdg_surface_wrapper_;

  // XDG Shell Stable object.
  wl::Object<xdg_popup> xdg_popup_;

  // Aura shell popup object
  wl::Object<zaura_popup> aura_popup_;

  // Parameters that help to configure this popup.
  ShellPopupParams params_;

  uint32_t next_reposition_token_ = 1;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_WRAPPER_IMPL_H_
