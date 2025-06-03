// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENT_LOGGER_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENT_LOGGER_BASE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_ablation_study.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_interactions_flow.h"

namespace autofill::autofill_metrics {

// Utility to log autofill form events in the relevant histograms depending on
// the presence of server and/or local data.
class FormEventLoggerBase {
 public:
  FormEventLoggerBase(
      const std::string& form_type_name,
      bool is_in_any_main_frame,
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
      AutofillClient* client);

  inline void set_server_record_type_count(size_t server_record_type_count) {
    server_record_type_count_ = server_record_type_count;
  }

  inline void set_local_record_type_count(size_t local_record_type_count) {
    local_record_type_count_ = local_record_type_count;
  }

  void OnDidInteractWithAutofillableForm(
      const FormStructure& form,
      AutofillMetrics::PaymentsSigninState signin_state_for_metrics);

  void OnDidPollSuggestions(
      const FormFieldData& field,
      AutofillMetrics::PaymentsSigninState signin_state_for_metrics);

  void OnDidParseForm(const FormStructure& form);

  void OnUserHideSuggestions(const FormStructure& form,
                             const AutofillField& field);

  virtual void OnDidShowSuggestions(
      const FormStructure& form,
      const AutofillField& field,
      const base::TimeTicks& form_parsed_timestamp,
      AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
      bool off_the_record);

  void OnWillSubmitForm(
      AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
      const FormStructure& form);

  void OnFormSubmitted(
      AutofillMetrics::PaymentsSigninState signin_state_for_metrics,
      const FormStructure& form);

  void OnTypedIntoNonFilledField();
  void OnEditedAutofilledField();

  // Must be called right before the event logger is destroyed. It triggers the
  // logging of funnel and key metrics.
  // The function must not be called from the destructor, since this makes it
  // impossible to dispatch virtual functions into the derived classes.
  void OnDestroyed();

  // See BrowserAutofillManager::SuggestionContext for the definitions of the
  // AblationGroup parameters.
  void SetAblationStatus(AblationGroup ablation_group,
                         AblationGroup conditional_ablation_group);
  void SetTimeFromInteractionToSubmission(
      base::TimeDelta time_from_interaction_to_submission);

  void OnAutofilledFieldWasClearedByJavaScriptShortlyAfterFill(
      const FormStructure& form);

  void Log(FormEvent event, const FormStructure& form);

  void OnTextFieldDidChange(const FieldGlobalId& field_global_id);

  const FormInteractionCounts& form_interaction_counts() const {
    return form_interaction_counts_;
  }

  void SetFastCheckoutRunId(int64_t run_id) { fast_checkout_run_id_ = run_id; }

  AutofillMetrics::FormEventSet GetFormEvents(FormGlobalId form_global_id);

  const FormInteractionsFlowId& form_interactions_flow_id_for_test() const {
    return flow_id_;
  }

  const absl::optional<int64_t> fast_checkout_run_id_for_test() const {
    return fast_checkout_run_id_;
  }

 protected:
  virtual ~FormEventLoggerBase();

  virtual void RecordPollSuggestions() = 0;
  virtual void RecordParseForm() = 0;
  virtual void RecordShowSuggestions() = 0;

  virtual void LogWillSubmitForm(const FormStructure& form);
  virtual void LogFormSubmitted(const FormStructure& form);

  // Only used for UKM backward compatibility since it depends on IsCreditCard.
  // TODO (crbug.com/925913): Remove IsCreditCard from UKM logs amd replace with
  // |form_type_name_|.
  virtual void LogUkmInteractedWithForm(FormSignature form_signature);

  virtual void OnSuggestionsShownOnce(const FormStructure& form) {}
  virtual void OnSuggestionsShownSubmittedOnce(const FormStructure& form) {}

  // Logs |event| in a histogram prefixed with |name| according to the
  // FormEventLogger type and |form|. For example, in the address context, it
  // may be useful to analyze metrics for forms (A) with only name and address
  // fields and (B) with only name and phone fields separately.
  virtual void OnLog(const std::string& name,
                     FormEvent event,
                     const FormStructure& form) const {}

  // Records UMA metrics on the funnel and writes logs to autofill-internals.
  void RecordFunnelMetrics() const;

  // For each funnel metric, a separate function is defined below.
  // `RecordFunnelMetrics()` checks the necessary pre-conditions for metrics to
  // be emitted and calls the relevant functions.
  void RecordInteractionAfterParsedAsType(LogBuffer& logs) const;
  void RecordSuggestionAfterInteraction(LogBuffer& logs) const;
  void RecordFillAfterSuggestion(LogBuffer& logs) const;
  void RecordSubmissionAfterFill(LogBuffer& logs) const;

  // Records UMA metrics on keym etrics and writes logs to autofill-internals.
  // Similar to the funnel metrics, a separate function for each key metric is
  // defined below.
  void RecordKeyMetrics() const;

  // Whether for a submitted form, Chrome had data stored that could be
  // filled.
  void RecordFillingReadiness(LogBuffer& logs) const;

  // Whether a user accepted a filling suggestion they saw for a form that
  // was later submitted.
  void RecordFillingAcceptance(LogBuffer& logs) const;

  // Whether a filled form and submitted form required no fixes to filled
  // fields.
  virtual void RecordFillingCorrectness(LogBuffer& logs) const;

  // Whether a submitted form was filled.
  virtual void RecordFillingAssistance(LogBuffer& logs) const;

  // Whether a (non-)autofilled form was submitted.
  void RecordFormSubmission(LogBuffer& logs) const;

  // Records UMA metrics if this form submission happened as part of an ablation
  // study or the corresponding control group. This is not virtual because it is
  // called in the destructor.
  void RecordAblationMetrics() const;

  void UpdateFlowId();

  // Constructor parameters.
  std::string form_type_name_;
  bool is_in_any_main_frame_;

  // State variables.
  size_t server_record_type_count_ = 0;
  size_t local_record_type_count_ = 0;
  bool has_parsed_form_ = false;
  bool has_logged_interacted_ = false;
  bool has_logged_user_hide_suggestions_ = false;
  bool has_logged_suggestions_shown_ = false;
  bool has_logged_suggestion_filled_ = false;
  bool has_logged_autocomplete_off_ = false;
  bool has_logged_will_submit_ = false;
  bool has_logged_submitted_ = false;
  bool logged_suggestion_filled_was_server_data_ = false;
  bool has_logged_typed_into_non_filled_field_ = false;
  bool has_logged_edited_autofilled_field_ = false;
  bool has_logged_autofilled_field_was_cleared_by_javascript_after_fill_ =
      false;
  bool has_called_on_destoryed_ = false;
  bool is_heuristic_only_email_form_ = false;
  AblationGroup ablation_group_ = AblationGroup::kDefault;
  AblationGroup conditional_ablation_group_ = AblationGroup::kDefault;
  absl::optional<base::TimeDelta> time_from_interaction_to_submission_;

  // The last field that was polled for suggestions.
  FormFieldData last_polled_field_;

  // Used to count consecutive modifications on the same field as one change.
  FieldGlobalId last_field_global_id_modified_by_user_;
  // Keeps counts of Autofill fills and form elements that were modified by the
  // user.
  FormInteractionCounts form_interaction_counts_ = {};
  // Unique random id that is set on the first form interaction and identical
  // during the flow.
  FormInteractionsFlowId flow_id_;
  // Unique ID of a Fast Checkout run. Used for metrics.
  absl::optional<int64_t> fast_checkout_run_id_;

  // Form types of the submitted form.
  DenseSet<FormType> submitted_form_types_;

  // A map of the form's global id and its form events.
  std::map<FormGlobalId, AutofillMetrics::FormEventSet> form_events_set_;

  // Weak reference.
  raw_ptr<AutofillMetrics::FormInteractionsUkmLogger>
      form_interactions_ukm_logger_;

  // Weak reference.
  const raw_ref<AutofillClient> client_;

  AutofillMetrics::PaymentsSigninState signin_state_for_metrics_ =
      AutofillMetrics::PaymentsSigninState::kUnknown;
};
}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENT_LOGGER_BASE_H_
