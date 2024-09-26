// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/frame_utils.h"

#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/tablet_state.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/hit_test_utils.h"
#include "ui/views/window/non_client_view.h"

namespace chromeos {

using WindowOpacity = views::Widget::InitParams::WindowOpacity;

int FrameBorderNonClientHitTest(views::NonClientFrameView* view,
                                const gfx::Point& point_in_widget) {
  gfx::Rect expanded_bounds = view->bounds();
  int outside_bounds = chromeos::kResizeOutsideBoundsSize;

  if (aura::Env::GetInstance()->is_touch_down())
    outside_bounds *= chromeos::kResizeOutsideBoundsScaleForTouch;
  expanded_bounds.Inset(-outside_bounds);

  if (!expanded_bounds.Contains(point_in_widget))
    return HTNOWHERE;

  // Check the frame first, as we allow a small area overlapping the contents
  // to be used for resize handles.
  views::Widget* widget = view->GetWidget();
  bool in_tablet_mode = chromeos::TabletState::Get()->InTabletMode();
  // Ignore the resize border when maximized or full screen or in (split view)
  // tablet mode.
  const bool has_resize_border =
      !widget->IsMaximized() && !widget->IsFullscreen() && !in_tablet_mode;
  const int resize_border_size =
      has_resize_border ? chromeos::kResizeInsideBoundsSize : 0;

  int frame_component = view->GetHTComponentForFrame(
      point_in_widget, gfx::Insets(resize_border_size),
      chromeos::kResizeAreaCornerSize, chromeos::kResizeAreaCornerSize,
      has_resize_border);
  if (frame_component != HTNOWHERE)
    return frame_component;

  int client_component =
      widget->client_view()->NonClientHitTest(point_in_widget);
  if (client_component != HTNOWHERE)
    return client_component;

  // Check if it intersects with children (frame caption button, back button,
  // etc.).
  int hit_test_component =
      views::GetHitTestComponent(widget->non_client_view(), point_in_widget);
  if (hit_test_component != HTNOWHERE)
    return hit_test_component;

  // Caption is a safe default.
  return HTCAPTION;
}

void ResolveInferredOpacity(views::Widget::InitParams* params) {
  DCHECK_EQ(params->opacity, WindowOpacity::kInferred);
  if (params->type == views::Widget::InitParams::TYPE_WINDOW &&
      params->layer_type == ui::LAYER_TEXTURED) {
    // A framed window may have a rounded corner which requires the
    // window to be transparent. WindowManager controls the actual
    // opaque-ness of the window depending on its window state.
    params->init_properties_container.SetProperty(
        chromeos::kWindowManagerManagesOpacityKey, true);
    params->opacity = WindowOpacity::kTranslucent;
  } else {
    params->opacity = WindowOpacity::kOpaque;
  }
}

bool ShouldUseRestoreFrame(const views::Widget* widget) {
  aura::Window* window = widget->GetNativeWindow();
  // This is true when dragging a maximized window in ash. During this phase,
  // the window should look as if it was restored, but keep its maximized state.
  if (window->GetProperty(chromeos::kFrameRestoreLookKey))
    return true;

  // Maximized and fullscreen windows should use the maximized frame.
  if (widget->IsMaximized() || widget->IsFullscreen())
    return false;

  return true;
}

SnapDirection GetSnapDirectionForWindow(aura::Window* window, bool left_top) {
  const bool is_primary_display_layout = chromeos::IsDisplayLayoutPrimary(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
  if (left_top) {
    return is_primary_display_layout ? SnapDirection::kPrimary
                                     : SnapDirection::kSecondary;
  } else {
    return is_primary_display_layout ? SnapDirection::kSecondary
                                     : SnapDirection::kPrimary;
  }
}

}  // namespace chromeos
