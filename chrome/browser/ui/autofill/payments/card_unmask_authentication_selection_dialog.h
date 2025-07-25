// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_H_

namespace content {
class WebContents;
}

namespace autofill {

class CardUnmaskAuthenticationSelectionDialogController;

// Interface that exposes the dialog to
// CardUnmaskAuthenticationSelectionDialogControllerImpl.
class CardUnmaskAuthenticationSelectionDialog {
 public:
  // Creates a dialog and displays it as a modal on top of the web contents of
  // CardUnmaskAuthenticationSelectionDialogController.
  static CardUnmaskAuthenticationSelectionDialog* CreateAndShow(
      CardUnmaskAuthenticationSelectionDialogController* controller,
      content::WebContents* web_contents);

  // Method to safely close this dialog (this also includes the case where the
  // controller is destroyed first). |user_closed_dialog| indicates whether the
  // dismissal was triggered by user closing the dialog. If |server_success| is
  // true it means the Payments server has responded with a success response to
  // the current flow's step. For example, in the SMS OTP flow, it would signify
  // the issuer has sent the OTP, and we can move on to the OTP Input Dialog.
  virtual void Dismiss(bool user_closed_dialog, bool server_success) = 0;

  // Updates the dialog content to show pending state.
  virtual void UpdateContent() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_H_
