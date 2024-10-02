// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_EDIT_ADDRESS_PROFILE_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_EDIT_ADDRESS_PROFILE_DIALOG_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class AutofillBubbleBase;

// The controller functionality for EditAddressProfileView.
class EditAddressProfileDialogControllerImpl
    : public EditAddressProfileDialogController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<
          EditAddressProfileDialogControllerImpl> {
 public:
  EditAddressProfileDialogControllerImpl(
      const EditAddressProfileDialogControllerImpl&) = delete;
  EditAddressProfileDialogControllerImpl& operator=(
      const EditAddressProfileDialogControllerImpl&) = delete;
  ~EditAddressProfileDialogControllerImpl() override;

  // Sets up the controller and offers to edit the `profile` before saving it.
  // If `original_profile` is not nullptr, this indicates that this dialog is
  // opened from an update prompt. The originating prompt (save or update) will
  // be re-opened once the user makes a decision with respect to the
  // offer-to-edit prompt. The `is_migration_to_account` argument is used to
  // re-open the original prompt in a correct state.
  void OfferEdit(const AutofillProfile& profile,
                 const AutofillProfile* original_profile,
                 const std::u16string& footer_message,
                 AutofillClient::AddressProfileSavePromptCallback
                     address_profile_save_prompt_callback,
                 bool is_migration_to_account);

  // EditAddressProfileDialogController:
  std::u16string GetWindowTitle() const override;
  const std::u16string& GetFooterMessage() const override;
  std::u16string GetOkButtonLabel() const override;
  const AutofillProfile& GetProfileToEdit() const override;
  bool GetIsValidatable() const override;
  void OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision decision,
      const AutofillProfile& profile_with_edits) override;
  void OnDialogClosed() override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  explicit EditAddressProfileDialogControllerImpl(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      EditAddressProfileDialogControllerImpl>;

  // Remove the |dialog_view_| and hide the dialog.
  void HideDialog();

  // nullptr if no dialog is currently shown.
  raw_ptr<AutofillBubbleBase> dialog_view_ = nullptr;

  // Editor's footnote message.
  std::u16string footer_message_;

  // Callback to run once the user makes a decision with respect to saving the
  // address profile currently being edited.
  AutofillClient::AddressProfileSavePromptCallback
      address_profile_save_prompt_callback_;

  // Contains the details of the address profile that the user requested to edit
  // before saving.
  AutofillProfile address_profile_to_edit_;

  // If not nullptr, this dialog was opened from an update prompt. Contains the
  // details of the address profile that will be updated if the user accepts
  // that update prompt from which this edit dialog was opened..
  absl::optional<AutofillProfile> original_profile_;

  // Whether the editor is used in the profile migration case. It is required
  // to restore the original prompt state (save or update) if it is reopened.
  bool is_migration_to_account_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_EDIT_ADDRESS_PROFILE_DIALOG_CONTROLLER_IMPL_H_
