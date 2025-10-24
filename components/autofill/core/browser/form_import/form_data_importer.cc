// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/form_data_importer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_name.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/addresses/phone_number.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_requirement_utils.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_import/addresses/address_profile_save_manager.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/integrators/plus_addresses/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/profile_import_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/iban_save_manager.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/history/core/browser/history_service.h"

namespace autofill {

namespace {

using AddressImportRequirement =
    autofill_metrics::AddressProfileImportRequirementMetric;

// Struct storing a field's value for import and selected option value, if
// present.
struct ValueForImport {
  // The return value of `AutofillField::value_for_import()`.
  std::u16string value_for_import;
  // The value of the selected option. Only set for <select> fields and where
  // `FormFieldData::selected_option()` doesn't return std::nullopt.
  std::optional<std::u16string> selected_option_value;
};

// Determines if a field's value matches a previously observed field of the same
// type, allowing for duplicate fields with identical values or <select>/<input>
// value mirroring.
bool FieldValueMatchesPrecedingField(const ValueForImport& current_values,
                                     const ValueForImport& preceding_values) {
  bool field_values_match = false;
  // Checks for exact value match between current and previously observed
  // fields.
  if (preceding_values.value_for_import == current_values.value_for_import) {
    field_values_match = true;
  }

  // Check if the selected option value of the current field (if it's a <select>
  // field) matches the value previously observed for a field of the same type.
  // This handles the case where a <select> option's value is stored in a
  // separate <input> field.
  // Example:
  // <select id="country">
  //   <option value="US">United States</option>
  // </select>
  // <input type="text" name="country_code" value="US">
  // Here, `selected_option_value` would be "US", and
  // `observed_field.value_for_import` from the input would also be "US".
  if (current_values.selected_option_value ==
      preceding_values.value_for_import) {
    field_values_match = true;
  }

  // Check if the value of the selected option from a previously observed
  // <select> field matches the value intended for import from the current
  // field.
  // Example:
  // <input type="text" name="country_code" value="US">
  // <select id="country">
  //   <option value="US">United States</option>
  // </select>
  // Here, `observed_field.selected_option_value` would be "US", and
  // `value_for_import` from the <select> would also be "US".
  if (preceding_values.selected_option_value ==
      current_values.value_for_import) {
    field_values_match = true;
  }

  // TODO(crbug.com/40735892) Remove feature check when launched.
  return field_values_match &&
         base::FeatureList::IsEnabled(features::kAutofillRelaxAddressImport);
}

// Return true if the `field_type` and `current_values` are valid within the
// context of importing a form.
bool IsValidFieldTypeAndValue(
    const base::flat_map<FieldType, ValueForImport>& preceding_values,
    FieldType field_type,
    const ValueForImport& current_values,
    LogBuffer* import_log_buffer) {
  // Abandon the import if an email address value shows up in a field that is
  // not an email address.
  if (field_type != EMAIL_ADDRESS &&
      IsValidEmailAddress(current_values.value_for_import)) {
    LOG_AF(import_log_buffer)
        << LogMessage::kImportAddressProfileFromFormFailed
        << "Email address found in field of different type: "
        << FieldTypeToStringView(field_type) << CTag{};
    return false;
  }

  // Allow the import if `field_type` wasn't observed before. Also, allow it for
  // duplicate fields with identical field values.
  // TODO(crbug.com/395855125): Clean up when launched.
  if (auto it = preceding_values.find(field_type);
      it == preceding_values.end() ||
      FieldValueMatchesPrecedingField(current_values,
                                      preceding_values.at(field_type))) {
    return true;
  }

  // Allow the import for duplicate EMAIL_ADDRESS fields because it is common to
  // see a second 'confirm email address' field.
  if (field_type == EMAIL_ADDRESS) {
    return true;
  }

  // Allow the import for duplicate phone number component fields because a form
  // might request several phone numbers.
  // TODO(crbug.com/40735892) Remove feature check when launched.
  if (GroupTypeOfFieldType(field_type) == FieldTypeGroup::kPhone ||
      base::FeatureList::IsEnabled(
          features::kAutofillEnableImportWhenMultiplePhoneNumbers)) {
    return true;
  }

  // Abandon the import if two fields of the same type are encountered (after
  // prior exception checks). This indicates ambiguous data or miscategorization
  // of types.
  LOG_AF(import_log_buffer)
      << LogMessage::kImportAddressProfileFromFormFailed
      << "Multiple fields of type " << FieldTypeToStringView(field_type) << "."
      << CTag{};
  return false;
}

// `extracted_credit_card` refers to the credit card that was most recently
// submitted and |fetched_card_instrument_id| refers to the instrument id of the
// most recently downstreamed (fetched from the server) credit card.
// These need to match to offer virtual card enrollment for the
// `extracted_credit_card`.
bool ShouldOfferVirtualCardEnrollment(
    const std::optional<CreditCard>& extracted_credit_card,
    std::optional<int64_t> fetched_card_instrument_id) {
  if (!extracted_credit_card) {
    return false;
  }

  if (extracted_credit_card->virtual_card_enrollment_state() !=
      CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible) {
    return false;
  }

  if (!fetched_card_instrument_id.has_value() ||
      extracted_credit_card->instrument_id() !=
          fetched_card_instrument_id.value()) {
    return false;
  }

  return true;
}

bool HasSynthesizedTypes(
    const base::flat_map<FieldType, std::u16string>& observed_field_values,
    AddressCountryCode country_code) {
  return std::ranges::any_of(observed_field_values, [country_code](
                                                        const auto& entry) {
    return i18n_model_definition::IsSynthesizedType(entry.first, country_code);
  });
}

bool ShouldProcessExtractedCreditCard(
    const raw_ref<AutofillClient>& client,
    FormDataImporter::CreditCardImportType credit_card_import_type) {
  // Processing should not occur if the current window is a tab modal pop-up, as
  // no credit card save or feature enrollment should happen in this case.
  if (base::FeatureList::IsEnabled(
          features::kAutofillSkipSaveCardForTabModalPopup) &&
      client->GetPaymentsAutofillClient()->IsTabModalPopupDeprecated()) {
    return false;
  }

  // If there is no `credit_card_import_type` from form extraction, the
  // extracted card is not a viable candidate for processing.
  if (credit_card_import_type ==
      FormDataImporter::CreditCardImportType::kNoCard) {
    return false;
  }

  return true;
}

}  // namespace

FormDataImporter::ExtractedFormData::ExtractedFormData() = default;

FormDataImporter::ExtractedFormData::ExtractedFormData(
    const ExtractedFormData& extracted_form_data) = default;

FormDataImporter::ExtractedFormData&
FormDataImporter::ExtractedFormData::operator=(
    const ExtractedFormData& extracted_form_data) = default;

FormDataImporter::ExtractedFormData::~ExtractedFormData() = default;

FormDataImporter::FormDataImporter(AutofillClient* client,
                                   history::HistoryService* history_service)
    : client_(CHECK_DEREF(client)),
      credit_card_save_manager_(
          std::make_unique<CreditCardSaveManager>(client)),
      address_profile_save_manager_(
          std::make_unique<AddressProfileSaveManager>(client)),
#if !BUILDFLAG(IS_IOS)
      iban_save_manager_(std::make_unique<IbanSaveManager>(client)),
#endif  // !BUILDFLAG(IS_IOS)
      multistep_importer_(client_->GetAppLocale(),
                          client_->GetVariationConfigCountryCode()) {
  address_data_manager_observation_.Observe(&address_data_manager());
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
}

FormDataImporter::~FormDataImporter() = default;

FormDataImporter::ExtractedAddressProfile::ExtractedAddressProfile() = default;
FormDataImporter::ExtractedAddressProfile::ExtractedAddressProfile(
    const FormDataImporter::ExtractedAddressProfile& other) = default;
FormDataImporter::ExtractedAddressProfile::~ExtractedAddressProfile() = default;

void FormDataImporter::ImportAndProcessFormData(
    const FormStructure& submitted_form,
    bool profile_autofill_enabled,
    bool payment_methods_autofill_enabled,
    ukm::SourceId ukm_source_id) {
  ExtractedFormData extracted_data =
      ExtractFormData(submitted_form, profile_autofill_enabled,
                      payment_methods_autofill_enabled);

  // Create a vector of extracted address profiles.
  // This is used to make preliminarily imported profiles available
  // to the credit card import logic.
  std::vector<AutofillProfile> preliminary_imported_address_profiles;
  for (const auto& candidate : extracted_data.extracted_address_profiles) {
    if (candidate.all_requirements_fulfilled) {
      preliminary_imported_address_profiles.push_back(candidate.profile);
    }
  }
  credit_card_save_manager_->SetPreliminarilyImportedAutofillProfile(
      preliminary_imported_address_profiles);

  bool cc_prompt_potentially_shown = false;
  if (ShouldProcessExtractedCreditCard(client_, credit_card_import_type_)) {
    // Only check IsCreditCardUploadEnabled() if conditions that enable
    // processing of the extracted credit card are true, in order to prevent
    // the metrics it logs from being diluted by cases where extracted credit
    // cards should not be processed or there was no credit card to process.
    bool credit_card_upload_enabled =
        credit_card_save_manager_->IsCreditCardUploadEnabled();
    cc_prompt_potentially_shown = ProcessExtractedCreditCard(
        submitted_form, extracted_data.extracted_credit_card,
        credit_card_upload_enabled, ukm_source_id);
  }
  fetched_card_instrument_id_.reset();

  bool iban_prompt_potentially_shown = false;
  if (extracted_data.extracted_iban.has_value() &&
      payment_methods_autofill_enabled) {
    iban_prompt_potentially_shown =
        ProcessIbanImportCandidate(*extracted_data.extracted_iban);
  }

  ProcessExtractedAddressProfiles(
      extracted_data.extracted_address_profiles,
      // If a prompt for credit cards or IBANs is potentially shown, do not
      // allow for a second address profile import dialog.
      /*allow_prompt=*/!cc_prompt_potentially_shown &&
          !iban_prompt_potentially_shown,
      ukm_source_id);
}

bool FormDataImporter::ComplementCountry(AutofillProfile& profile,
                                         LogBuffer* import_log_buffer) {
  if (profile.HasRawInfo(ADDRESS_HOME_COUNTRY)) {
    return false;
  }
  const std::string fallback =
      address_data_manager().GetDefaultCountryCodeForNewAddress().value();
  if (import_log_buffer) {
    *import_log_buffer
        << LogMessage::kImportAddressProfileComplementedCountryCode << fallback
        << CTag{};
  }
  return profile.SetInfoWithVerificationStatus(
      ADDRESS_HOME_COUNTRY, base::ASCIIToUTF16(fallback),
      client_->GetAppLocale(), VerificationStatus::kObserved);
}

bool FormDataImporter::SetPhoneNumber(
    AutofillProfile& profile,
    const PhoneNumber::PhoneCombineHelper& combined_phone) {
  if (combined_phone.IsEmpty()) {
    return true;
  }

  bool parsed_successfully = PhoneNumber::ImportPhoneNumberToProfile(
      combined_phone, client_->GetAppLocale(), profile);
  autofill_metrics::LogPhoneNumberImportParsingResult(parsed_successfully);
  return parsed_successfully;
}

void FormDataImporter::RemoveInaccessibleProfileValues(
    AutofillProfile& profile) {
  const FieldTypeSet inaccessible_fields =
      profile.FindInaccessibleProfileValues();
  profile.ClearFields(inaccessible_fields);
  autofill_metrics::LogRemovedSettingInaccessibleFields(
      !inaccessible_fields.empty());
  for (const FieldType inaccessible_field : inaccessible_fields) {
    autofill_metrics::LogRemovedSettingInaccessibleField(inaccessible_field);
  }
}

void FormDataImporter::CacheFetchedVirtualCard(
    const std::u16string& last_four) {
  fetched_virtual_cards_.insert(last_four);
}

void FormDataImporter::SetFetchedCardInstrumentId(int64_t instrument_id) {
  fetched_card_instrument_id_ = instrument_id;
}

FormDataImporter::ExtractedFormData FormDataImporter::ExtractFormData(
    const FormStructure& submitted_form,
    bool profile_autofill_enabled,
    bool payment_methods_autofill_enabled) {
  ExtractedFormData extracted_form_data;
  // We try the same `form` for both credit card and address import/update.
  // - `ExtractCreditCard()` may update an existing card, or fill
  //   `extracted_credit_card` contained in `extracted_form_data` with an
  //   extracted card.
  // - `ExtractAddressProfiles()` collects all importable
  // profiles, but currently
  //   at most one import prompt is shown.
  // Reset `credit_card_import_type_` every time we extract
  // data from form no matter whether `ExtractCreditCard()` is
  // called or not.
  credit_card_import_type_ = CreditCardImportType::kNoCard;
  if (payment_methods_autofill_enabled) {
    extracted_form_data.extracted_credit_card =
        ExtractCreditCard(submitted_form);
  }

#if !BUILDFLAG(IS_IOS)
  if (payment_methods_autofill_enabled) {
    extracted_form_data.extracted_iban = ExtractIban(submitted_form);
  }
#endif  // !BUILDFLAG(IS_IOS)

  size_t num_complete_address_profiles = 0;
  if (profile_autofill_enabled &&
      !base::FeatureList::IsEnabled(features::kAutofillDisableAddressImport)) {
    num_complete_address_profiles = ExtractAddressProfiles(
        submitted_form, &extracted_form_data.extracted_address_profiles);
  }

  if (profile_autofill_enabled && payment_methods_autofill_enabled) {
    const url::Origin origin = submitted_form.main_frame_origin();
    FormSignature form_signature = submitted_form.form_signature();
    // If multiple complete address profiles were extracted, this most likely
    // corresponds to billing and shipping sections within the same form.
    for (size_t i = 0; i < num_complete_address_profiles; i++) {
      form_associator_.TrackFormAssociations(
          origin, form_signature, FormAssociator::FormType::kAddressForm);
    }
    if (extracted_form_data.extracted_credit_card) {
      form_associator_.TrackFormAssociations(
          origin, form_signature, FormAssociator::FormType::kCreditCardForm);
    }
  }

  return extracted_form_data;
}

size_t FormDataImporter::ExtractAddressProfiles(
    const FormStructure& form,
    std::vector<FormDataImporter::ExtractedAddressProfile>*
        extracted_address_profiles) {
  // Create a buffer to collect logging output for the autofill-internals.
  LogManager* log_manager = client_->GetCurrentLogManager();
  LogBuffer import_log_buffer(IsLoggingActive(log_manager));
  LOG_AF(import_log_buffer) << LoggingScope::kAddressProfileFormImport;
  // Print the full form into the logging scope.
  LOG_AF(import_log_buffer)
      << LogMessage::kImportAddressProfileFromForm << form << CTag{};

  // We save a maximum of 2 profiles per submitted form (e.g. for shipping and
  // billing).
  static const size_t kMaxNumAddressProfilesSaved = 2;
  size_t num_complete_profiles = 0;

  if (!form.field_count()) {
    LOG_AF(import_log_buffer) << LogMessage::kImportAddressProfileFromFormFailed
                              << "Form is empty." << CTag{};
  } else {
    // Relevant sections for address fields.
    std::map<Section, std::vector<const AutofillField*>> section_fields;
    for (const auto& field : form) {
      if (IsAddressType(field->Type().GetStorableType())) {
        section_fields[field->section()].push_back(field.get());
      }
    }

    for (const auto& [section, fields] : section_fields) {
      if (num_complete_profiles == kMaxNumAddressProfilesSaved) {
        break;
      }
      // Log the output from a section in a separate div for readability.
      LOG_AF(import_log_buffer)
          << Tag{"div"} << Attrib{"class", "profile_import_from_form_section"};
      LOG_AF(import_log_buffer)
          << LogMessage::kImportAddressProfileFromFormSection << section
          << CTag{};
      // Try to extract an address profile from the form fields of this section.
      // Only allow for a prompt if no other complete profile was found so far.
      if (ExtractAddressProfileFromSection(fields, form.source_url(),
                                           extracted_address_profiles,
                                           &import_log_buffer)) {
        num_complete_profiles++;
      }
      // And close the div of the section import log.
      LOG_AF(import_log_buffer) << CTag{"div"};
    }
    autofill_metrics::LogAddressFormImportStatusMetric(
        num_complete_profiles == 0
            ? autofill_metrics::AddressProfileImportStatusMetric::kNoImport
            : autofill_metrics::AddressProfileImportStatusMetric::
                  kRegularImport);
  }
  LOG_AF(import_log_buffer)
      << LogMessage::kImportAddressProfileFromFormNumberOfImports
      << num_complete_profiles << CTag{};

  // Write log buffer to autofill-internals.
  LOG_AF(log_manager) << std::move(import_log_buffer);

  return num_complete_profiles;
}

AutofillProfile FormDataImporter::ConstructProfileFromObservedValues(
    const base::flat_map<FieldType, std::u16string>& observed_values,
    LogBuffer* import_log_buffer,
    ProfileImportMetadata& import_metadata) {
  AutofillProfile candidate_profile(
      i18n_model_definition::kLegacyHierarchyCountryCode);

  auto country_it = observed_values.find(ADDRESS_HOME_COUNTRY);
  if (country_it != observed_values.end()) {
    // Try setting the collected country value into the profile and report
    // invalid country if the operation failed.
    candidate_profile.SetInfoWithVerificationStatus(
        ADDRESS_HOME_COUNTRY, country_it->second, client_->GetAppLocale(),
        VerificationStatus::kObserved);

    // Track the validity of the entered country for metrics.
    import_metadata.observed_invalid_country =
        !candidate_profile.HasRawInfo(ADDRESS_HOME_COUNTRY);
  }

  // When setting a phone number, the region is deduced from the profile's
  // country or the app locale. For the variation country code to take
  // precedence over the app locale, country code complemention needs to happen
  // before `SetPhoneNumber()`.
  import_metadata.did_complement_country =
      ComplementCountry(candidate_profile, import_log_buffer);

  // We only set complete phone, so aggregate phone parts in these vars and set
  // complete at the end.
  PhoneNumber::PhoneCombineHelper combined_phone;

  // Populate the profile with the collected values. Note that this is after the
  // profile's country has been set to make sure the correct address
  // representation is used.
  for (const auto& [type, value] : observed_values) {
    // The profile country has already been established by this point. It's
    // ignored here to avoid re-setting up a potentially invalid country that
    // was present in the form.
    if (type == ADDRESS_HOME_COUNTRY) {
      continue;
    }
    if (GroupTypeOfFieldType(type) == FieldTypeGroup::kPhone) {
      // We need to store phone data in the variables, before building the whole
      // number at the end.
      combined_phone.SetInfo(type, value);
    } else {
      candidate_profile.SetInfoWithVerificationStatus(
          type, value, client_->GetAppLocale(), VerificationStatus::kObserved);
    }
  }

  if (!SetPhoneNumber(candidate_profile, combined_phone)) {
    candidate_profile.ClearFields({PHONE_HOME_WHOLE_NUMBER});
    import_metadata.phone_import_status = PhoneImportStatus::kInvalid;
    LOG_AF(import_log_buffer)
        << LogMessage::kImportAddressProfileFromFormRemoveInvalidValue
        << "Phone number." << CTag{};
  } else if (!combined_phone.IsEmpty()) {
    import_metadata.phone_import_status = PhoneImportStatus::kValid;
  }
  return candidate_profile;
}

base::flat_map<FieldType, std::u16string>
FormDataImporter::GetAddressObservedFieldValues(
    base::span<const AutofillField* const> section_fields,
    ProfileImportMetadata& import_metadata,
    LogBuffer* import_log_buffer,
    bool& has_invalid_field_types,
    bool& has_multiple_distinct_email_addresses,
    bool& has_address_related_fields) const {
  AutofillPlusAddressDelegate* plus_address_delegate =
      client_->GetPlusAddressDelegate();
  base::flat_map<FieldType, ValueForImport> preceding_values;

  // Tracks if subsequent phone number fields should be ignored,
  // since they do not belong to the first phone number in the form.
  bool ignore_phone_number_fields = false;

  // Go through each |form| field and attempt to constitute a valid profile.
  for (const AutofillField* const field : section_fields) {
    std::u16string value = field->value_for_import();
    base::TrimWhitespace(value, base::TRIM_ALL, &value);

    // If we don't know the type of the field, or the user hasn't entered any
    // information into the field, then skip it.
    if (!field->IsFieldFillable() || value.empty()) {
      continue;
    }
    // If the field was filled with a fallback type, skip it in order to not
    // introduce noise to the map's data, as this would add an entry for
    // field type X with a value retrieved from another field type Y.
    if (field->WasAutofilledWithFallback()) {
      continue;
    }
    // When the experimental plus addresses feature is enabled, and the value is
    // a plus address, exclude it from the resulting address profile.
    if (plus_address_delegate &&
        (plus_address_delegate->IsPlusAddress(base::UTF16ToUTF8(value)) ||
         plus_address_delegate->MatchesPlusAddressFormat(value))) {
      continue;
    }

    FieldType field_type = field->Type().GetStorableType();
    // Only address types are relevant in this function, other types are treated
    // in different flows.
    if (!IsAddressType(field_type)) {
      continue;
    }
    has_address_related_fields = true;

    // There can be multiple email fields (e.g. in the case of 'confirm email'
    // fields) but they must all contain the same value, else the profile is
    // invalid.
    if (field_type == EMAIL_ADDRESS) {
      auto email_it = preceding_values.find(EMAIL_ADDRESS);
      if (email_it != preceding_values.end() &&
          email_it->second.value_for_import != value) {
        LOG_AF(import_log_buffer)
            << LogMessage::kImportAddressProfileFromFormFailed
            << "Multiple different email addresses present." << CTag{};
        has_multiple_distinct_email_addresses = true;
      }
    }
    std::optional<std::u16string> selected_option_value = std::nullopt;
    if (base::optional_ref<const SelectOption> o = field->selected_option()) {
      selected_option_value = o->value;
    }
    // If the field type and |value| don't pass basic validity checks then
    // abandon the import.
    if (!IsValidFieldTypeAndValue(
            preceding_values, field_type,
            {.value_for_import = value,
             .selected_option_value = selected_option_value},
            import_log_buffer)) {
      has_invalid_field_types = true;
    }
    // Found phone number component field.
    // TODO(crbug.com/40735892) Remove feature check when launched.
    if (GroupTypeOfFieldType(field_type) == FieldTypeGroup::kPhone &&
        base::FeatureList::IsEnabled(
            features::kAutofillEnableImportWhenMultiplePhoneNumbers)) {
      if (ignore_phone_number_fields) {
        continue;
      }
      // Each phone number related type only occurs once per number. Seeing a
      // type a second time implies that it belongs to a new number. Since
      // Autofill currently supports storing only one phone number per profile,
      // ignore this and all subsequent phone number fields.
      if (preceding_values.contains(field_type)) {
        ignore_phone_number_fields = true;
        continue;
      }
    }
    // Ensure that for <select> fields, the selected option's displayed text
    // (which is typically user-friendly) is prioritized over the potentially
    // less readable option value that might be present in a corresponding
    // <input> field.
    if (!preceding_values.contains(field_type) ||
        !preceding_values[field_type].selected_option_value.has_value() ||
        selected_option_value.has_value()) {
      preceding_values.insert_or_assign(
          field_type, ValueForImport{.value_for_import = std::move(value),
                                     .selected_option_value =
                                         std::move(selected_option_value)});
    }

    if (field->parsed_autocomplete()) {
      import_metadata.did_import_from_unrecognized_autocomplete_field |=
          field->parsed_autocomplete()->field_type ==
          HtmlFieldType::kUnrecognized;
    }
  }
  return base::MakeFlatMap<FieldType, std::u16string>(
      preceding_values, {}, [](const std::pair<FieldType, ValueForImport>& p) {
        return std::make_pair(p.first, p.second.value_for_import);
      });
}

bool FormDataImporter::ExtractAddressProfileFromSection(
    base::span<const AutofillField* const> section_fields,
    const GURL& source_url,
    std::vector<FormDataImporter::ExtractedAddressProfile>*
        extracted_address_profiles,
    LogBuffer* import_log_buffer) {
  // Tracks if the form section contains multiple distinct email addresses.
  bool has_multiple_distinct_email_addresses = false;

  // Tracks if the form section contains an invalid types.
  bool has_invalid_field_types = false;

  // Metadata about the way we construct candidate_profile.
  ProfileImportMetadata import_metadata;
  import_metadata.origin = url::Origin::Create(source_url);

  // Tracks if any of the fields belongs to FormType::kAddressForm.
  bool has_address_related_fields = false;

  // Stores the values collected for each related `FieldType`. Used as
  // well to detect and discard address forms with multiple fields of the same
  // type.
  base::flat_map<FieldType, std::u16string> observed_field_values =
      GetAddressObservedFieldValues(section_fields, import_metadata,
                                    import_log_buffer, has_invalid_field_types,
                                    has_multiple_distinct_email_addresses,
                                    has_address_related_fields);

  // The candidate for profile import.
  AutofillProfile candidate_profile = ConstructProfileFromObservedValues(
      observed_field_values, import_log_buffer, import_metadata);

  // After ensuring the correct country is set on the profile, we can search for
  // any synthesized nodes. If any of these exist, we'll exclude the profile
  // from the import process
  bool has_synthesized_types = HasSynthesizedTypes(
      observed_field_values, candidate_profile.GetAddressCountryCode());

  // This is done prior to checking the validity of the profile, because multi-
  // step import profile merging requires the profile to be finalized. Ideally
  // we would return false here if it fails, but that breaks the metrics.
  bool finalized_import = candidate_profile.FinalizeAfterImport();

  // Reject the profile if the validation requirements are not met.
  // `ValidateNonEmptyValues()` goes first to collect metrics.
  bool has_invalid_information =
      !ValidateNonEmptyValues(candidate_profile, import_log_buffer) ||
      has_multiple_distinct_email_addresses || has_invalid_field_types ||
      has_synthesized_types;

  // Profiles with valid information qualify for multi-step imports.
  // This requires the profile to be finalized to apply the merging logic.
  if (finalized_import && has_address_related_fields &&
      !has_invalid_information) {
    multistep_importer_.ProcessMultiStepImport(candidate_profile,
                                               import_metadata);
  }

  // This relies on the profile's country code and must be done strictly after
  // `ComplementCountry()`.
  RemoveInaccessibleProfileValues(candidate_profile);

  // Do not import a profile if any of the requirements is violated.
  // `IsMinimumAddress()` goes first, since it logs to autofill-internals.
  bool all_fulfilled = IsMinimumAddress(candidate_profile, import_log_buffer) &&
                       !has_invalid_information;

  // Collect metrics regarding the requirements for an address profile import.
  autofill_metrics::LogAddressFormImportRequirementMetric(candidate_profile);
  autofill_metrics::LogAddressFormImportRequirementMetric(
      has_multiple_distinct_email_addresses
          ? AddressImportRequirement::kEmailAddressUniqueRequirementViolated
          : AddressImportRequirement::kEmailAddressUniqueRequirementFulfilled);
  autofill_metrics::LogAddressFormImportRequirementMetric(
      has_invalid_field_types
          ? AddressImportRequirement::kNoInvalidFieldTypesRequirementViolated
          : AddressImportRequirement::kNoInvalidFieldTypesRequirementFulfilled);
  autofill_metrics::LogAddressFormImportRequirementMetric(
      has_synthesized_types
          ? AddressImportRequirement::kNoSythesizedTypesRequirementViolated
          : AddressImportRequirement::kNoSythesizedTypesRequirementFulfilled);
  autofill_metrics::LogAddressFormImportRequirementMetric(
      import_metadata.observed_invalid_country
          ? AddressImportRequirement::kCountryValidRequirementViolated
          : AddressImportRequirement::kCountryValidRequirementFulfilled);
  autofill_metrics::LogAddressFormImportRequirementMetric(
      all_fulfilled ? AddressImportRequirement::kOverallRequirementFulfilled
                    : AddressImportRequirement::kOverallRequirementViolated);

  if (!finalized_import || !all_fulfilled) {
    return false;
  }

  // At this stage, the saving of the profile can only be omitted by the
  // incognito mode but the import is not triggered if the browser is in the
  // incognito mode.
  DCHECK(!client_->IsOffTheRecord());

  ExtractedAddressProfile extracted_address_profile;
  extracted_address_profile.profile = candidate_profile;
  extracted_address_profile.url = source_url;
  extracted_address_profile.all_requirements_fulfilled = all_fulfilled;
  extracted_address_profile.import_metadata = import_metadata;
  extracted_address_profiles->push_back(std::move(extracted_address_profile));

  // Return true if a complete importable profile was found.
  return all_fulfilled;
}

bool FormDataImporter::ProcessExtractedAddressProfiles(
    const std::vector<FormDataImporter::ExtractedAddressProfile>&
        extracted_address_profiles,
    bool allow_prompt,
    ukm::SourceId ukm_source_id) {
  int imported_profiles = 0;

  // `allow_prompt` is true if no credit card or IBAN prompt was shown. If it is
  // true, we know there is no UI currently displaying, so we can display UI to
  // import addresses. If it is false, we should not display UI to import
  // addresses due to a possible dialog or bubble conflict.
  if (allow_prompt) {
    for (const auto& candidate : extracted_address_profiles) {
      // First try to import a single complete profile.
      if (!candidate.all_requirements_fulfilled) {
        continue;
      }
      address_profile_save_manager_->ImportProfileFromForm(
          candidate.profile, client_->GetAppLocale(), candidate.url,
          ukm_source_id, /*allow_only_silent_updates=*/false,
          candidate.import_metadata);
      // Limit the number of importable profiles to 2.
      if (++imported_profiles >= 2) {
        return true;
      }
    }
  }
  // If a profile was already imported, do not try to use partial profiles for
  // silent updates.
  if (imported_profiles > 0) {
    return true;
  }
  // Otherwise try again but restrict the import to silent updates.
  for (const auto& candidate : extracted_address_profiles) {
    // First try to import a single complete profile.
    address_profile_save_manager_->ImportProfileFromForm(
        candidate.profile, client_->GetAppLocale(), candidate.url,
        ukm_source_id, /*allow_only_silent_updates=*/true,
        candidate.import_metadata);
  }
  return false;
}

bool FormDataImporter::ProcessExtractedCreditCard(
    const FormStructure& submitted_form,
    const std::optional<CreditCard>& extracted_credit_card,
    bool is_credit_card_upstream_enabled,
    ukm::SourceId ukm_source_id) {
  // If a flow without interactive authentication was completed and the user
  // didn't update the result that was filled into the form, re-auth opt-in flow
  // might be offered.
  if (auto* mandatory_reauth_manager =
          client_->GetPaymentsAutofillClient()
              ->GetOrCreatePaymentsMandatoryReauthManager();
      credit_card_import_type_ != CreditCardImportType::kNewCard &&
      mandatory_reauth_manager &&
      mandatory_reauth_manager->ShouldOfferOptin(
          payment_method_type_if_non_interactive_authentication_flow_completed_)) {
    payment_method_type_if_non_interactive_authentication_flow_completed_
        .reset();
    mandatory_reauth_manager->StartOptInFlow();
    return true;
  }

  // If a virtual card was extracted from the form, return as we do not do
  // anything with virtual cards beyond this point.
  if (credit_card_import_type_ == CreditCardImportType::kVirtualCard) {
    return false;
  }

  // Do not offer upload save for google domain.
  if (net::HasGoogleHost(submitted_form.main_frame_origin().GetURL()) &&
      is_credit_card_upstream_enabled) {
    return false;
  }

  if (client_->GetPaymentsAutofillClient()->GetVirtualCardEnrollmentManager() &&
      ShouldOfferVirtualCardEnrollment(extracted_credit_card,
                                       fetched_card_instrument_id_)) {
    client_->GetPaymentsAutofillClient()
        ->GetVirtualCardEnrollmentManager()
        ->InitVirtualCardEnroll(*extracted_credit_card,
                                VirtualCardEnrollmentSource::kDownstream);
    return true;
  }

  // Proceed with card or CVC saving if applicable.
  return extracted_credit_card &&
         credit_card_save_manager_->ProceedWithSavingIfApplicable(
             submitted_form, *extracted_credit_card, credit_card_import_type_,
             is_credit_card_upstream_enabled, ukm_source_id);
}

bool FormDataImporter::ProcessIbanImportCandidate(Iban& extracted_iban) {
  // If a flow where there was no interactive authentication was completed,
  // re-auth opt-in flow might be offered.
  if (auto* mandatory_reauth_manager =
          client_->GetPaymentsAutofillClient()
              ->GetOrCreatePaymentsMandatoryReauthManager();
      mandatory_reauth_manager &&
      mandatory_reauth_manager->ShouldOfferOptin(
          payment_method_type_if_non_interactive_authentication_flow_completed_)) {
    payment_method_type_if_non_interactive_authentication_flow_completed_
        .reset();
    mandatory_reauth_manager->StartOptInFlow();
    return true;
  }

  return iban_save_manager_->AttemptToOfferSave(extracted_iban);
}

std::optional<CreditCard> FormDataImporter::ExtractCreditCard(
    const FormStructure& form) {
  // The candidate for credit card import. There are many ways for the candidate
  // to be rejected as indicated by the `return std::nullopt` statements below.
  auto [candidate, form_has_duplicate_cc_type] =
      ExtractCreditCardFromForm(form);
  if (form_has_duplicate_cc_type) {
    return std::nullopt;
  }

  if (candidate.IsValid()) {
    AutofillMetrics::LogSubmittedCardStateMetric(
        AutofillMetrics::HAS_CARD_NUMBER_AND_EXPIRATION_DATE);
  } else {
    if (candidate.HasValidCardNumber()) {
      AutofillMetrics::LogSubmittedCardStateMetric(
          AutofillMetrics::HAS_CARD_NUMBER_ONLY);
    }
    if (candidate.HasValidExpirationDate()) {
      AutofillMetrics::LogSubmittedCardStateMetric(
          AutofillMetrics::HAS_EXPIRATION_DATE_ONLY);
    }
  }

  // Cards with invalid expiration dates can be uploaded due to the existence of
  // the expiration date fix flow. However, cards with invalid card numbers must
  // still be ignored.
  if (!candidate.HasValidCardNumber()) {
    return std::nullopt;
  }

  // If the extracted card is a known virtual card, return the extracted card.
  if (fetched_virtual_cards_.contains(candidate.LastFourDigits())) {
    credit_card_import_type_ = CreditCardImportType::kVirtualCard;
    return candidate;
  }

  // Can import one valid card per form. Start by treating it as kNewCard, but
  // overwrite this type if we discover it is already a local or server card.
  credit_card_import_type_ = CreditCardImportType::kNewCard;

  // Attempt to merge with an existing local credit card without presenting a
  // prompt.
  for (const CreditCard* local_card :
       payments_data_manager().GetLocalCreditCards()) {
    // Make a local copy so that the data in `local_credit_cards_` isn't
    // modified directly by the UpdateFromImportedCard() call.
    CreditCard maybe_updated_card = *local_card;
    if (maybe_updated_card.UpdateFromImportedCard(candidate,
                                                  client_->GetAppLocale())) {
      payments_data_manager().UpdateCreditCard(maybe_updated_card);
      credit_card_import_type_ = CreditCardImportType::kLocalCard;
      // Update `candidate` to reflect all the details of the updated card.
      // `UpdateFromImportedCard` has updated all values except for the
      // extracted CVC, as we will not update that until later after prompting
      // the user to store their CVC.
      std::u16string extracted_cvc = candidate.cvc();
      candidate = maybe_updated_card;
      candidate.set_cvc(extracted_cvc);
    }
  }

  // Return `candidate` if no server card is matched but the card in the form is
  // a valid card.
  return TryMatchingExistingServerCard(candidate);
}

std::optional<CreditCard> FormDataImporter::TryMatchingExistingServerCard(
    const CreditCard& candidate) {
  // Used for logging purposes later if we found a matching masked server card
  // with the same last four digits, but different expiration date as
  // `candidate`, and we treat it as a new card.
  bool same_last_four_but_different_expiration_date = false;

  for (const CreditCard* server_card :
       payments_data_manager().GetServerCreditCards()) {
    if (!server_card->HasSameNumberAs(candidate)) {
      continue;
    }

    // Cards with invalid expiration dates can be uploaded due to the existence
    // of the expiration date fix flow, however, since a server card with same
    // number is found, the imported card is treated as invalid card, abort
    // importing.
    if (!candidate.HasValidExpirationDate()) {
      return std::nullopt;
    }

    // Only return the masked server card if both the last four digits and
    // expiration date match.
    if (server_card->HasSameExpirationDateAs(candidate)) {
      AutofillMetrics::LogSubmittedServerCardExpirationStatusMetric(
          AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_MATCHED);

      // Return that we found a masked server card with matching last four
      // digits and copy over the user entered CVC so that future processing
      // logic check if CVC upload save should be offered.
      CreditCard server_card_with_cvc = *server_card;
      server_card_with_cvc.set_cvc(candidate.cvc());

      // If `credit_card_import_type_` was local card, then a local card was
      // extracted from the form. If a server card is now also extracted from
      // the form, the duplicate local and server card case is detected.
      if (credit_card_import_type_ == CreditCardImportType::kLocalCard) {
        credit_card_import_type_ =
            CreditCardImportType::kDuplicateLocalServerCard;
      } else {
        credit_card_import_type_ = CreditCardImportType::kServerCard;
      }
      return server_card_with_cvc;
    } else {
      // Keep track of the fact that we found a server card with matching
      // last four digits as `candidate`, but with a different expiration
      // date. If we do not end up finding a masked server card with
      // matching last four digits and the same expiration date as
      // `candidate`, we will later use
      // `same_last_four_but_different_expiration_date` for logging
      // purposes.
      same_last_four_but_different_expiration_date = true;
    }
  }

  // The only case that this is true is that we found a masked server card has
  // the same number as `candidate`, but with different expiration dates. Thus
  // we want to log this information once.
  if (same_last_four_but_different_expiration_date) {
    AutofillMetrics::LogSubmittedServerCardExpirationStatusMetric(
        AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH);
  }

  return candidate;
}

std::optional<Iban> FormDataImporter::ExtractIban(const FormStructure& form) {
  Iban candidate_iban = ExtractIbanFromForm(form);
  if (candidate_iban.value().empty()) {
    return std::nullopt;
  }

  // Sets the `kAutofillHasSeenIban` pref to true indicating that the user has
  // submitted a form with an IBAN, which indicates that the user is familiar
  // with IBANs as a concept. We set the pref so that even if the user travels
  // to a country where IBAN functionality is not typically used, they will
  // still be able to save new IBANs from the settings page using this pref.
  payments_data_manager().SetAutofillHasSeenIban();

  return candidate_iban;
}

FormDataImporter::ExtractCreditCardFromFormResult
FormDataImporter::ExtractCreditCardFromForm(const FormStructure& form) {
  // Populated by the lambdas below.
  ExtractCreditCardFromFormResult result;
  std::string app_locale = client_->GetAppLocale();

  // Populates `result` from `field` if it's a credit card field.
  // For example, if `field` contains credit card number, this sets the number
  // of `result.card` to the `field`'s value.
  auto extract_if_credit_card_field = [&result,
                                       app_locale](const AutofillField& field) {
    std::u16string value = [&field] {
      if (field.Type().GetStorableType() == FieldType::CREDIT_CARD_NUMBER) {
        // Credit card numbers are sometimes obfuscated on form submission.
        // Therefore, we give preference to the user input over the field value.
        std::u16string user_input = field.user_input();
        base::TrimWhitespace(user_input, base::TRIM_ALL);
        if (!user_input.empty()) {
          return user_input;
        }
      }
      return field.value_for_import();
    }();
    base::TrimWhitespace(value, base::TRIM_ALL);

    // If we don't know the type of the field, or the user hasn't entered any
    // information into the field, then skip it.
    if (value.empty() || field.Type().group() != FieldTypeGroup::kCreditCard) {
      return;
    }
    std::u16string old_value = result.card.GetInfo(field.Type(), app_locale);
    if (field.form_control_type() == FormControlType::kInputMonth) {
      // If |field| is an HTML5 month input, handle it as a special case.
      DCHECK_EQ(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                field.Type().GetStorableType());
      result.card.SetInfoForMonthInputType(value);
    } else {
      // If the credit card number offset is within the range of the old value,
      // replace the portion of the old value with the value from the current
      // field. For example:
      // old value: '1234', offset: 4, new value:'5678', result: '12345678'
      // old value: '12345678', offset: 4, new value:'0000', result: '12340000'
      if (field.credit_card_number_offset() > 0 &&
          field.credit_card_number_offset() <= old_value.size() &&
          base::FeatureList::IsEnabled(
              features::kAutofillFixSplitCreditCardImport)) {
        value = old_value.replace(field.credit_card_number_offset(),
                                  value.size(), value);
      }
      bool saved = result.card.SetInfo(field.Type(), value, app_locale);
      if (!saved && field.IsSelectElement()) {
        // Saving with the option text (here `value`) may fail for the
        // expiration month. Attempt to save with the option value. First find
        // the index of the option text in the select options and try the
        // corresponding value.
        if (auto it =
                std::ranges::find(field.options(), value, &SelectOption::text);
            it != field.options().end()) {
          result.card.SetInfo(field.Type(), it->value, app_locale);
        }
      }
    }

    std::u16string new_value = result.card.GetInfo(field.Type(), app_locale);
    // Skip duplicate field check if the field is a split credit card
    // number field.
    bool skip_duplication_check =
        field.Type().GetStorableType() == FieldType::CREDIT_CARD_NUMBER &&
        field.credit_card_number_offset() > 0 &&
        base::FeatureList::IsEnabled(
            features::kAutofillFixSplitCreditCardImport);
    result.has_duplicate_credit_card_field_type |=
        !skip_duplication_check && !old_value.empty() && old_value != new_value;
  };

  // Populates `result` from `fields` that satisfy `pred`, and erases those
  // fields. Afterwards, it also erases all remaining fields whose type is now
  // present in `result.card`.
  // For example, if a `CREDIT_CARD_NAME_FULL` field matches `pred`, this
  // function sets the credit card first, last, and full name and erases
  // all `fields` of type `CREDIT_CARD_NAME_{FULL,FIRST,LAST}`.
  auto extract_data_and_remove_field_if =
      [&result, &extract_if_credit_card_field, &app_locale](
          std::vector<const AutofillField*>& fields, const auto& pred) {
        for (const AutofillField* field : fields) {
          if (std::invoke(pred, *field)) {
            extract_if_credit_card_field(*field);
          }
        }
        std::erase_if(fields, [&](const AutofillField* field) {
          return std::invoke(pred, *field) ||
                 !result.card.GetInfo(field->Type(), app_locale).empty();
        });
      };

  // We split the fields into three priority groups: user-typed values,
  // autofilled values, other values. The duplicate-value recognition is limited
  // to values of the respective group.
  //
  // Suppose the user first autofills a form, including invisible fields. Then
  // they edited a visible fields. The priority groups ensure that the invisible
  // field does not prevent credit card import.
  std::vector<const AutofillField*> fields;
  fields.reserve(form.fields().size());
  for (const std::unique_ptr<AutofillField>& field : form.fields()) {
    fields.push_back(field.get());
  }
  extract_data_and_remove_field_if(fields, &AutofillField::is_user_edited);
  extract_data_and_remove_field_if(fields, &AutofillField::is_autofilled);
  extract_data_and_remove_field_if(fields, [](const auto&) { return true; });
  return result;
}

Iban FormDataImporter::ExtractIbanFromForm(const FormStructure& form) {
  // Creates an IBAN candidate with `kUnknown` record type as it is currently
  // unknown if this IBAN already exists locally or on the server.
  Iban candidate_iban;
  for (const auto& field : form) {
    const std::u16string& value = field->value_for_import();
    if (!field->IsFieldFillable() || value.empty()) {
      continue;
    }
    FieldType field_type = field->Type().GetStorableType();
    if (field_type == IBAN_VALUE && Iban::IsValid(value)) {
      candidate_iban.set_value(value);
      break;
    }
  }
  return candidate_iban;
}

void FormDataImporter::OnAddressDataChanged() {
  multistep_importer_.OnAddressDataChanged(address_data_manager());
}

void FormDataImporter::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  multistep_importer_.OnBrowsingHistoryCleared(deletion_info);
  form_associator_.OnBrowsingHistoryCleared(deletion_info);
}

void FormDataImporter::
    SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
        std::optional<NonInteractivePaymentMethodType>
            payment_method_type_if_non_interactive_authentication_flow_completed) {
  payment_method_type_if_non_interactive_authentication_flow_completed_ =
      payment_method_type_if_non_interactive_authentication_flow_completed;
}

AddressDataManager& FormDataImporter::address_data_manager() {
  return client_->GetPersonalDataManager().address_data_manager();
}

PaymentsDataManager& FormDataImporter::payments_data_manager() {
  return client_->GetPersonalDataManager().payments_data_manager();
}

}  // namespace autofill
