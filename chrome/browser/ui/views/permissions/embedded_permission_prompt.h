// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_base_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_desktop.h"
#include "components/permissions/permission_prompt.h"

class Browser;

namespace content {
class WebContents;
}

class EmbeddedPermissionPrompt
    : public PermissionPromptDesktop,
      public EmbeddedPermissionPromptBaseView::Delegate {
 public:
  EmbeddedPermissionPrompt(Browser* browser,
                           content::WebContents* web_contents,
                           permissions::PermissionPrompt::Delegate* delegate);
  ~EmbeddedPermissionPrompt() override;
  EmbeddedPermissionPrompt(const EmbeddedPermissionPrompt&) = delete;
  EmbeddedPermissionPrompt& operator=(const EmbeddedPermissionPrompt&) = delete;

  // Prompt views shown after the user clicks on the embedded permission prompt.
  // The values represent the priority of each variant, higher number means
  // higher priority.
  enum class Variant {
    // Default when conditions are not met to show any of the permission views.
    kUninitialized = 0,
    // Informs the user that the permission was allowed by their administrator.
    kAdministratorGranted = 1,
    // Permission prompt that informs the user they already granted permission.
    // Offers additional options to modify the permission decision.
    kPreviouslyGranted = 2,
    // Informs the user that they need to go to OS system settings to grant
    // access to Chrome.
    kOsSystemSettings = 3,
    // Informs the user that Chrome needs permission from the OS level, in order
    // for the site to be able to access a permission.
    kOsPrompt = 4,
    // Permission prompt that asks the user for site-level permission.
    kAsk = 5,
    // Permission prompt that additionally informs the user that they have
    // previously denied permission to the site. May offer different options
    // (buttons) to the site-level prompt |kAsk|.
    kPreviouslyDenied = 6,
    // Informs the user that the permission was denied by their administrator.
    kAdministratorDenied = 7,
  };

  void CloseCurrentViewAndMaybeShowNext(bool first_prompt);

  void CloseView();

  // permissions::PermissionPrompt:
  TabSwitchingBehavior GetTabSwitchingBehavior() override;
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;
  bool ShouldFinalizeRequestAfterDecided() const override;

  // EmbeddedPermissionPromptBaseView::Delegate
  void Allow() override;
  void AllowThisTime() override;
  void Dismiss() override;
  void Acknowledge() override;
  void StopAllowing() override;
  base::WeakPtr<permissions::PermissionPrompt::Delegate>
  GetPermissionPromptDelegate() const override;

 private:
  static Variant DeterminePromptVariant(
      ContentSetting setting,
      const content_settings::SettingInfo& info);

  Variant embedded_prompt_variant_ = Variant::kUninitialized;
  raw_ptr<EmbeddedPermissionPromptBaseView> prompt_view_;

  raw_ptr<permissions::PermissionPrompt::Delegate> delegate_;

  base::WeakPtrFactory<EmbeddedPermissionPrompt> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_H_
