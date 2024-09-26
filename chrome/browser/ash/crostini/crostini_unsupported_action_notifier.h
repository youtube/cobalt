// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_UNSUPPORTED_ACTION_NOTIFIER_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_UNSUPPORTED_ACTION_NOTIFIER_H_

#include <memory>
#include <string>

#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace crostini {

// Notifies the user when they try to do something Crostini doesn't yet support
// e.g. use the virtual keyboard in a crostini app.
// TODO(davidmunro): Emit metrics around how often we're hitting these issues so
// we can prioritise appropriately.
class CrostiniUnsupportedActionNotifier
    : public ash::TabletModeObserver,
      public aura::client::FocusChangeObserver,
      public ash::input_method::InputMethodManager::Observer,
      public ash::KeyboardControllerObserver {
 public:
  // Adapter around external integrations which we can mock out for testing,
  // stateless.
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    virtual bool IsInTabletMode();

    // True if the window which currently has focus is a crostini window,
    // doesn't count the terminal.
    virtual bool IsFocusedWindowCrostini();

    // Gets the descriptor for the currently active input method.
    virtual absl::optional<ash::input_method::InputMethodDescriptor>
    GetCurrentInputMethod();

    // Is the current virtual keyboard visible.
    virtual bool IsVirtualKeyboardVisible();

    // Shows a toast to the user.
    virtual void ShowToast(ash::ToastData toast_data);

    // Gets a human-friendly name for the given input method descriptor
    // in the current display language.
    virtual std::string GetLocalizedDisplayName(
        const ash::input_method::InputMethodDescriptor& descriptor);

    // How long toasts should be displayed for. Timing varies depending on
    // e.g. whether screen magnification is enabled.
    virtual base::TimeDelta ToastTimeout();

    virtual void AddFocusObserver(aura::client::FocusChangeObserver* observer);
    virtual void RemoveFocusObserver(
        aura::client::FocusChangeObserver* observer);
    virtual void AddTabletModeObserver(ash::TabletModeObserver* observer);
    virtual void RemoveTabletModeObserver(ash::TabletModeObserver* observer);
    virtual void AddInputMethodObserver(
        ash::input_method::InputMethodManager::Observer* observer);
    virtual void RemoveInputMethodObserver(
        ash::input_method::InputMethodManager::Observer* observer);
    virtual void AddKeyboardControllerObserver(
        ash::KeyboardControllerObserver* observer);
    virtual void RemoveKeyboardControllerObserver(
        ash::KeyboardControllerObserver* observer);
  };

  CrostiniUnsupportedActionNotifier();
  explicit CrostiniUnsupportedActionNotifier(
      std::unique_ptr<Delegate> delegate);

  CrostiniUnsupportedActionNotifier(const CrostiniUnsupportedActionNotifier&) =
      delete;
  CrostiniUnsupportedActionNotifier& operator=(
      const CrostiniUnsupportedActionNotifier&) = delete;

  ~CrostiniUnsupportedActionNotifier() override;

  // ash::TabletModeObserver:
  void OnTabletModeStarted() override;

  // aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // ash::input_method::InputMethodManager::Observer:
  void InputMethodChanged(ash::input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // ash::KeyboardControllerObserver:
  void OnKeyboardVisibilityChanged(bool visible) override;

  Delegate* get_delegate_for_testing() { return delegate_.get(); }

 private:
  // Checks if the user is trying to use a virtual keyboard with a crostini
  // app and, if so and if they haven't already been notified that it's not
  // supported, notify them.
  void ShowVirtualKeyboardUnsupportedNotifictionIfNeeded();

  // If the user is trying to use an unsupported IME with a crostini app and if
  // they haven't already been notified that it's not supported, notify them.
  // Generally Crostini supports IMEs with 1:1 mappings betweens keys and glyphs
  // e.g. Armenian, and simple combinations like US International, but doesn't
  // support CJK, handwriting, completion, etc.
  void ShowIMEUnsupportedNotificationIfNeeded();

  bool IsIMESupportedByCrostini(
      const ash::input_method::InputMethodDescriptor& method);

  std::unique_ptr<Delegate> delegate_;
  bool virtual_keyboard_unsupported_message_shown_ = false;
  bool ime_unsupported_message_shown_ = false;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_UNSUPPORTED_ACTION_NOTIFIER_H_
