// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"

namespace autofill::autofill_metrics {

AddressFormEventLogger::AddressFormEventLogger(
    bool is_in_any_main_frame,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    AutofillClient* client)
    : FormEventLoggerBase("Address",
                          is_in_any_main_frame,
                          form_interactions_ukm_logger,
                          client) {}

AddressFormEventLogger::~AddressFormEventLogger() = default;

void AddressFormEventLogger::OnDidFillSuggestion(
    const AutofillProfile& profile,
    const FormStructure& form,
    const AutofillField& field,
    AutofillSyncSigninState sync_state,
    const AutofillTriggerSource trigger_source) {
  AutofillProfile::RecordType record_type = profile.record_type();
  sync_state_ = sync_state;

  form_interactions_ukm_logger_->LogDidFillSuggestion(
      record_type,
      /*is_for_credit_card=*/false, form, field);

  if (record_type == AutofillProfile::SERVER_PROFILE) {
    Log(FORM_EVENT_SERVER_SUGGESTION_FILLED, form);
  } else {
    Log(FORM_EVENT_LOCAL_SUGGESTION_FILLED, form);
  }

  if (!has_logged_suggestion_filled_) {
    has_logged_suggestion_filled_ = true;
    logged_suggestion_filled_was_server_data_ =
        record_type == AutofillProfile::SERVER_PROFILE;
    Log(record_type == AutofillProfile::SERVER_PROFILE
            ? FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE
            : FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE,
        form);
  }

  base::RecordAction(
      base::UserMetricsAction("Autofill_FilledProfileSuggestion"));

  if (trigger_source != AutofillTriggerSource::kFastCheckout) {
    ++form_interaction_counts_.autofill_fills;
  }
  UpdateFlowId();

  if (base::FeatureList::IsEnabled(
          features::kAutofillAccountProfilesUnionView)) {
    profile_categories_filled_.insert(GetCategoryOfProfile(profile));
  }
}

void AddressFormEventLogger::OnDidSeeFillableDynamicForm(
    AutofillSyncSigninState sync_state,
    const FormStructure& form) {
  sync_state_ = sync_state;
  Log(FORM_EVENT_DID_SEE_FILLABLE_DYNAMIC_FORM, form);
}

void AddressFormEventLogger::OnDidRefill(AutofillSyncSigninState sync_state,
                                         const FormStructure& form) {
  sync_state_ = sync_state;
  Log(FORM_EVENT_DID_DYNAMIC_REFILL, form);
}

void AddressFormEventLogger::OnSubsequentRefillAttempt(
    AutofillSyncSigninState sync_state,
    const FormStructure& form) {
  sync_state_ = sync_state;
  Log(FORM_EVENT_DYNAMIC_CHANGE_AFTER_REFILL, form);
}

void AddressFormEventLogger::OnLog(const std::string& name,
                                   FormEvent event,
                                   const FormStructure& form) const {
  uint32_t groups = data_util::DetermineGroups(form);
  base::UmaHistogramEnumeration(
      name + data_util::GetSuffixForProfileFormType(groups), event,
      NUM_FORM_EVENTS);
  if (data_util::ContainsAddress(groups) &&
      (data_util::ContainsPhone(groups) || data_util::ContainsEmail(groups))) {
    base::UmaHistogramEnumeration(name + ".AddressPlusContact", event,
                                  NUM_FORM_EVENTS);
  }
}

void AddressFormEventLogger::RecordPollSuggestions() {
  base::RecordAction(
      base::UserMetricsAction("Autofill_PolledProfileSuggestions"));
}

void AddressFormEventLogger::RecordParseForm() {
  base::RecordAction(base::UserMetricsAction("Autofill_ParsedProfileForm"));
}

void AddressFormEventLogger::RecordShowSuggestions() {
  base::RecordAction(
      base::UserMetricsAction("Autofill_ShowedProfileSuggestions"));
}

void AddressFormEventLogger::RecordFillingAssistance(LogBuffer& logs) const {
  FormEventLoggerBase::RecordFillingAssistance(logs);
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAccountProfilesUnionView)) {
    return;
  }
  // Log the origin-resolved filling assistance metric by converting the
  // `profile_categories_filled` to an CategoryResolvedFillingAssistanceBucket.
  auto filled_categories_to_bucket = [&] {
    if (profile_categories_filled_.empty()) {
      return CategoryResolvedFillingAssistanceBucket::kNone;
    }
    if (profile_categories_filled_.size() > 1) {
      return CategoryResolvedFillingAssistanceBucket::kMixed;
    }
    switch (*profile_categories_filled_.begin()) {
      case AutofillProfileSourceCategory::kLocalOrSyncable:
        return CategoryResolvedFillingAssistanceBucket::kLocalOrSyncable;
      case AutofillProfileSourceCategory::kAccountChrome:
        return CategoryResolvedFillingAssistanceBucket::kAccountChrome;
      case AutofillProfileSourceCategory::kAccountNonChrome:
        return CategoryResolvedFillingAssistanceBucket::kAccountNonChrome;
    }
  };
  base::UmaHistogramEnumeration("Autofill.Leipzig.FillingAssistanceCategory",
                                filled_categories_to_bucket());
}

void AddressFormEventLogger::RecordFillingCorrectness(LogBuffer& logs) const {
  FormEventLoggerBase::RecordFillingCorrectness(logs);
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAccountProfilesUnionView)) {
    return;
  }
  // Non-empty because correctness is only logged when an Autofill
  // suggestion was accepted.
  DCHECK(!profile_categories_filled_.empty());
  const std::string kBucket =
      profile_categories_filled_.size() == 1
          ? GetProfileCategorySuffix(*profile_categories_filled_.begin())
          : "Mixed";
  base::UmaHistogramBoolean("Autofill.Leipzig.FillingCorrectness." + kBucket,
                            !has_logged_edited_autofilled_field_);
}

}  // namespace autofill::autofill_metrics
