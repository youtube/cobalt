// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_DELEGATE_H_

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill {

class AutofillSaveCardInfoBarDelegateMobileTest;

// Delegate class providing callbacks for UIs presenting save card offers.
class AutofillSaveCardDelegate {
 public:
  AutofillSaveCardDelegate(
      absl::variant<AutofillClient::LocalSaveCardPromptCallback,
                    AutofillClient::UploadSaveCardPromptCallback>
          save_card_callback,
      AutofillClient::SaveCreditCardOptions options);

  virtual ~AutofillSaveCardDelegate();

  bool is_for_upload() const {
    return absl::holds_alternative<
        AutofillClient::UploadSaveCardPromptCallback>(save_card_callback_);
  }

  void OnUiShown();
  // `on_save_card_completed` will be triggered after the save card flow has
  // finished.
  virtual void OnUiAccepted(
      base::OnceClosure on_save_card_completed = base::NullCallback());
  void OnUiUpdatedAndAccepted(
      AutofillClient::UserProvidedCardDetails user_provided_details);
  virtual void OnUiCanceled();
  virtual void OnUiIgnored();

 protected:
  // Called when all of the prerequisites for saving a card have been met.
  void OnFinishedGatheringConsent(
      AutofillClient::SaveCardOfferUserDecision user_decision,
      AutofillClient::UserProvidedCardDetails user_provided_details);

 private:
  friend class AutofillSaveCardInfoBarDelegateMobileTest;

  // Runs the appropriate local or upload save callback with the given
  // |user_decision|, using the |user_provided_details|. If
  // |user_provided_details| is empty then the current Card values will be used.
  // The cardholder name and expiration date portions of
  // |user_provided_details| are handled separately, so if either of them are
  // empty the current card values will be used.
  void RunSaveCardPromptCallback(
      AutofillClient::SaveCardOfferUserDecision user_decision,
      AutofillClient::UserProvidedCardDetails user_provided_details);

  // TODO(crbug.com/1486941): Make GatherAdditionalConsentIfApplicable() a pure
  //                          virtual function.
  // This function by default saves the credit card, but allows subclasses to
  // override if there are prerequisites to saving the card (ex: Android
  // automotive requires a pin/password to be set on the device and must
  // redirect to that flow before saving card information).
  virtual void GatherAdditionalConsentIfApplicable(
      AutofillClient::UserProvidedCardDetails user_provided_details);

  void LogUserAction(AutofillMetrics::InfoBarMetric user_action);

  // If the cardholder name is missing, request the name from the user before
  // saving the card. If the expiration date is missing, request the missing
  // data from the user before saving the card.
  AutofillClient::SaveCreditCardOptions options_;

  // Did the user ever explicitly accept or dismiss this UI?
  bool had_user_interaction_;

  // The callback to run once the user makes a decision with respect to the
  // credit card offer-to-save prompt.
  absl::variant<AutofillClient::LocalSaveCardPromptCallback,
                AutofillClient::UploadSaveCardPromptCallback>
      save_card_callback_;

  // Callback to run immediately after `save_card_callback_`. An example of a
  // callback is cleaning up pointers to delegates that have their lifecycle
  // extended due to user going through device lock setup flows before saving a
  // card.
  base::OnceClosure on_finished_gathering_consent_callback_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_DELEGATE_H_
