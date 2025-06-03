// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_DEBUG_VIEW_H_
#define ASH_LOGIN_UI_LOCK_DEBUG_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/detachable_base/detachable_base_pairing_status.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/lock_screen.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class LabelButton;
}

namespace ash {

class LockContentsView;

namespace mojom {
enum class TrayActionState;
}

// Contains the debug UI row (ie, add user, toggle PIN buttons).
class LockDebugView : public views::View {
 public:
  LockDebugView(mojom::TrayActionState initial_note_action_state,
                LockScreen::ScreenType screen_type);

  LockDebugView(const LockDebugView&) = delete;
  LockDebugView& operator=(const LockDebugView&) = delete;

  ~LockDebugView() override;

  // views::View:
  void Layout() override;
  void OnThemeChanged() override;

  LockContentsView* lock() { return lock_; }

 private:
  class DebugDataDispatcherTransformer;
  class DebugLoginDetachableBaseModel;
  enum class AuthErrorType {
    kFirstUnlockFailed,
    kFirstUnlockFailedCapsLockOn,
    kSecondUnlockFailed,
    kSecondUnlockFailedCapsLockOn,
    kDetachableBaseFailed,
  };

  // Adds or removes users.
  void AddOrRemoveUsersButtonPressed(int delta);

  // Iteratively adds more info to the system info labels to test 7 permutations
  // and then disables the button.
  void AddSystemInfoButtonPressed();

  // Enables/disables auth. This is useful for testing auth failure scenarios on
  // Linux Desktop builds, where the cryptohome dbus stub accepts all passwords
  // as valid.
  void ToggleAuthButtonPressed();

  void AddKioskAppButtonPressed();
  void RemoveKioskAppButtonPressed();
  void ToggleDebugDetachableBaseButtonPressed();
  void CycleDetachableBaseStatusButtonPressed();
  void CycleDetachableBaseIdButtonPressed();

  // Shows or hides warning banner.
  void ToggleWarningBannerButtonPressed();

  void ToggleManagedSessionDisclosureButtonPressed();
  void ShowSecurityCurtainScreenButtonPressed();

  // Updates the last used detachable base.
  void UseDetachableBaseButtonPressed(int index);

  // Converts this user to regular user or public account.
  void TogglePublicAccountButtonPressed(int index);

  // Cycles through the various types of auth error bubbles that can be shown on
  // the login screen.
  void CycleAuthErrorMessage();

  // Rebuilds the debug user column which contains per-user actions.
  void UpdatePerUserActionContainer();
  void UpdatePerUserActionContainerAndLayout();

  // Updates buttons provided in detachable base column, depending on detected
  // detachable base pairing state.
  void UpdateDetachableBaseColumn();

  // Creates a button on the debug row that cannot be focused.
  views::LabelButton* AddButton(const std::string& text,
                                views::Button::PressedCallback callback,
                                views::View* container);

  raw_ptr<LockContentsView, ExperimentalAsh> lock_ = nullptr;

  // Debug container which holds the entire debug UI.
  raw_ptr<views::View, ExperimentalAsh> container_ = nullptr;

  // Container which holds global actions.
  raw_ptr<views::View, ExperimentalAsh> global_action_view_container_ = nullptr;

  // Global add system info button. Reference is needed to dynamically disable.
  raw_ptr<views::LabelButton, ExperimentalAsh> global_action_add_system_info_ =
      nullptr;

  // Global toggle auth button. Reference is needed to update the string.
  raw_ptr<views::LabelButton, ExperimentalAsh> global_action_toggle_auth_ =
      nullptr;

  // Row that contains buttons for debugging detachable base state.
  raw_ptr<views::View, ExperimentalAsh> global_action_detachable_base_group_ =
      nullptr;

  // Container which contains rows of buttons, one row associated with one user.
  // Each button in the row has an id which can be used to identify it. The
  // button also has a tag which identifies which user index the button applies
  // to.
  raw_ptr<views::View, ExperimentalAsh> per_user_action_view_container_ =
      nullptr;

  // Debug dispatcher and cached data for the UI.
  std::unique_ptr<DebugDataDispatcherTransformer> const debug_data_dispatcher_;
  // Reference to the detachable base model passed to (and owned by) lock_.
  raw_ptr<DebugLoginDetachableBaseModel, ExperimentalAsh>
      debug_detachable_base_model_ = nullptr;
  size_t num_system_info_clicks_ = 0u;
  LoginScreenController::ForceFailAuth force_fail_auth_ =
      LoginScreenController::ForceFailAuth::kOff;

  // The next type of authentication error bubble to show when the "Cycle auth
  // error" button is clicked.
  AuthErrorType next_auth_error_type_ = AuthErrorType::kFirstUnlockFailed;

  // True if a warning banner is shown.
  bool is_warning_banner_shown_ = false;

  // True if full management disclosure method for the managed sessions is
  // shown.
  bool is_managed_session_disclosure_shown_ = false;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_DEBUG_VIEW_H_
