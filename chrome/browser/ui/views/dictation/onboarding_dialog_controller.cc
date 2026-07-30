// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/onboarding_dialog_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/grit/branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/view_class_properties.h"

namespace dictation {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kDictationOnboardingDialogElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kDictationOnboardingOkButtonElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kDictationOnboardingCancelButtonElementId);

namespace {
inline constexpr char kOnboardingDialogName[] = "DictationOnboardingDialog";
}

OnboardingDialogController::OnboardingDialogController(tabs::TabInterface& tab)
    : tab_(tab) {}

OnboardingDialogController::~OnboardingDialogController() {
  if (IsShowing()) {
    Close(views::Widget::ClosedReason::kUnspecified);
  }
}

void OnboardingDialogController::Show(base::OnceClosure complete_callback,
                                      base::OnceClosure close_callback) {
  if (IsShowing()) {
    return;
  }
  close_callback_ = std::move(close_callback);

  // The widget will own `model_host` through DialogDelegate.
  views::BubbleDialogModelHost* model_host =
      views::BubbleDialogModelHost::CreateModal(
          CreateDialogModel(std::move(complete_callback)),
          ui::mojom::ModalType::kChild)
          .release();
  model_host->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  widget_ = tab_->GetTabFeatures()->tab_dialog_manager()->CreateAndShowDialog(
      model_host, std::make_unique<tabs::TabDialogManager::Params>());

  widget_->MakeCloseSynchronous(base::BindOnce(
      &OnboardingDialogController::Close, base::Unretained(this)));
}

bool OnboardingDialogController::IsShowing() const {
  return widget_ && !widget_->IsClosed();
}

std::unique_ptr<ui::DialogModel> OnboardingDialogController::CreateDialogModel(
    base::OnceClosure complete_callback) {
  // TODO(crbug.com/525857719): Temporary placeholder strings.
  return ui::DialogModel::Builder()
      .SetInternalName(kOnboardingDialogName)
      .SetTitle(l10n_util::GetStringUTF16(IDS_DICTATION_ONBOARDING_TITLE))
      .SetElementIdentifier(kDictationOnboardingDialogElementId)
      .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
          IDS_DICTATION_ONBOARDING_BULLET_MICROPHONE)))
      .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
          IDS_DICTATION_ONBOARDING_BULLET_DATA_SHARING)))
      .AddOkButton(base::BindOnce(&OnboardingDialogController::OnDialogAccepted,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(complete_callback)),
                   ui::DialogModel::Button::Params()
                       .SetLabel(l10n_util::GetStringUTF16(
                           IDS_DICTATION_ONBOARDING_BUTTON_ACCEPT))
                       .SetId(kDictationOnboardingOkButtonElementId)
                       .SetStyle(ui::ButtonStyle::kProminent))
      .AddCancelButton(base::DoNothing(),
                       ui::DialogModel::Button::Params()
                           .SetLabel(l10n_util::GetStringUTF16(IDS_NO_THANKS))
                           .SetId(kDictationOnboardingCancelButtonElementId)
                           .SetStyle(ui::ButtonStyle::kTonal))
      .Build();
}

void OnboardingDialogController::OnDialogAccepted(
    base::OnceClosure complete_callback) {
  std::move(complete_callback).Run();
  Close(views::Widget::ClosedReason::kAcceptButtonClicked);
  // WARNING: `this` is deleted
}

void OnboardingDialogController::Close(views::Widget::ClosedReason reason) {
  widget_.reset();
  if (close_callback_) {
    std::move(close_callback_).Run();
  }
  // WARNING: close_callback_ above deletes `this`.
}

}  // namespace dictation
