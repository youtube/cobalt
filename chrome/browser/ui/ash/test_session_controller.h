// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TEST_SESSION_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_TEST_SESSION_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/public/cpp/session/session_controller.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Test implementation of ash's SessionController interface.
class TestSessionController : public ash::SessionController {
 public:
  TestSessionController();

  TestSessionController(const TestSessionController&) = delete;
  TestSessionController& operator=(const TestSessionController&) = delete;

  ~TestSessionController() override;

  const absl::optional<ash::SessionInfo>& last_session_info() const {
    return last_session_info_;
  }

  base::TimeDelta last_session_length_limit() const {
    return last_session_length_limit_;
  }

  base::Time last_session_start_time() const {
    return last_session_start_time_;
  }

  const absl::optional<ash::UserSession>& last_user_session() const {
    return last_user_session_;
  }

  int update_user_session_count() const { return update_user_session_count_; }

  int lock_animation_complete_call_count() const {
    return lock_animation_complete_call_count_;
  }

  int set_user_session_order_count() const {
    return set_user_session_order_count_;
  }

  void set_is_enterprise_managed(bool is_enterprise_managed) {
    is_enterprise_managed_ = is_enterprise_managed;
  }

  void set_existing_users_count(int existing_users_count) {
    existing_users_count_ = existing_users_count;
  }

  void SetScreenLocked(bool locked);

  // ash::SessionController:
  void SetClient(ash::SessionControllerClient* client) override;
  void SetSessionInfo(const ash::SessionInfo& info) override;
  void UpdateUserSession(const ash::UserSession& user_session) override;
  void SetUserSessionOrder(
      const std::vector<uint32_t>& user_session_order) override;
  void PrepareForLock(PrepareForLockCallback callback) override;
  void StartLock(StartLockCallback callback) override;
  void NotifyChromeLockAnimationsComplete() override;
  void RunUnlockAnimation(RunUnlockAnimationCallback callback) override;
  void NotifyChromeTerminating() override;
  void SetSessionLengthLimit(base::TimeDelta length_limit,
                             base::Time start_time) override;
  void CanSwitchActiveUser(CanSwitchActiveUserCallback callback) override;
  void ShowMultiprofilesIntroDialog(
      ShowMultiprofilesIntroDialogCallback callback) override;
  void ShowTeleportWarningDialog(
      ShowTeleportWarningDialogCallback callback) override;
  void ShowMultiprofilesSessionAbortedDialog(
      const std::string& user_email) override;
  void AddSessionActivationObserverForAccountId(
      const AccountId& account_id,
      ash::SessionActivationObserver* observer) override;
  void RemoveSessionActivationObserverForAccountId(
      const AccountId& account_id,
      ash::SessionActivationObserver* observer) override;
  void AddObserver(ash::SessionObserver* observer) override;
  void RemoveObserver(ash::SessionObserver* observer) override;
  bool IsScreenLocked() const override;
  bool IsEnterpriseManaged() const override;
  absl::optional<int> GetExistingUsersCount() const override;

 private:
  absl::optional<ash::SessionInfo> last_session_info_;
  absl::optional<ash::UserSession> last_user_session_;
  base::TimeDelta last_session_length_limit_;
  base::Time last_session_start_time_;
  int update_user_session_count_ = 0;
  int lock_animation_complete_call_count_ = 0;
  int set_user_session_order_count_ = 0;
  bool is_screen_locked_ = false;
  base::ObserverList<ash::SessionObserver> observers_;
  bool is_enterprise_managed_ = false;
  int existing_users_count_ = 0;
};

#endif  // CHROME_BROWSER_UI_ASH_TEST_SESSION_CONTROLLER_H_
