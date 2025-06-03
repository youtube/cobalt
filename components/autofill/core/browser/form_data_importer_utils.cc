// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_data_importer_utils.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/metrics/profile_import_metrics.h"
#include "components/autofill/core/browser/profile_requirement_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"

namespace autofill {

namespace {

using AddressImportRequirement =
    autofill_metrics::AddressProfileImportRequirementMetric;

bool IsOriginPartOfDeletionInfo(const absl::optional<url::Origin>& origin,
                                const history::DeletionInfo& deletion_info) {
  if (!origin)
    return false;
  return deletion_info.IsAllHistory() ||
         base::Contains(deletion_info.deleted_rows(), *origin,
                        [](const history::URLRow& url_row) {
                          return url::Origin::Create(url_row.url());
                        });
}

}  // anonymous namespace

bool IsValidLearnableProfile(const AutofillProfile& profile,
                             LogBuffer* import_log_buffer) {
  // Returns false if `profile` has invalid information for `type`.
  auto ValidateAndLog = [&](ServerFieldType type,
                            AddressImportRequirement valid,
                            AddressImportRequirement invalid) {
    if (profile.IsPresentButInvalid(type)) {
      autofill_metrics::LogAddressFormImportRequirementMetric(invalid);
      LOG_AF(import_log_buffer)
          << LogMessage::kImportAddressProfileFromFormFailed << "Invalid "
          << FieldTypeToStringView(type) << "." << CTag{};
      return false;
    } else {
      autofill_metrics::LogAddressFormImportRequirementMetric(valid);
      return true;
    }
  };

  // Reject profiles with invalid `EMAIL_ADDRESS`, `ADDRESS_HOME_STATE` or
  // `ADDRESS_HOME_ZIP` entries and collect metrics on their validity.
  bool all_requirements_satisfied = ValidateAndLog(
      EMAIL_ADDRESS, AddressImportRequirement::kEmailValidRequirementFulfilled,
      AddressImportRequirement::kEmailValidRequirementViolated);

  all_requirements_satisfied &=
      ValidateAndLog(ADDRESS_HOME_STATE,
                     AddressImportRequirement::kStateValidRequirementFulfilled,
                     AddressImportRequirement::kStateValidRequirementViolated);

  all_requirements_satisfied &= ValidateAndLog(
      ADDRESS_HOME_ZIP, AddressImportRequirement::kZipValidRequirementFulfilled,
      AddressImportRequirement::kZipValidRequirementViolated);

  return all_requirements_satisfied;
}

std::string GetPredictedCountryCode(
    const AutofillProfile& profile,
    const GeoIpCountryCode& variation_country_code,
    const std::string& app_locale,
    LogBuffer* import_log_buffer) {
  // Try to acquire the country code form the filled form.
  std::string country_code =
      base::UTF16ToASCII(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));

  if (import_log_buffer && !country_code.empty()) {
    *import_log_buffer << LogMessage::kImportAddressProfileFromFormCountrySource
                       << "Country entry in form." << CTag{};
  }

  // As a fallback, use the variation service state to get a country code.
  if (country_code.empty() && !variation_country_code.value().empty()) {
    country_code = variation_country_code.value();
    if (import_log_buffer) {
      *import_log_buffer
          << LogMessage::kImportAddressProfileFromFormCountrySource
          << "Variations service." << CTag{};
    }
  }

  // As the last resort, derive the country code from the app_locale.
  if (country_code.empty()) {
    country_code = AutofillCountry::CountryCodeForLocale(app_locale);
    if (import_log_buffer && !country_code.empty()) {
      *import_log_buffer
          << LogMessage::kImportAddressProfileFromFormCountrySource
          << "App locale." << CTag{};
    }
  }

  return country_code;
}

MultiStepImportMerger::MultiStepImportMerger(
    const std::string& app_locale,
    const GeoIpCountryCode& variation_country_code)
    : app_locale_(app_locale),
      variation_country_code_(variation_country_code),
      comparator_(app_locale_) {}
MultiStepImportMerger::~MultiStepImportMerger() = default;

void MultiStepImportMerger::ProcessMultiStepImport(
    AutofillProfile& profile,
    ProfileImportMetadata& import_metadata) {
  multistep_candidates_.RemoveOutdatedItems(kMultiStepImportTTL,
                                            import_metadata.origin);
  bool has_min_address_requirements =
      MergeProfileWithMultiStepCandidates(profile, import_metadata);
  if (!has_min_address_requirements) {
    // Add the incomplete `profile` as an `multistep_candidate`, so it can be
    // complemented during later imports. Complete profiles for multi-step
    // complements are added in the AddressProfileSaveManager after a user
    // decision was made.
    AddMultiStepImportCandidate(profile, import_metadata,
                                /*is_imported=*/false);
  }
}

void MultiStepImportMerger::AddMultiStepImportCandidate(
    const AutofillProfile& profile,
    const ProfileImportMetadata& import_metadata,
    bool is_imported) {
  multistep_candidates_.Push({profile, import_metadata, is_imported},
                             import_metadata.origin);
}

bool MultiStepImportMerger::MergeProfileWithMultiStepCandidates(
    AutofillProfile& profile,
    ProfileImportMetadata& import_metadata) {
  // Start merging with the most recent `multistep_candidates_`.
  auto candidate = multistep_candidates_.begin();
  AutofillProfile completed_profile = profile;
  ProfileImportMetadata completed_metadata = import_metadata;
  // Merging might fail due to an incorrectly complemented country in one of the
  // merge candidates. In this case, try removing the complemented country.
  while (candidate != multistep_candidates_.end() &&
         (comparator_.AreMergeable(completed_profile, candidate->profile) ||
          MergeableByRemovingIncorrectlyComplementedCountry(
              completed_profile, completed_metadata.did_complement_country,
              candidate->profile,
              candidate->import_metadata.did_complement_country))) {
    completed_profile.MergeDataFrom(candidate->profile, app_locale_);
    MergeImportMetadata(candidate->import_metadata, completed_metadata);
    candidate++;
  }

  // The minimum address requirements depend on the country, which has possibly
  // changed as a result of the merge.
  if (IsMinimumAddress(completed_profile)) {
    profile = std::move(completed_profile);
    import_metadata = std::move(completed_metadata);
    multistep_candidates_.Clear();
    return true;
  } else {
    // Remove all profiles that couldn't be merged.
    multistep_candidates_.erase(candidate, multistep_candidates_.end());
    return false;
  }
}

bool MultiStepImportMerger::MergeableByRemovingIncorrectlyComplementedCountry(
    AutofillProfile& profile_a,
    bool& complemented_profile_a,
    AutofillProfile& profile_b,
    bool& complemented_profile_b) const {
  // Check if exactly one of the profiles has a complemented country.
  if (complemented_profile_a == complemented_profile_b ||
      profile_a.GetInfo(ADDRESS_HOME_COUNTRY, app_locale_) ==
          profile_b.GetInfo(ADDRESS_HOME_COUNTRY, app_locale_)) {
    return false;
  }
  AutofillProfile& complemented_profile =
      complemented_profile_a ? profile_a : profile_b;
  std::u16string complemented_country =
      complemented_profile.GetInfo(ADDRESS_HOME_COUNTRY, app_locale_);
  complemented_profile.ClearFields({ADDRESS_HOME_COUNTRY});
  if (comparator_.AreMergeable(profile_a, profile_b)) {
    if (complemented_profile_a)
      complemented_profile_a = false;
    else
      complemented_profile_b = false;
    return true;
  }
  // Even after removing the disagreeing country code, merging still failed.
  // Reset the profile back to it's original state. Otherwise we might end up
  // importing a country-less profile.
  complemented_profile.SetInfoWithVerificationStatus(
      AutofillType(ADDRESS_HOME_COUNTRY), complemented_country, app_locale_,
      VerificationStatus::kObserved);
  return false;
}

void MultiStepImportMerger::MergeImportMetadata(
    const ProfileImportMetadata& source,
    ProfileImportMetadata& target) const {
  // If an invalid phone number was observed in either of the partial profiles,
  // importing was only possible due to its removal. For the purpose of metrics,
  // we care about the status of the validity of the phone number in the
  // combined profile. Thus the logic merges towards kValid.
  if (target.phone_import_status != PhoneImportStatus::kValid &&
      source.phone_import_status != PhoneImportStatus::kNone) {
    target.phone_import_status = source.phone_import_status;
  }
  // If either of the partial profiles contains information imported from an
  // unrecognized autocomplete attribute, so does the combined profile.
  target.did_import_from_unrecognized_autocomplete_field |=
      source.did_import_from_unrecognized_autocomplete_field;
  // The country of the merged profile is only considered complemented if both
  // of them were complemented. Otherwise one of them was observed and
  // complementing the country has not made a difference.
  target.did_complement_country &= source.did_complement_country;
}

void MultiStepImportMerger::OnBrowsingHistoryCleared(
    const history::DeletionInfo& deletion_info) {
  if (IsOriginPartOfDeletionInfo(multistep_candidates_.origin(), deletion_info))
    Clear();
}

void MultiStepImportMerger::OnPersonalDataChanged(
    PersonalDataManager& personal_data_manager) {
  auto it = multistep_candidates_.begin();
  while (it != multistep_candidates_.end()) {
    // `it` might get erased, so `it++` at the end of the loop doesn't suffice.
    auto next = std::next(it);
    // Incomplete profiles are not imported yet, so they cannot have changed.
    if (it->is_imported) {
      AutofillProfile* stored_profile =
          personal_data_manager.GetProfileByGUID(it->profile.guid());
      if (!stored_profile) {
        // The profile was deleted, so we shouldn't offer importing it again.
        multistep_candidates_.erase(it, next);
      } else if (it->profile != *stored_profile) {
        // The profile was edited in some way. Make sure that we offer updates
        // for the latest version.
        it->profile = *stored_profile;
      }
    }
    it = next;
  }
}

FormAssociator::FormAssociator() = default;
FormAssociator::~FormAssociator() = default;

void FormAssociator::TrackFormAssociations(const url::Origin& origin,
                                           FormSignature form_signature,
                                           FormType form_type) {
  const base::TimeDelta ttl = features::kAutofillAssociateFormsTTL.Get();
  // This ensures that `recent_address_forms_` and `recent_credit_card_forms`
  // share the same origin (if they are non-empty).
  recent_address_forms_.RemoveOutdatedItems(ttl, origin);
  recent_credit_card_forms_.RemoveOutdatedItems(ttl, origin);

  auto& container = form_type == FormType::kAddressForm
                        ? recent_address_forms_
                        : recent_credit_card_forms_;
  container.Push(form_signature, origin);
}

absl::optional<FormStructure::FormAssociations>
FormAssociator::GetFormAssociations(FormSignature form_signature) const {
  FormStructure::FormAssociations associations;
  if (!recent_address_forms_.empty())
    associations.last_address_form_submitted = *recent_address_forms_.begin();
  if (!recent_credit_card_forms_.empty()) {
    associations.last_credit_card_form_submitted =
        *recent_credit_card_forms_.begin();
  }
  if (associations.last_address_form_submitted != form_signature &&
      associations.last_credit_card_form_submitted != form_signature) {
    return absl::nullopt;
  }
  if (recent_address_forms_.size() > 1) {
    associations.second_last_address_form_submitted =
        *std::next(recent_address_forms_.begin());
  }
  return associations;
}

const absl::optional<url::Origin>& FormAssociator::origin() const {
  DCHECK(
      !recent_address_forms_.origin() || !recent_credit_card_forms_.origin() ||
      *recent_address_forms_.origin() == *recent_credit_card_forms_.origin());
  return recent_address_forms_.origin() ? recent_address_forms_.origin()
                                        : recent_credit_card_forms_.origin();
}

void FormAssociator::Clear() {
  recent_address_forms_.Clear();
  recent_credit_card_forms_.Clear();
}

void FormAssociator::OnBrowsingHistoryCleared(
    const history::DeletionInfo& deletion_info) {
  if (IsOriginPartOfDeletionInfo(origin(), deletion_info))
    Clear();
}

}  // namespace autofill
