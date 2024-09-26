// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_CONTROLLER_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_controller.h"
#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_view.h"
#include "components/autofill/core/browser/autofill_progress_dialog_type.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

enum class AutofillProgressDialogType;

// Implementation of the AutofillProgressDialogController. This class shows a
// progress bar with a cancel button that can be updated to a success state
// (check mark). The controller is destroyed once the view is dismissed.
class AutofillProgressDialogControllerImpl
    : public AutofillProgressDialogController {
 public:
  explicit AutofillProgressDialogControllerImpl(
      content::WebContents* web_contents);

  AutofillProgressDialogControllerImpl(
      const AutofillProgressDialogControllerImpl&) = delete;
  AutofillProgressDialogControllerImpl& operator=(
      const AutofillProgressDialogControllerImpl&) = delete;

  ~AutofillProgressDialogControllerImpl() override;

  // Show a progress dialog for underlying autofill processes. The
  // `autofill_progress_dialog_type` determines the type of the progress dialog
  // and `cancel_callback` is the function to invoke when the cancel button is
  // clicked.
  void ShowDialog(AutofillProgressDialogType autofill_progress_dialog_type,
                  base::OnceClosure cancel_callback);
  void DismissDialog(bool show_confirmation_before_closing);

  // AutofillProgressDialogController.
  void OnDismissed(bool is_canceled_by_user) override;
  const std::u16string GetTitle() override;
  const std::u16string GetCancelButtonLabel() override;
  const std::u16string GetLoadingMessage() override;
  const std::u16string GetConfirmationMessage() override;

  content::WebContents* GetWebContents() override;

  AutofillProgressDialogView* autofill_progress_dialog_view() {
    return autofill_progress_dialog_view_;
  }

 private:
  const raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;

  // View that displays the error dialog.
  raw_ptr<AutofillProgressDialogView> autofill_progress_dialog_view_ = nullptr;

  // Callback function invoked when the cancel button is clicked.
  base::OnceClosure cancel_callback_;

  // The type of the progress dialog that is being displayed.
  AutofillProgressDialogType autofill_progress_dialog_type_ =
      AutofillProgressDialogType::kUnspecified;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_CONTROLLER_IMPL_H_
