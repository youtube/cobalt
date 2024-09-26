// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_controller_impl.h"

#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillProgressDialogControllerImpl::AutofillProgressDialogControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

AutofillProgressDialogControllerImpl::~AutofillProgressDialogControllerImpl() {
  if (autofill_progress_dialog_view_) {
    autofill_progress_dialog_view_->Dismiss(
        /*show_confirmation_before_closing=*/false,
        /*is_canceled_by_user=*/true);
    autofill_progress_dialog_view_ = nullptr;
  }
}

void AutofillProgressDialogControllerImpl::ShowDialog(
    AutofillProgressDialogType autofill_progress_dialog_type,
    base::OnceClosure cancel_callback) {
  DCHECK(!autofill_progress_dialog_view_);

  autofill_progress_dialog_type_ = autofill_progress_dialog_type;
  cancel_callback_ = std::move(cancel_callback);
  autofill_progress_dialog_view_ =
      AutofillProgressDialogView::CreateAndShow(this);

  if (autofill_progress_dialog_view_)
    AutofillMetrics::LogProgressDialogShown(autofill_progress_dialog_type);
}

void AutofillProgressDialogControllerImpl::DismissDialog(
    bool show_confirmation_before_closing) {
  if (!autofill_progress_dialog_view_)
    return;

  autofill_progress_dialog_view_->Dismiss(show_confirmation_before_closing,
                                          /*is_canceled_by_user=*/false);
  autofill_progress_dialog_view_ = nullptr;
}

void AutofillProgressDialogControllerImpl::OnDismissed(
    bool is_canceled_by_user) {
  // Dialog is being dismissed so set the pointer to nullptr.
  autofill_progress_dialog_view_ = nullptr;
  if (is_canceled_by_user)
    std::move(cancel_callback_).Run();
  AutofillMetrics::LogProgressDialogResultMetric(
      is_canceled_by_user, autofill_progress_dialog_type_);
  autofill_progress_dialog_type_ = AutofillProgressDialogType::kUnspecified;
  cancel_callback_.Reset();
}

const std::u16string AutofillProgressDialogControllerImpl::GetTitle() {
  switch (autofill_progress_dialog_type_) {
    case AutofillProgressDialogType::kAndroidFIDOProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_FIDO_AUTHENTICATION_PROMPT_TITLE);
    case AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE_V2);
    case AutofillProgressDialogType::kUnspecified:
      NOTREACHED();
      return std::u16string();
  }
}

const std::u16string
AutofillProgressDialogControllerImpl::GetCancelButtonLabel() {
  switch (autofill_progress_dialog_type_) {
    case AutofillProgressDialogType::kAndroidFIDOProgressDialog:
      return l10n_util::GetStringUTF16(IDS_CANCEL);
    case AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_CANCEL_BUTTON_LABEL);
    case AutofillProgressDialogType::kUnspecified:
      NOTREACHED();
      return std::u16string();
  }
}

const std::u16string AutofillProgressDialogControllerImpl::GetLoadingMessage() {
  switch (autofill_progress_dialog_type_) {
    case AutofillProgressDialogType::kAndroidFIDOProgressDialog:
      return std::u16string();
    case AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROGRESS_BAR_MESSAGE);
    case AutofillProgressDialogType::kUnspecified:
      NOTREACHED();
      return std::u16string();
  }
}

const std::u16string
AutofillProgressDialogControllerImpl::GetConfirmationMessage() {
  switch (autofill_progress_dialog_type_) {
    case AutofillProgressDialogType::kAndroidFIDOProgressDialog:
      return std::u16string();
    case AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_CONFIRMATION_MESSAGE);
    case AutofillProgressDialogType::kUnspecified:
      NOTREACHED();
      return std::u16string();
  }
}

content::WebContents* AutofillProgressDialogControllerImpl::GetWebContents() {
  return web_contents_;
}

}  // namespace autofill
