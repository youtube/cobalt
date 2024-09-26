// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_EXTENSIONS_WAYLAND_EXTENSION_H_
#define UI_PLATFORM_WINDOW_EXTENSIONS_WAYLAND_EXTENSION_H_

#include "base/component_export.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"

namespace ui {

class PlatformWindow;

enum class WaylandWindowSnapDirection {
  kNone,
  kPrimary,
  kSecondary,
};

enum class WaylandOrientationLockType {
  kAny,
  kNatural,
  kPortrait,
  kLandscape,
  kPortraitPrimary,
  kLandscapePrimary,
  kPortraitSecondary,
  kLandscapeSecondary,
};

class COMPONENT_EXPORT(PLATFORM_WINDOW) WaylandExtension {
 public:
  // Starts a window dragging session from the owning platform window triggered
  // by `event_source` (kMouse or kTouch) if it is not running yet. Under
  // Wayland, window dragging is backed by a platform drag-and-drop session.
  // |allow_system_drag| indicates whether it is allowed to use a regular
  // drag-and-drop session if the compositor does not support the extended-drag
  // protocol needed to implement all window dragging features.
  virtual void StartWindowDraggingSessionIfNeeded(
      ui::mojom::DragEventSource event_source,
      bool allow_system_drag) = 0;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Signals the underneath platform that browser is entering (or exiting)
  // 'immersive fullscreen mode'.
  // Under lacros, it controls for instance interaction with the system shelf
  // widget, when browser goes in fullscreen.
  virtual void SetImmersiveFullscreenStatus(bool status) = 0;
#endif

  // Signals the underneath platform to shows a preview for the given window
  // snap direction. `allow_haptic_feedback` indicates if it should send haptic
  // feedback.
  virtual void ShowSnapPreview(WaylandWindowSnapDirection snap,
                               bool allow_haptic_feedback) = 0;

  // Requests the underneath platform to snap the window in the given direction,
  // if not WaylandWindowSnapDirection::kNone, otherwise cancels the window
  // snapping. `snap_ratio` indicates the width of the work area to snap to in
  // landscape mode, or height in portrait mode.
  virtual void CommitSnap(WaylandWindowSnapDirection snap,
                          float snap_ratio) = 0;

  // Signals the underneath platform whether the current tab of the browser
  // window can go back. The underneath platform might react, for example,
  // by minimizing the window upon a system wide back gesture.
  virtual void SetCanGoBack(bool value) = 0;

  // Requests the underneath platform to set the window to picture-in-picture
  // (PIP).
  virtual void SetPip() = 0;

  // Whether or not the underlying platform supports native pointer locking.
  virtual bool SupportsPointerLock() = 0;
  virtual void LockPointer(bool enabled) = 0;

  // Lock and unlock the window rotation.
  virtual void Lock(WaylandOrientationLockType lock_Type) = 0;
  virtual void Unlock() = 0;

  // Retrieve current layout state.
  virtual bool GetTabletMode() = 0;

  // Signals the underneath platform to float the browser window on top other
  // windows.
  virtual void SetFloat(bool value) = 0;

 protected:
  virtual ~WaylandExtension();

  // Sets the pointer to the extension as a property of the PlatformWindow.
  void SetWaylandExtension(PlatformWindow* window, WaylandExtension* extension);
};

COMPONENT_EXPORT(PLATFORM_WINDOW)
WaylandExtension* GetWaylandExtension(const PlatformWindow& window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_EXTENSIONS_WAYLAND_EXTENSION_H_
