// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_RECOVERY_ELIGIBILITY_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_RECOVERY_ELIGIBILITY_SCREEN_H_

#include "chrome/browser/ash/login/screens/base_screen.h"

#include "base/functional/callback.h"

namespace ash {

// This pseudo-screen checks if user is eligible for setting up recovery factor
// and sets up relevant data in WizardContext for Consolidated TOS screen.
// This screen has no UI, it just encapsulates decision logic.
class RecoveryEligibilityScreen : public BaseScreen {
 public:
  enum class Result { PROCEED, NOT_APPLICABLE };
  static std::string GetResultString(Result result);
  static bool ShouldSkipRecoverySetupBecauseOfPolicy();

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  explicit RecoveryEligibilityScreen(const ScreenExitCallback& exit_callback);
  ~RecoveryEligibilityScreen() override;

  ScreenExitCallback get_exit_callback_for_testing() { return exit_callback_; }
  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  bool MaybeSkip(WizardContext& context) override;

  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_RECOVERY_ELIGIBILITY_SCREEN_H_
