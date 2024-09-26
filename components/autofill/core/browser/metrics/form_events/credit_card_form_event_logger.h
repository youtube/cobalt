// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_CREDIT_CARD_FORM_EVENT_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_CREDIT_CARD_FORM_EVENT_LOGGER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

enum class UnmaskAuthFlowType;

namespace autofill_metrics {

class CreditCardFormEventLogger : public FormEventLoggerBase {
 public:
  enum class UnmaskAuthFlowEvent {
    // Authentication prompt is shown.
    kPromptShown = 0,
    // Authentication prompt successfully completed.
    kPromptCompleted = 1,
    // Form was submitted.
    kFormSubmitted = 2,
    kMaxValue = kFormSubmitted,
  };

  CreditCardFormEventLogger(
      bool is_in_any_main_frame,
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
      PersonalDataManager* personal_data_manager,
      AutofillClient* client);

  ~CreditCardFormEventLogger() override;

  // Invoked when `suggestions` are successfully fetched. `with_offer` indicates
  // whether an offer is attached to any of the suggestion in the list.
  // `metadata_logging_context` contains information about whether any card has
  // a non-empty product description or art image, and whether they are shown.
  void OnDidFetchSuggestion(const std::vector<Suggestion>& suggestions,
                            bool with_offer,
                            const autofill_metrics::CardMetadataLoggingContext&
                                metadata_logging_context);

  void OnDidShowSuggestions(const FormStructure& form,
                            const AutofillField& field,
                            const base::TimeTicks& form_parsed_timestamp,
                            AutofillSyncSigninState sync_state,
                            bool off_the_record) override;

  void OnDidSelectCardSuggestion(const CreditCard& credit_card,
                                 const FormStructure& form,
                                 AutofillSyncSigninState sync_state);

  // To be called whenever (by BrowserAutofillManager) whenever a form is filled
  // (but not on preview).
  //
  // In case of masked cards, the caller must make sure this gets called before
  // the card is upgraded to a full card.
  //
  // The `newly_filled_fields` are all fields of `form` that are newly
  // filled by BrowserAutofillManager. They are still subject to the security
  // policy for cross-frame filling.
  //
  // The `safe_fields` are all fields of `form` that adhere to the security
  // policy for cross-frame filling.
  //
  // Therefore, the intersection of `newly_filled_fields` and `safe_fields`
  // contains the actually filled fields.
  void OnDidFillSuggestion(
      const CreditCard& credit_card,
      const FormStructure& form,
      const AutofillField& field,
      const base::flat_set<FieldGlobalId>& newly_filled_fields,
      const base::flat_set<FieldGlobalId>& safe_fields,
      AutofillSyncSigninState sync_state,
      const AutofillTriggerSource trigger_source);

  // Logging what type of authentication flow was prompted.
  void LogCardUnmaskAuthenticationPromptShown(UnmaskAuthFlowType flow);

  // Logging when an authentication prompt is completed.
  void LogCardUnmaskAuthenticationPromptCompleted(UnmaskAuthFlowType flow);

  // Allows mocking that a virtual card was selected, for unit tests that don't
  // run the actual Autofill suggestions dropdown UI.
  void set_latest_selected_card_was_virtual_card_for_testing(
      bool latest_selected_card_was_virtual_card) {
    latest_selected_card_was_virtual_card_ =
        latest_selected_card_was_virtual_card;
  }

 protected:
  // FormEventLoggerBase pure-virtual overrides.
  void RecordPollSuggestions() override;
  void RecordParseForm() override;
  void RecordShowSuggestions() override;

  // FormEventLoggerBase virtual overrides.
  void LogWillSubmitForm(const FormStructure& form) override;
  void LogFormSubmitted(const FormStructure& form) override;
  void LogUkmInteractedWithForm(FormSignature form_signature) override;
  void OnSuggestionsShownOnce(const FormStructure& form) override;
  void OnSuggestionsShownSubmittedOnce(const FormStructure& form) override;
  void OnLog(const std::string& name,
             FormEvent event,
             const FormStructure& form) const override;

  // Bringing base class' Log function into scope to allow overloading.
  using FormEventLoggerBase::Log;

 private:
  bool IsLocalDuplicateOfServerCard(const CreditCard& credit_card);
  FormEvent GetCardNumberStatusFormEvent(const CreditCard& credit_card);
  void RecordCardUnmaskFlowEvent(UnmaskAuthFlowType flow,
                                 UnmaskAuthFlowEvent event);
  bool DoesCardHaveOffer(const CreditCard& credit_card);
  // Returns whether the shown suggestions included a virtual credit card.
  bool DoSuggestionsIncludeVirtualCard();

  UnmaskAuthFlowType current_authentication_flow_;
  bool has_logged_suggestion_with_metadata_shown_ = false;
  bool has_logged_suggestion_with_metadata_selected_ = false;
  bool has_logged_masked_server_card_suggestion_selected_ = false;
  bool has_logged_virtual_card_suggestion_selected_ = false;
  bool logged_suggestion_filled_was_masked_server_card_ = false;
  bool logged_suggestion_filled_was_virtual_card_ = false;
  // If true, the most recent card to be selected as an Autofill suggestion was
  // a virtual card. False for all other card types.
  bool latest_selected_card_was_virtual_card_ = false;
  std::vector<Suggestion> suggestions_;
  bool has_eligible_offer_ = false;
  bool card_selected_has_offer_ = false;
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context_;

  // Set when a list of suggestion is shown.
  base::TimeTicks suggestion_shown_timestamp_;

  // Weak references.
  raw_ptr<PersonalDataManager> personal_data_manager_;
  raw_ptr<AutofillClient> client_;
};

}  // namespace autofill_metrics

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_CREDIT_CARD_FORM_EVENT_LOGGER_H_
