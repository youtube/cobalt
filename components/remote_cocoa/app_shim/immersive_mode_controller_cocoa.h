// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_CONTROLLER_COCOA_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_CONTROLLER_COCOA_H_

#import <AppKit/AppKit.h>

#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom-shared.h"

@class ClearTitlebarViewController;
@class ImmersiveModeMapper;
@class ImmersiveModeTitlebarObserver;

namespace remote_cocoa {
class ImmersiveModeControllerCocoa;
}  // namespace remote_cocoa

// Host of the overlay view.
@interface ImmersiveModeTitlebarViewController
    : NSTitlebarAccessoryViewController {
  NSView* __strong _blank_separator_view;
  base::WeakPtr<remote_cocoa::ImmersiveModeControllerCocoa>
      _immersive_mode_controller;
}
@end

namespace gfx {
class Rect;
}

namespace remote_cocoa {

// TODO(mek): This should not be exported and used outside of remote_cocoa. So
// figure out how to restructure code so callers outside of remote_cocoa can
// stop existing.
REMOTE_COCOA_APP_SHIM_EXPORT bool IsNSToolbarFullScreenWindow(NSWindow* window);

// Manages a single fullscreen session.
class REMOTE_COCOA_APP_SHIM_EXPORT ImmersiveModeControllerCocoa {
 public:
  explicit ImmersiveModeControllerCocoa(
      NativeWidgetMacNSWindow* browser_window,
      NativeWidgetMacNSWindow* overlay_window);
  virtual ~ImmersiveModeControllerCocoa();

  // Must be called once and only once after construction. Prevents the side-
  // effects of adding a toolbar accessory from accessing partially constructed
  // objects.
  virtual void Init();
  bool is_initialized() { return initialized_; }

  virtual void FullscreenTransitionCompleted();
  virtual void OnTopViewBoundsChanged(const gfx::Rect& bounds);
  virtual void UpdateToolbarVisibility(mojom::ToolbarVisibilityStyle style);

  // Reveal top chrome leaving it visible until all outstanding calls to
  // RevealLock() are balanced with RevealUnlock(). Reveal locks will persist
  // through calls to UpdateToolbarVisibility(). For example, the current
  // ToolbarVisibilityStyle is set to kAlways and RevealLock() has been called.
  // If ToolbarVisibilityStyle is then changed to kAutohide, top
  // chrome will stay on screen until RevealUnlock() is called. At that point
  // top chrome will autohide.
  virtual void RevealLock();
  virtual void RevealUnlock();
  int reveal_lock_count() { return reveal_lock_count_; }

  // Returns true if the toolbar is visible, either because there are
  // outstanding reveal locks, or the user hovers over the upper border of the
  // screen.
  bool IsToolbarRevealed();

  // Called when the reveal status changes.
  void OnToolbarRevealMaybeChanged();

  // Called when the menu bar reveal status changes.
  void OnMenuBarRevealChanged();

  // Called when the NSTitlebarContainerView frame changes.
  virtual void OnTitlebarFrameDidChange(NSRect frame);

  // Called when a child window is added to the observed windows.
  // `ObserveChildWindows` controls which windows are being observed.
  virtual void OnChildWindowAdded(NSWindow* child);

  // Called when a child window is removed from the observed windows.
  virtual void OnChildWindowRemoved(NSWindow* child);

  // Start observing child windows of `window`.
  void ObserveChildWindows(NSWindow* window);

  // Stop observing child windows of `window`.
  void StopObservingChildWindows(NSWindow* window);

  // Return true if the child window should trigger OnChildWindowAdded and
  // OnChildWindowRemoved events, otherwise return false.
  // Called for browser window children that survive the transition to
  // fullscreen.
  virtual bool ShouldObserveChildWindow(NSWindow* child);

  NSWindow* browser_window();
  NSWindow* overlay_window();
  BridgedContentView* overlay_content_view();

  // Called when `immersive_mode_titlebar_view_controller_`'s view is moved to
  // a different window.
  void ImmersiveModeViewWillMoveToWindow(NSWindow* window);

  // Returns true if kImmersiveFullscreenTabs is being used.
  virtual bool IsTabbed();
  bool IsContentFullscreen();

  ImmersiveModeTitlebarViewController*
  immersive_mode_titlebar_view_controller_for_testing() {
    return immersive_mode_titlebar_view_controller_;
  }

 protected:
  // Used by derived classes to manually set last_used_style_. Typically this is
  // used while a RevealLock is active, allowing for a style change after the
  // last RevealLock has been released.
  void set_last_used_style(mojom::ToolbarVisibilityStyle style) {
    last_used_style_ = style;
  }
  mojom::ToolbarVisibilityStyle last_used_style() { return last_used_style_; }

  // Layout the `window` on top of the `anchor_view`. The `window` will occupy
  // the same place on screen as the `anchor_view`, completely occluding the
  // `anchor_view`. The `window` is clear but needs to overlay the `anchor_view`
  // to handle drag events.
  // If the `anchor_view` is offscreen, the `window` will be moved offscreen.
  void LayoutWindowWithAnchorView(NSWindow* window, NSView* anchor_view);

 private:
  // Get offscreen y origin. Used for moving overlay windows offscreen.
  double GetOffscreenYOrigin();

  // Notify the browser window that the reveal status changes.
  void NotifyBrowserWindowAboutToolbarRevealChanged();

  bool initialized_ = false;

  int reveal_lock_count_ = 0;

  bool is_toolbar_revealed_ = false;

  int menu_bar_height_ = 0;

  mojom::ToolbarVisibilityStyle last_used_style_ =
      mojom::ToolbarVisibilityStyle::kAutohide;

  // Keeps the view controllers hidden until the fullscreen transition is
  // complete.
  bool fullscreen_transition_complete_ = false;

  NativeWidgetMacNSWindow* __weak browser_window_;
  NativeWidgetMacNSWindow* __weak overlay_window_;
  BridgedContentView* __weak overlay_content_view_;

  // A controller for top chrome.
  ImmersiveModeTitlebarViewController* __strong
      immersive_mode_titlebar_view_controller_;

  ImmersiveModeMapper* __strong immersive_mode_mapper_;
  ImmersiveModeTitlebarObserver* __strong immersive_mode_titlebar_observer_;

  // Keeps track of which windows have received titlebar and reveal locks.
  std::set<NSWindow*> window_lock_received_;

  base::WeakPtrFactory<ImmersiveModeControllerCocoa> weak_ptr_factory_;
};

}  // namespace remote_cocoa

// A small class that moves the overlay window along the y axis.
//
// The overlay's content view (top chrome) is not hosted in the overlay window.
// It is moved to the AppKit controlled fullscreen window via the
// NSTitlebarAccessoryViewController API. However the overlay window is still
// important.
//  * It is the parent window for top chrome popups. Moving the overlay window
//  in turn moves the child windows.
//  * Its origin in important for dragging operations.
//
// This class will keep the position of the overlay window in sync with its
// original content (top chrome).
REMOTE_COCOA_APP_SHIM_EXPORT @interface ImmersiveModeTitlebarObserver : NSObject

- (instancetype)initWithController:
                    (base::WeakPtr<remote_cocoa::ImmersiveModeControllerCocoa>)
                        controller
             titlebarContainerView:(NSView*)titlebarContainerView;

@end

// An empty NSView that is also opaque.
@interface OpaqueView : NSView
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_CONTROLLER_COCOA_H_
