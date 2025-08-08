// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_BASE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_BASE_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_base_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace permissions {
enum class PermissionAction;
enum class RequestType;
}  // namespace permissions

class Browser;

constexpr int DISTANCE_BUTTON_VERTICAL = 8;

// Base bubble view that prompts the user to grant or deny a permission request
// from a website. Should not be used directly, instead create one of the more
// specific subclasses.
// ----------------------------------------------
// |                                       [ X ]|
// | Prompt title                               |
// | ------------------------------------------ |
// | Extra text                                 |
// | ------------------------------------------ |
// |                        [ Block ] [ Allow ] |
// ----------------------------------------------
class PermissionPromptBubbleBaseView : public PermissionPromptBaseView {
 public:
  METADATA_HEADER(PermissionPromptBubbleBaseView);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMainViewId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAllowButtonElementId);
  PermissionPromptBubbleBaseView(
      Browser* browser,
      base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
      base::TimeTicks permission_requested_time,
      PermissionPromptStyle prompt_style,
      std::u16string window_title,
      std::u16string accessible_window_title_,
      absl::optional<std::u16string> extra_text);
  PermissionPromptBubbleBaseView(const PermissionPromptBubbleBaseView&) =
      delete;
  PermissionPromptBubbleBaseView& operator=(
      const PermissionPromptBubbleBaseView&) = delete;
  ~PermissionPromptBubbleBaseView() override;

  // Dialog button identifiers used to specify which buttons to show the user.
  enum class PermissionDialogButton {
    kAccept = 0,
    kAcceptOnce = 1,
    kDeny = 2,
    kNum = kDeny,
  };

  virtual void Show();

  // Anchors the bubble to the view or rectangle returned from
  // bubble_anchor_util::GetPageInfoAnchorConfiguration.
  void UpdateAnchorPosition();

  void ShowWidget();

  void SetPromptStyle(PermissionPromptStyle prompt_style);

  void ClosingPermission();

  // views::BubbleDialogDelegateView:
  bool ShouldShowCloseButton() const override;
  std::u16string GetAccessibleWindowTitle() const override;
  std::u16string GetWindowTitle() const override;

  // PermissionPromptBaseView:
  void RunButtonCallback(int button_id) override;

 protected:
  void CreateWidget();

  base::WeakPtr<permissions::PermissionPrompt::Delegate> GetDelegate() {
    return delegate_;
  }

  // Determines whether the current request should also display an
  // "Allow only this time" option in addition to the "Allow on every visit"
  // option.
  static bool IsOneTimePermission(
      permissions::PermissionPrompt::Delegate& delegate);

 private:
  // Record UMA Permissions.*.TimeToDecision.|action| metric. Can be
  // Permissions.Prompt.TimeToDecision.* or Permissions.Chip.TimeToDecision.*,
  // depending on which UI is used.
  void RecordDecision(permissions::PermissionAction action);

  // Convenience methods to convert enum class values to an int used as ViewId
  // and vice-versa.
  static int GetViewId(PermissionDialogButton button) {
    return static_cast<int>(button);
  }
  PermissionDialogButton GetPermissionDialogButton(int button_id) {
    return static_cast<PermissionDialogButton>(button_id);
  }

  const raw_ptr<Browser> browser_;
  base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate_;

  base::TimeTicks permission_requested_time_;

  PermissionPromptStyle prompt_style_;

  const bool is_one_time_permission_;
  const std::u16string accessible_window_title_;
  const std::u16string window_title_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_BASE_VIEW_H_
