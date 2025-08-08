// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_POLICY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_POLICY_VIEW_H_

#include <string>

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"

class Browser;

// A view used to display information to the user that the permission is
// controlled by policy and can't be changed.
class EmbeddedPermissionPromptPolicyView
    : public EmbeddedPermissionPromptBaseView {
 public:
  EmbeddedPermissionPromptPolicyView(Browser* browser,
                                     base::WeakPtr<Delegate> delegate,
                                     bool is_permission_allowed);
  EmbeddedPermissionPromptPolicyView(
      const EmbeddedPermissionPromptPolicyView&) = delete;
  EmbeddedPermissionPromptPolicyView& operator=(
      const EmbeddedPermissionPromptPolicyView&) = delete;
  ~EmbeddedPermissionPromptPolicyView() override;

  std::u16string GetAccessibleWindowTitle() const override;
  std::u16string GetWindowTitle() const override;
  void RunButtonCallback(int type) override;

 protected:
  std::vector<RequestLineConfiguration> GetRequestLinesConfiguration()
      const override;
  std::vector<ButtonConfiguration> GetButtonsConfiguration() const override;

 private:
  std::u16string GetMessageText() const;

  // Whether the administrator has "allowed" or "blocked" the particular
  // permission that this prompt is for.
  bool is_permission_allowed_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_POLICY_VIEW_H_
