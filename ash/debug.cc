// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/debug.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "ash/public/cpp/debug_utils.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "chromeos/ui/wm/debug_util.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/debug_utils.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/views/debug_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace debug {

namespace {

void PrintViewHierarchyForWidget(views::Widget* widget,
                                 std::ostringstream* out) {
  *out << "Host widget:\n";
  views::PrintWidgetInformation(*widget, /*detailed*/ true, out);
  *out << "View hierarchy:\n"
       << views::PrintViewHierarchy(widget->GetRootView());
}

aura::Window* GetToplevelWindowUnderMouse() {
  gfx::Point screen_point = display::Screen::Get()->GetCursorScreenPoint();
  aura::Window* root = window_util::GetRootWindowAt(screen_point);
  if (!root) {
    return nullptr;
  }
  gfx::Point local_point = screen_point;
  ::wm::ConvertPointFromScreen(root, &local_point);
  aura::Window* target = root->GetEventHandlerForPoint(local_point);
  if (target) {
    gfx::Point target_point = local_point;
    aura::Window::ConvertPointToTarget(root, target, &target_point);
    int component =
        target->delegate()
            ? target->delegate()->GetNonClientComponent(target_point)
            : HTCLIENT;
    if (component == HTCLIENT) {
      return ::wm::GetToplevelWindow(target);
    }
  }
  return nullptr;
}

}  // namespace

void PrintLayerHierarchy(std::ostringstream* out) {
  if (aura::Env::GetInstance()->IsMouseButtonDown()) {
    aura::Window* target_window = GetToplevelWindowUnderMouse();
    if (target_window && target_window->layer()) {
      ui::PrintLayerHierarchy(
          target_window->layer(),
          RootWindowController::ForWindow(target_window->GetRootWindow())
              ->GetLastMouseLocationInRoot(),
          /*print_invisible=*/true, out);
      return;
    }
    *out << "Warning: No window under mouse, falling back to default.\n";
  }
  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    ui::Layer* layer = root->layer();
    if (layer) {
      ui::PrintLayerHierarchy(
          layer,
          RootWindowController::ForWindow(root)->GetLastMouseLocationInRoot(),
          /*print_invisible=*/true, out);
    }
  }
}

void PrintViewHierarchy(std::ostringstream* out) {
  if (aura::Env::GetInstance()->IsMouseButtonDown()) {
    aura::Window* target_window = GetToplevelWindowUnderMouse();
    if (target_window) {
      views::Widget* widget =
          views::Widget::GetWidgetForNativeView(target_window);
      if (widget) {
        PrintViewHierarchyForWidget(widget, out);
      }
      return;
    }
    *out << "Warning: No window under mouse, falling back to default.\n";
  }

  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window) {
    return;
  }
  views::Widget* widget = views::Widget::GetWidgetForNativeView(active_window);
  if (widget) {
    PrintViewHierarchyForWidget(widget, out);
  }
}

std::vector<std::string> PrintWindowHierarchy(std::ostringstream* out,
                                              bool scrub_data) {
  if (aura::Env::GetInstance()->IsMouseButtonDown()) {
    aura::Window* target_window = GetToplevelWindowUnderMouse();
    if (target_window) {
      return chromeos::wm::PrintWindowHierarchy(
          aura::Window::Windows{target_window}, scrub_data, out);
    }
    *out << "Warning: No window under mouse, falling back to default.\n";
  }
  return chromeos::wm::PrintWindowHierarchy(Shell::Get()->GetAllRootWindows(),
                                            scrub_data, out);
}

void ToggleShowDebugBorders() {
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  std::unique_ptr<cc::DebugBorderTypes> value;
  for (aura::Window* window : root_windows) {
    ui::Compositor* compositor = window->GetHost()->compositor();
    cc::LayerTreeDebugState state = compositor->GetLayerTreeDebugState();
    if (!value.get())
      value = std::make_unique<cc::DebugBorderTypes>(
          state.show_debug_borders.flip());
    state.show_debug_borders = *value.get();
    compositor->SetLayerTreeDebugState(state);
  }
}

void ToggleShowFpsCounter() {
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  std::unique_ptr<bool> value;
  for (aura::Window* window : root_windows) {
    ui::Compositor* compositor = window->GetHost()->compositor();
    cc::LayerTreeDebugState state = compositor->GetLayerTreeDebugState();
    if (!value.get())
      value = std::make_unique<bool>(!state.show_fps_counter);
    state.show_fps_counter = *value.get();
    compositor->SetLayerTreeDebugState(state);
  }
}

void ToggleShowPaintRects() {
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  std::unique_ptr<bool> value;
  for (aura::Window* window : root_windows) {
    ui::Compositor* compositor = window->GetHost()->compositor();
    cc::LayerTreeDebugState state = compositor->GetLayerTreeDebugState();
    if (!value.get())
      value = std::make_unique<bool>(!state.show_paint_rects);
    state.show_paint_rects = *value.get();
    compositor->SetLayerTreeDebugState(state);
  }
}

}  // namespace debug
}  // namespace ash
