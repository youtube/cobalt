// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_card_infobar_delegate_mobile.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "base/functional/bind.h"
#import "base/guid.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/signin/public/identity_manager/account_info.h"

MockAutofillSaveCardInfoBarDelegateMobile::
    MockAutofillSaveCardInfoBarDelegateMobile(
        autofill::AutofillClient::SaveCreditCardOptions options,
        const autofill::CreditCard& card,
        absl::variant<autofill::AutofillClient::LocalSaveCardPromptCallback,
                      autofill::AutofillClient::UploadSaveCardPromptCallback>
            callback,
        const autofill::LegalMessageLines& legal_message_lines,
        const AccountInfo& displayed_target_account)
    : AutofillSaveCardInfoBarDelegateMobile(options,
                                            card,
                                            std::move(callback),
                                            legal_message_lines,
                                            displayed_target_account) {}

MockAutofillSaveCardInfoBarDelegateMobile::
    ~MockAutofillSaveCardInfoBarDelegateMobile() = default;

#pragma mark - MockAutofillSaveCardInfoBarDelegateMobileFactory

MockAutofillSaveCardInfoBarDelegateMobileFactory::
    MockAutofillSaveCardInfoBarDelegateMobileFactory()
    : credit_card_(base::GenerateGUID(), "https://www.example.com/") {}

MockAutofillSaveCardInfoBarDelegateMobileFactory::
    ~MockAutofillSaveCardInfoBarDelegateMobileFactory() {}

std::unique_ptr<MockAutofillSaveCardInfoBarDelegateMobile>
MockAutofillSaveCardInfoBarDelegateMobileFactory::
    CreateMockAutofillSaveCardInfoBarDelegateMobileFactory(
        bool upload,
        autofill::CreditCard card) {
  using Variant =
      absl::variant<autofill::AutofillClient::LocalSaveCardPromptCallback,
                    autofill::AutofillClient::UploadSaveCardPromptCallback>;
  autofill::AutofillClient::UploadSaveCardPromptCallback upload_cb =
      base::DoNothing();
  autofill::AutofillClient::LocalSaveCardPromptCallback local_cb =
      base::DoNothing();
  return std::make_unique<MockAutofillSaveCardInfoBarDelegateMobile>(
      autofill::AutofillClient::SaveCreditCardOptions(), card,
      upload ? Variant(std::move(upload_cb)) : Variant(std::move(local_cb)),
      autofill::LegalMessageLines(), AccountInfo());
}
