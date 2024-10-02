// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_H_

#include <stdint.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/views_export.h"

namespace base {
class TimeTicks;
}

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {
class MenuModel;
}

namespace views {

class MenuButtonController;
class MenuItemView;
class MenuRunnerHandler;
class Widget;

namespace internal {
class MenuRunnerImplInterface;
}

namespace test {
class MenuRunnerTestAPI;
}

// MenuRunner is responsible for showing (running) the menu and additionally
// owning the MenuItemView. It is safe to delete MenuRunner at any point, but
// MenuRunner will not notify you of the closure caused by a deletion.
// If MenuRunner is deleted while the menu is showing the delegate of the menu
// is reset. This is done to ensure delegates aren't notified after they may
// have been deleted.
//
// Similarly you should avoid creating MenuRunner on the stack. Doing so means
// MenuRunner may not be immediately destroyed if your object is destroyed,
// resulting in possible callbacks to your now deleted object. Instead you
// should define MenuRunner as a scoped_ptr in your class so that when your
// object is destroyed MenuRunner initiates the proper cleanup and ensures your
// object isn't accessed again.
class VIEWS_EXPORT MenuRunner {
 public:
  enum RunTypes {
    NO_FLAGS = 0,

    // The menu has mnemonics.
    HAS_MNEMONICS = 1 << 0,

    // The menu is a nested context menu. For example, click a folder on the
    // bookmark bar, then right click an entry to get its context menu.
    IS_NESTED = 1 << 1,

    // Used for showing a menu during a drop operation. This does NOT block the
    // caller, instead the delegate is notified when the menu closes via the
    // DropMenuClosed method.
    FOR_DROP = 1 << 2,

    // The menu is a context menu (not necessarily nested), for example right
    // click on a link on a website in the browser.
    CONTEXT_MENU = 1 << 3,

    // The menu should behave like a Windows native Combobox dropdow menu.
    // This behavior includes accepting the pending item and closing on F4.
    COMBOBOX = 1 << 4,

    // A child view is performing a drag-and-drop operation, so the menu should
    // stay open (even if it doesn't receive drag updated events). In this case,
    // the caller is responsible for closing the menu upon completion of the
    // drag-and-drop.
    NESTED_DRAG = 1 << 5,

    // Menu with fixed anchor position, so |MenuRunner| will not attempt to
    // adjust the anchor point. For example the context menu of shelf item.
    FIXED_ANCHOR = 1 << 6,

    // The menu's owner could be in the middle of a gesture when the menu opens
    // and can use this flag to continue the gesture. For example, Chrome OS's
    // shelf uses the flag to continue dragging an item without lifting the
    // finger after the context menu of the item is opened.
    SEND_GESTURE_EVENTS_TO_OWNER = 1 << 7,

    // Applying the system UI specific layout to the context menu. This should
    // be set for the context menu inside system UI only (e.g, the context menu
    // while right clicking the wallpaper of a ChromeBook). Thus, this can be
    // used to differentiate the context menu inside system UI from others. It
    // is useful if we want to customize some attributes of the context menu
    // inside system UI, e.g, colors.
    USE_ASH_SYS_UI_LAYOUT = 1 << 8,

    // Similar to COMBOBOX, but does not capture the mouse and lets some keys
    // propagate back to the parent so the combobox content can be edited even
    // while the menu is open.
    EDITABLE_COMBOBOX = 1 << 9,

    // Indicates that the menu should show mnemonics.
    SHOULD_SHOW_MNEMONICS = 1 << 10,
  };

  // Creates a new MenuRunner, which may use a native menu if available.
  // |run_types| is a bitmask of RunTypes. If provided,
  // |on_menu_closed_callback| is invoked when the menu is closed.
  // The MenuModelDelegate of |menu_model| will be overwritten by this call.
  MenuRunner(ui::MenuModel* menu_model,
             int32_t run_types,
             base::RepeatingClosure on_menu_closed_callback =
                 base::RepeatingClosure());

  // Creates a runner for a custom-created toolkit-views menu.
  MenuRunner(MenuItemView* menu, int32_t run_types);

  MenuRunner(const MenuRunner&) = delete;
  MenuRunner& operator=(const MenuRunner&) = delete;

  ~MenuRunner();

  // Runs the menu. MenuDelegate::OnMenuClosed will be notified of the results.
  // If `anchor` uses a `BUBBLE_..` type, the bounds will get determined by
  // using `bounds` as the thing to point at in screen coordinates.
  // `native_view_for_gestures` is a NativeView that is used for cases where the
  // surface hosting the menu has a different gfx::NativeView than the `parent`.
  // This is required to correctly route gesture events to the correct
  // NativeView in the cases where the surface hosting the menu is a
  // WebContents.
  // `corners` assigns the customized `RoundedCornersF` to the context menu. If
  // it's set, the passed in corner will be used to render each corner radius of
  // the context menu. This only works when using `USE_ASH_SYS_UI_LAYOUT`.
  // Note that this is a blocking call for a native menu on Mac. See
  // http://crbug.com/682544.
  void RunMenuAt(Widget* parent,
                 MenuButtonController* button_controller,
                 const gfx::Rect& bounds,
                 MenuAnchorPosition anchor,
                 ui::MenuSourceType source_type,
                 gfx::NativeView native_view_for_gestures = nullptr,
                 absl::optional<gfx::RoundedCornersF> corners = absl::nullopt);

  // Returns true if we're in a nested run loop running the menu.
  bool IsRunning() const;

  // Hides and cancels the menu. This does nothing if the menu is not open.
  void Cancel();

  // Returns the time from the event which closed the menu - or 0.
  base::TimeTicks closing_event_time() const;

 private:
  friend class test::MenuRunnerTestAPI;

  // Sets an implementation of RunMenuAt. This is intended to be used at test.
  void SetRunnerHandler(std::unique_ptr<MenuRunnerHandler> runner_handler);

  const int32_t run_types_;

  // We own this. No scoped_ptr because it is destroyed by calling Release().
  raw_ptr<internal::MenuRunnerImplInterface, DanglingUntriaged> impl_;

  // An implementation of RunMenuAt. This is usually NULL and ignored. If this
  // is not NULL, this implementation will be used.
  std::unique_ptr<MenuRunnerHandler> runner_handler_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_H_
