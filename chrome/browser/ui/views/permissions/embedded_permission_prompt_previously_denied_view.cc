// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_previously_denied_view.h"

#include "base/memory/weak_ptr.h"

#include "chrome/browser/ui/url_identity.h"
#include "components/permissions/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

EmbeddedPermissionPromptPreviouslyDeniedView::
    EmbeddedPermissionPromptPreviouslyDeniedView(
        Browser* browser,
        base::WeakPtr<Delegate> delegate)
    : EmbeddedPermissionPromptBaseView(browser, delegate) {}

EmbeddedPermissionPromptPreviouslyDeniedView::
    ~EmbeddedPermissionPromptPreviouslyDeniedView() = default;

std::u16string
EmbeddedPermissionPromptPreviouslyDeniedView::GetAccessibleWindowTitle() const {
  return GetMessageText();
}

std::u16string EmbeddedPermissionPromptPreviouslyDeniedView::GetWindowTitle()
    const {
  return std::u16string();
}

void EmbeddedPermissionPromptPreviouslyDeniedView::RunButtonCallback(
    int button_id) {
  if (!delegate()) {
    return;
  }

  ButtonType button = GetButtonType(button_id);

  if (button == ButtonType::kContinueNotAllowing) {
    // The permission is already denied.
    return;
  }

  if (button == ButtonType::kAllowThisTime) {
    delegate()->AllowThisTime();
  }

  if (button == ButtonType::kAllow) {
    delegate()->Allow();
  }
}

std::vector<
    EmbeddedPermissionPromptPreviouslyDeniedView::RequestLineConfiguration>
EmbeddedPermissionPromptPreviouslyDeniedView::GetRequestLinesConfiguration()
    const {
  return {{/*icon=*/nullptr, GetMessageText()}};
}

std::vector<EmbeddedPermissionPromptPreviouslyDeniedView::ButtonConfiguration>
EmbeddedPermissionPromptPreviouslyDeniedView::GetButtonsConfiguration() const {
  std::vector<ButtonConfiguration> buttons;
  buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_EMBEDDED_PROMPT_CONTINUE_NOT_ALLOWING),
      ButtonType::kContinueNotAllowing, ui::ButtonStyle::kTonal);

  if (base::FeatureList::IsEnabled(permissions::features::kOneTimePermission)) {
    buttons.emplace_back(
        l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_THIS_TIME),
        ButtonType::kAllowThisTime, ui::ButtonStyle::kTonal);
  } else {
    buttons.emplace_back(
        l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_THIS_TIME),
        ButtonType::kAllow, ui::ButtonStyle::kTonal);
  }
  return buttons;
}

std::u16string EmbeddedPermissionPromptPreviouslyDeniedView::GetMessageText()
    const {
  const auto& requests = delegate()->Requests();
  CHECK_GT(requests.size(), 0U);

  std::u16string permission_name;
  if (requests.size() == 2) {
    permission_name = l10n_util::GetStringUTF16(
        IDS_CAMERA_AND_MICROPHONE_PERMISSION_NAME_FRAGMENT);
  } else {
    permission_name = requests[0]->GetPermissionNameTextFragment();
  }

  return l10n_util::GetStringFUTF16(IDS_EMBEDDED_PROMPT_PREVIOUSLY_NOT_ALLOWED,
                                    permission_name,
                                    GetUrlIdentityObject().name);
}
