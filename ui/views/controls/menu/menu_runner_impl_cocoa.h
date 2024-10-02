// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_COCOA_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_COCOA_H_

#include <stdint.h>

#include "base/functional/callback.h"
#import "base/mac/scoped_nsobject.h"
#include "base/time/time.h"
#include "ui/views/controls/menu/menu_runner_impl_interface.h"

@class MenuControllerCocoa;
@class MenuControllerCocoaDelegateImpl;

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

namespace views {
namespace test {
class MenuRunnerCocoaTest;
}
namespace internal {

// A menu runner implementation that uses NSMenu to show a context menu.
class VIEWS_EXPORT MenuRunnerImplCocoa : public MenuRunnerImplInterface {
 public:
  MenuRunnerImplCocoa(ui::MenuModel* menu,
                      base::RepeatingClosure on_menu_closed_callback);

  MenuRunnerImplCocoa(const MenuRunnerImplCocoa&) = delete;
  MenuRunnerImplCocoa& operator=(const MenuRunnerImplCocoa&) = delete;

  bool IsRunning() const override;
  void Release() override;
  void RunMenuAt(
      Widget* parent,
      MenuButtonController* button_controller,
      const gfx::Rect& bounds,
      MenuAnchorPosition anchor,
      int32_t run_types,
      gfx::NativeView native_view_for_gestures,
      absl::optional<gfx::RoundedCornersF> corners = absl::nullopt) override;
  void Cancel() override;
  base::TimeTicks GetClosingEventTime() const override;

 private:
  friend class views::test::MenuRunnerCocoaTest;

  ~MenuRunnerImplCocoa() override;

  // The Cocoa menu controller that this instance is bridging.
  base::scoped_nsobject<MenuControllerCocoa> menu_controller_;

  // The delegate for the |menu_controller_|.
  base::scoped_nsobject<MenuControllerCocoaDelegateImpl> menu_delegate_;

  // Are we in run waiting for it to return?
  bool running_ = false;

  // Set if |running_| and Release() has been invoked.
  bool delete_after_run_ = false;

  // The timestamp of the event which closed the menu - or 0.
  base::TimeTicks closing_event_time_;

  // Invoked before RunMenuAt() returns, except upon a Release().
  base::RepeatingClosure on_menu_closed_callback_;
};

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_COCOA_H_
