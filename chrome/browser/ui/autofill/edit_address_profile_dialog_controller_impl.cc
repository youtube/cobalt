// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"
#include <string>

#include "base/types/optional_util.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/edit_address_profile_view.h"
#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

EditAddressProfileDialogControllerImpl::EditAddressProfileDialogControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<EditAddressProfileDialogControllerImpl>(
          *web_contents) {}

EditAddressProfileDialogControllerImpl::
    ~EditAddressProfileDialogControllerImpl() {
  HideDialog();
}

void EditAddressProfileDialogControllerImpl::OfferEdit(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    const std::u16string& footer_message,
    AutofillClient::AddressProfileSavePromptCallback on_user_decision_callback,
    bool is_migration_to_account) {
  // Don't show the editor if it's already visible, and inform the backend.
  if (dialog_view_) {
    std::move(on_user_decision_callback)
        .Run(AutofillClient::SaveAddressProfileOfferUserDecision::kAutoDeclined,
             profile);
    return;
  }
  address_profile_to_edit_ = profile;
  original_profile_ = base::OptionalFromPtr(original_profile);
  footer_message_ = footer_message;
  on_user_decision_callback_ = std::move(on_user_decision_callback);
  is_migration_to_account_ = is_migration_to_account;

  if (view_factory_for_test_) {
    dialog_view_ = view_factory_for_test_.Run(web_contents(), this);
    return;
  }

  dialog_view_ = ShowEditAddressProfileDialogView(web_contents(), this);
}

std::u16string EditAddressProfileDialogControllerImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_TITLE);
}

const std::u16string& EditAddressProfileDialogControllerImpl::GetFooterMessage()
    const {
  return footer_message_;
}

std::u16string EditAddressProfileDialogControllerImpl::GetOkButtonLabel()
    const {
  return l10n_util::GetStringUTF16(
      original_profile_.has_value()
          ? IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_OK_BUTTON_LABEL_UPDATE
          : IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_OK_BUTTON_LABEL_SAVE);
}

const AutofillProfile&
EditAddressProfileDialogControllerImpl::GetProfileToEdit() const {
  return address_profile_to_edit_;
}

bool EditAddressProfileDialogControllerImpl::GetIsValidatable() const {
  // Only account address profiles should be validated, i.e. the ones already
  // stored in account (the source property) and those that are currently
  // migrating.
  return address_profile_to_edit_.source() ==
             AutofillProfile::Source::kAccount ||
         is_migration_to_account_;
}

void EditAddressProfileDialogControllerImpl::OnDialogClosed(
    AutofillClient::SaveAddressProfileOfferUserDecision decision,
    base::optional_ref<const AutofillProfile> profile_with_edits) {
  std::move(on_user_decision_callback_).Run(decision, profile_with_edits);
  dialog_view_ = nullptr;
}

void EditAddressProfileDialogControllerImpl::WebContentsDestroyed() {
  HideDialog();
}

void EditAddressProfileDialogControllerImpl::HideDialog() {
  if (dialog_view_) {
    dialog_view_->Hide();
    dialog_view_ = nullptr;
  }
}

void EditAddressProfileDialogControllerImpl::SetViewFactoryForTest(
    EditAddressProfileViewTestingFactory factory) {
  view_factory_for_test_ = std::move(factory);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(EditAddressProfileDialogControllerImpl);

}  // namespace autofill
