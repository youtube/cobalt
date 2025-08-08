// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"

#include "components/autofill/core/browser/payments/test_payments_client.h"

namespace autofill {

TestCreditCardSaveManager::TestCreditCardSaveManager(
    AutofillDriver* driver,
    AutofillClient* client,
    payments::TestPaymentsClient* payments_client,
    PersonalDataManager* personal_data_manager)
    : CreditCardSaveManager(client,
                            payments_client,
                            "en-US",
                            personal_data_manager) {}

TestCreditCardSaveManager::~TestCreditCardSaveManager() {}

bool TestCreditCardSaveManager::IsCreditCardUploadEnabled() {
  return credit_card_upload_enabled_;
}

void TestCreditCardSaveManager::SetCreditCardUploadEnabled(
    bool credit_card_upload_enabled) {
  credit_card_upload_enabled_ = credit_card_upload_enabled;
}

bool TestCreditCardSaveManager::CreditCardWasUploaded() {
  return credit_card_was_uploaded_;
}

void TestCreditCardSaveManager::set_show_save_prompt(bool show_save_prompt) {
  show_save_prompt_ = show_save_prompt;
}

void TestCreditCardSaveManager::set_upload_request_card_number(
    const std::u16string& credit_card_number) {
  upload_request_.card.SetNumber(credit_card_number);
}

void TestCreditCardSaveManager::set_upload_request_card(
    const CreditCard& card) {
  upload_request_.card = std::move(card);
}

payments::PaymentsClient::UploadRequestDetails*
TestCreditCardSaveManager::upload_request() {
  return &upload_request_;
}

void TestCreditCardSaveManager::OnDidUploadCard(
    AutofillClient::PaymentsRpcResult result,
    const payments::PaymentsClient::UploadCardResponseDetails&
        upload_card_response_details) {
  credit_card_was_uploaded_ = true;
  CreditCardSaveManager::OnDidUploadCard(result, upload_card_response_details);
}

}  // namespace autofill
