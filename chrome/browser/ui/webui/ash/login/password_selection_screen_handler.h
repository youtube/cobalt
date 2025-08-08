// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PASSWORD_SELECTION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PASSWORD_SELECTION_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class PasswordSelectionScreen;

// Interface for dependency injection between PasswordSelectionScreen and
// its WebUI representation.
class PasswordSelectionScreenView
    : public base::SupportsWeakPtr<PasswordSelectionScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "password-selection", "PasswordSelectionScreen"};

  virtual ~PasswordSelectionScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  virtual void ShowProgress() = 0;
  virtual void ShowPasswordChoice() = 0;
};

class PasswordSelectionScreenHandler : public PasswordSelectionScreenView,
                                       public BaseScreenHandler {
 public:
  using TView = PasswordSelectionScreenView;

  PasswordSelectionScreenHandler();

  PasswordSelectionScreenHandler(const PasswordSelectionScreenHandler&) =
      delete;
  PasswordSelectionScreenHandler& operator=(
      const PasswordSelectionScreenHandler&) = delete;

  ~PasswordSelectionScreenHandler() override;

 private:
  // PasswordSelectionScreenView
  void Show() override;
  void ShowProgress() override;
  void ShowPasswordChoice() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PASSWORD_SELECTION_SCREEN_HANDLER_H_
