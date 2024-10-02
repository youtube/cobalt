// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LACROS_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LACROS_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/base/buildflags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

namespace ui {
class WaylandExtension;
class DeskExtension;
class PinnedModeExtension;
}  // namespace ui

namespace views {

class WindowEventFilterLacros;

// Contains Lacros specific implementation.
class VIEWS_EXPORT DesktopWindowTreeHostLacros
    : public DesktopWindowTreeHostPlatform {
 public:
  // Casts from a base WindowTreeHost instance.
  static DesktopWindowTreeHostLacros* From(WindowTreeHost* wth);

  DesktopWindowTreeHostLacros(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura);

  DesktopWindowTreeHostLacros(const DesktopWindowTreeHostLacros&) = delete;
  DesktopWindowTreeHostLacros& operator=(const DesktopWindowTreeHostLacros&) =
      delete;

  ~DesktopWindowTreeHostLacros() override;

  ui::WaylandExtension* GetWaylandExtension();
  const ui::WaylandExtension* GetWaylandExtension() const;

  ui::DeskExtension* GetDeskExtension();
  const ui::DeskExtension* GetDeskExtension() const;

  ui::PinnedModeExtension* GetPinnedModeExtension();
  const ui::PinnedModeExtension* GetPinnedModeExtension() const;

 protected:
  // Overridden from DesktopWindowTreeHost:
  void OnNativeWidgetCreated(const Widget::InitParams& params) override;
  void InitModalType(ui::ModalType modal_type) override;

  // PlatformWindowDelegate:
  void OnClosed() override;
  void OnWindowStateChanged(
      ui::PlatformWindowState old_window_show_state,
      ui::PlatformWindowState new_window_show_state) override;
  void OnImmersiveModeChanged(bool enabled) override;
  void OnTooltipShownOnServer(const std::u16string& text,
                              const gfx::Rect& bounds) override;
  void OnTooltipHiddenOnServer() override;

  // DesktopWindowTreeHostPlatform overrides:
  void AddAdditionalInitProperties(
      const Widget::InitParams& params,
      ui::PlatformWindowInitProperties* properties) override;
  std::unique_ptr<corewm::Tooltip> CreateTooltip() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DesktopWindowTreeHostPlatformImplTestWithTouch,
                           HitTest);

  void CreateNonClientEventFilter();
  void DestroyNonClientEventFilter();

  // A handler for events intended for non client area.
  // A posthandler for events intended for non client area. Handles events if no
  // other consumer handled them.
  std::unique_ptr<WindowEventFilterLacros> non_client_window_event_filter_;

  // The display and the native X window hosting the root window.
  base::WeakPtrFactory<DesktopWindowTreeHostLacros> weak_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LACROS_H_
