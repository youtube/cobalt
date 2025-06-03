// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/contact_info_sync_util.h"

#include "base/hash/hash.h"
#include "base/memory/raw_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/profile_token_quality.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {

using sync_pb::ContactInfoSpecifics;

// Converts the verification status representation used in AutofillProfile to
// the one used in ContactInfoSpecifics.
ContactInfoSpecifics::VerificationStatus
ConvertProfileToSpecificsVerificationStatus(VerificationStatus status) {
  switch (status) {
    case VerificationStatus::kNoStatus:
      return ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED;
    case VerificationStatus::kParsed:
      return ContactInfoSpecifics::PARSED;
    case VerificationStatus::kFormatted:
      return ContactInfoSpecifics::FORMATTED;
    case VerificationStatus::kObserved:
      return ContactInfoSpecifics::OBSERVED;
    case VerificationStatus::kUserVerified:
      return ContactInfoSpecifics::USER_VERIFIED;
    case VerificationStatus::kServerParsed:
      return ContactInfoSpecifics::SERVER_PARSED;
  }
}

// Converts the verification status representation used in
// ContactInfoSpecifics to the one used in AutofillProfile.
VerificationStatus ConvertSpecificsToProfileVerificationStatus(
    ContactInfoSpecifics::VerificationStatus status) {
  switch (status) {
    case ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED:
      return VerificationStatus::kNoStatus;
    case ContactInfoSpecifics::PARSED:
      return VerificationStatus::kParsed;
    case ContactInfoSpecifics::FORMATTED:
      return VerificationStatus::kFormatted;
    case ContactInfoSpecifics::OBSERVED:
      return VerificationStatus::kObserved;
    case ContactInfoSpecifics::USER_VERIFIED:
      return VerificationStatus::kUserVerified;
    case ContactInfoSpecifics::SERVER_PARSED:
      return VerificationStatus::kServerParsed;
  }
}

class EntryTokenDeleter {
 public:
  bool Delete(ContactInfoSpecifics::StringToken* token) {
    // Delete the supported metadata from the token and delete the complete
    // metadata message when there are no fields left.
    if (DeleteMetadata(token->mutable_metadata())) {
      token->clear_metadata();
    }

    token->clear_value();
    return token->ByteSize() == 0;
  }

  bool Delete(ContactInfoSpecifics::IntegerToken* token) {
    // Delete the supported metadata from the token and delete the complete
    // metadata message when there are no fields left.
    if (DeleteMetadata(token->mutable_metadata())) {
      token->clear_metadata();
    }

    token->clear_value();
    return token->ByteSize() == 0;
  }

 private:
  bool DeleteMetadata(ContactInfoSpecifics::TokenMetadata* metadata) {
    metadata->clear_status();
    metadata->clear_observations();
    metadata->clear_value_hash();
    return metadata->ByteSize() == 0;
  }
};

// Returns a hash of the `profile`'s value for the given `type`. This hash is
// not persisted on the Autofill side, but used to detect changes by external
// integrators when the data gets synced back.
// Since the uploaded data contains the raw value too, this is not a privacy
// concern.
uint32_t GetProfileValueHash(const AutofillProfile& profile,
                             ServerFieldType type) {
  return base::PersistentHash(base::UTF16ToUTF8(profile.GetRawInfo(type)));
}

}  // namespace

// Helper class to simplify setting the value and metadata of
// ContactInfoSpecifics String- and IntegerTokens from an AutofillProfile.
// Outside of the anonymous namespace to be befriended by `ProfileTokenQuality`.
class ContactInfoEntryDataSetter {
 public:
  explicit ContactInfoEntryDataSetter(const AutofillProfile& profile)
      : profile_(profile) {}

  void Set(ContactInfoSpecifics::StringToken* token,
           ServerFieldType type) const {
    token->set_value(base::UTF16ToUTF8(profile_->GetRawInfo(type)));
    SetMetadata(token->mutable_metadata(), type);
  }

  void Set(ContactInfoSpecifics::IntegerToken* token,
           ServerFieldType type) const {
    token->set_value(profile_->GetRawInfoAsInt(type));
    SetMetadata(token->mutable_metadata(), type);
  }

 private:
  void SetMetadata(ContactInfoSpecifics::TokenMetadata* metadata,
                   ServerFieldType type) const {
    metadata->set_status(ConvertProfileToSpecificsVerificationStatus(
        profile_->GetVerificationStatus(type)));
    if (!base::FeatureList::IsEnabled(
            features::kAutofillTrackProfileTokenQuality)) {
      return;
    }
    if (auto observations = profile_->token_quality().observations_.find(type);
        observations != profile_->token_quality().observations_.end()) {
      for (const ProfileTokenQuality::Observation& observation :
           observations->second) {
        sync_pb::ContactInfoSpecifics::Observation* proto_observation =
            metadata->add_observations();
        proto_observation->set_type(observation.type);
        proto_observation->set_form_hash(observation.form_hash.value());
      }
    }
    metadata->set_value_hash(GetProfileValueHash(*profile_, type));
  }

  const raw_ref<const AutofillProfile> profile_;
};

// Helper class to set the info and verification status of an AutofillProfile
// from ContactInfoSpecifics String- and Integer tokens.
// Outside of the anonymous namespace to be befriended by `ProfileTokenQuality`.
class ContactInfoProfileSetter {
 public:
  explicit ContactInfoProfileSetter(AutofillProfile& profile)
      : profile_(profile) {}

  void Set(const ContactInfoSpecifics::StringToken& token,
           ServerFieldType type) {
    profile_->SetRawInfoWithVerificationStatus(
        type, base::UTF8ToUTF16(token.value()),
        ConvertSpecificsToProfileVerificationStatus(token.metadata().status()));
    SetObservations(token.metadata(), type);
  }

  void Set(const ContactInfoSpecifics::IntegerToken& token,
           ServerFieldType type) {
    profile_->SetRawInfoAsIntWithVerificationStatus(
        type, token.value(),
        ConvertSpecificsToProfileVerificationStatus(token.metadata().status()));
    SetObservations(token.metadata(), type);
  }

 private:
  void SetObservations(const ContactInfoSpecifics::TokenMetadata& metadata,
                       ServerFieldType type) const {
    if (metadata.observations().empty() ||
        !base::FeatureList::IsEnabled(
            features::kAutofillTrackProfileTokenQuality)) {
      return;
    }
    // If the value of the `type` was changed by an external integrator, the
    // associated observations are no longer valid. In this case, they are left
    // empty and effectively dropped.
    // There is no need to re-upload to sync without the observations, since
    // all Chrome clients drop them by themselves. If new observations get
    // collected by another Chrome client, these get synced and other clients
    // overwrite their local state unconditionally.
    // Not syncing back has the additional advantage that it makes deprecating
    // these fields (should this ever happen) easier.
    if (GetProfileValueHash(*profile_, type) == metadata.value_hash()) {
      auto& observations = profile_->token_quality().observations_[type];
      CHECK(observations.empty());
      for (const sync_pb::ContactInfoSpecifics::Observation& proto_observation :
           metadata.observations()) {
        observations.emplace_back(proto_observation.type(),
                                  ProfileTokenQuality::FormSignatureHash(
                                      proto_observation.form_hash()));
      }
    }
  }

  const raw_ref<AutofillProfile> profile_;
};

sync_pb::ContactInfoSpecifics ContactInfoSpecificsFromAutofillProfile(
    const AutofillProfile& profile,
    const sync_pb::ContactInfoSpecifics& base_contact_info_specifics) {
  sync_pb::ContactInfoSpecifics specifics = base_contact_info_specifics;

  specifics.set_guid(profile.guid());
  specifics.set_use_count(profile.use_count());
  specifics.set_use_date_unix_epoch_seconds(
      (profile.use_date() - base::Time::UnixEpoch()).InSeconds());
  specifics.set_date_modified_unix_epoch_seconds(
      (profile.modification_date() - base::Time::UnixEpoch()).InSeconds());
  specifics.set_language_code(profile.language_code());
  specifics.set_profile_label(profile.profile_label());

  specifics.set_initial_creator_id(profile.initial_creator_id());
  specifics.set_last_modifier_id(profile.last_modifier_id());

  ContactInfoEntryDataSetter s(profile);
  // Set name-related values and statuses.
  s.Set(specifics.mutable_name_honorific(), NAME_HONORIFIC_PREFIX);
  s.Set(specifics.mutable_name_first(), NAME_FIRST);
  s.Set(specifics.mutable_name_middle(), NAME_MIDDLE);
  s.Set(specifics.mutable_name_last(), NAME_LAST);
  s.Set(specifics.mutable_name_last_first(), NAME_LAST_FIRST);
  s.Set(specifics.mutable_name_last_conjunction(), NAME_LAST_CONJUNCTION);
  s.Set(specifics.mutable_name_last_second(), NAME_LAST_SECOND);
  s.Set(specifics.mutable_name_full(), NAME_FULL);
  s.Set(specifics.mutable_name_full_with_honorific(),
        NAME_FULL_WITH_HONORIFIC_PREFIX);

  // Set address-related values and statuses.
  s.Set(specifics.mutable_address_city(), ADDRESS_HOME_CITY);
  s.Set(specifics.mutable_address_state(), ADDRESS_HOME_STATE);
  s.Set(specifics.mutable_address_zip(), ADDRESS_HOME_ZIP);
  s.Set(specifics.mutable_address_country(), ADDRESS_HOME_COUNTRY);
  s.Set(specifics.mutable_address_street_address(),
        ADDRESS_HOME_STREET_ADDRESS);
  s.Set(specifics.mutable_address_sorting_code(), ADDRESS_HOME_SORTING_CODE);
  s.Set(specifics.mutable_address_dependent_locality(),
        ADDRESS_HOME_DEPENDENT_LOCALITY);
  s.Set(specifics.mutable_address_thoroughfare_name(),
        ADDRESS_HOME_STREET_NAME);
  s.Set(specifics.mutable_address_thoroughfare_number(),
        ADDRESS_HOME_HOUSE_NUMBER);
  s.Set(specifics.mutable_address_street_location(),
        ADDRESS_HOME_STREET_LOCATION);
  s.Set(specifics.mutable_address_subpremise_name(), ADDRESS_HOME_SUBPREMISE);
  s.Set(specifics.mutable_address_apt_num(), ADDRESS_HOME_APT_NUM);
  s.Set(specifics.mutable_address_floor(), ADDRESS_HOME_FLOOR);
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForLandmark)) {
    s.Set(specifics.mutable_address_landmark(), ADDRESS_HOME_LANDMARK);
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForBetweenStreets)) {
    s.Set(specifics.mutable_address_between_streets(),
          ADDRESS_HOME_BETWEEN_STREETS);
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForAdminLevel2)) {
    s.Set(specifics.mutable_address_admin_level_2(), ADDRESS_HOME_ADMIN_LEVEL2);
  }

  // Set email, phone and company values and statuses.
  s.Set(specifics.mutable_email_address(), EMAIL_ADDRESS);
  s.Set(specifics.mutable_company_name(), COMPANY_NAME);
  s.Set(specifics.mutable_phone_home_whole_number(), PHONE_HOME_WHOLE_NUMBER);

  // Set birthdate-related values and statuses.
  s.Set(specifics.mutable_birthdate_day(), BIRTHDATE_DAY);
  s.Set(specifics.mutable_birthdate_month(), BIRTHDATE_MONTH);
  s.Set(specifics.mutable_birthdate_year(), BIRTHDATE_4_DIGIT_YEAR);

  return specifics;
}

std::unique_ptr<syncer::EntityData>
CreateContactInfoEntityDataFromAutofillProfile(
    const AutofillProfile& profile,
    const sync_pb::ContactInfoSpecifics& base_contact_info_specifics) {
  // Profiles fall into two categories, kLocalOrSyncable and kAccount.
  // kLocalOrSyncable profiles are synced through the AutofillProfileSyncBridge,
  // while kAccount profiles are synced through the ContactInfoSyncBridge. Make
  // sure that syncing a profile through the wrong sync bridge fails early.
  if (!base::Uuid::ParseCaseInsensitive(profile.guid()).is_valid() ||
      profile.source() != AutofillProfile::Source::kAccount) {
    return nullptr;
  }

  auto entity_data = std::make_unique<syncer::EntityData>();

  entity_data->name = profile.guid();

  ContactInfoSpecifics* specifics =
      entity_data->specifics.mutable_contact_info();

  *specifics = ContactInfoSpecificsFromAutofillProfile(
      profile, base_contact_info_specifics);

  // This check verified that stripping all supported fields from the specifics
  // results in an empty record. If this is not case, most likely a new field
  // was forgotten to be added in the trimming function.
  DCHECK_EQ(0u, TrimContactInfoSpecificsDataForCaching(
                    ContactInfoSpecificsFromAutofillProfile(
                        profile, /*base_contact_info_specifics=*/{}))
                    .ByteSizeLong());

  DCHECK(AreContactInfoSpecificsValid(*specifics));
  return entity_data;
}

std::unique_ptr<AutofillProfile> CreateAutofillProfileFromContactInfoSpecifics(
    const ContactInfoSpecifics& specifics) {
  if (!AreContactInfoSpecificsValid(specifics))
    return nullptr;

  std::u16string country_name_or_code =
      base::ASCIIToUTF16(specifics.address_country().value());
  std::string country_code =
      CountryNames::GetInstance()->GetCountryCode(country_name_or_code);

  std::unique_ptr<AutofillProfile> profile = std::make_unique<AutofillProfile>(
      specifics.guid(), AutofillProfile::Source::kAccount,
      AddressCountryCode(country_code));

  profile->set_use_count(specifics.use_count());
  profile->set_use_date(base::Time::UnixEpoch() +
                        base::Seconds(specifics.use_date_unix_epoch_seconds()));
  profile->set_modification_date(
      base::Time::UnixEpoch() +
      base::Seconds(specifics.date_modified_unix_epoch_seconds()));
  profile->set_language_code(specifics.language_code());
  profile->set_profile_label(specifics.profile_label());
  profile->set_initial_creator_id(specifics.initial_creator_id());
  profile->set_last_modifier_id(specifics.last_modifier_id());

  ContactInfoProfileSetter s(*profile);
  // Set name-related values and statuses.
  s.Set(specifics.name_honorific(), NAME_HONORIFIC_PREFIX);
  s.Set(specifics.name_first(), NAME_FIRST);
  s.Set(specifics.name_middle(), NAME_MIDDLE);
  s.Set(specifics.name_last(), NAME_LAST);
  s.Set(specifics.name_last_first(), NAME_LAST_FIRST);
  s.Set(specifics.name_last_conjunction(), NAME_LAST_CONJUNCTION);
  s.Set(specifics.name_last_second(), NAME_LAST_SECOND);
  s.Set(specifics.name_full(), NAME_FULL);
  s.Set(specifics.name_full_with_honorific(), NAME_FULL_WITH_HONORIFIC_PREFIX);

  // Set address-related values and statuses.
  s.Set(specifics.address_city(), ADDRESS_HOME_CITY);
  s.Set(specifics.address_state(), ADDRESS_HOME_STATE);
  s.Set(specifics.address_zip(), ADDRESS_HOME_ZIP);
  s.Set(specifics.address_street_address(), ADDRESS_HOME_STREET_ADDRESS);
  s.Set(specifics.address_sorting_code(), ADDRESS_HOME_SORTING_CODE);
  s.Set(specifics.address_dependent_locality(),
        ADDRESS_HOME_DEPENDENT_LOCALITY);
  s.Set(specifics.address_thoroughfare_name(), ADDRESS_HOME_STREET_NAME);
  s.Set(specifics.address_thoroughfare_number(), ADDRESS_HOME_HOUSE_NUMBER);
  s.Set(specifics.address_street_location(), ADDRESS_HOME_STREET_LOCATION);
  s.Set(specifics.address_subpremise_name(), ADDRESS_HOME_SUBPREMISE);
  s.Set(specifics.address_apt_num(), ADDRESS_HOME_APT_NUM);
  s.Set(specifics.address_floor(), ADDRESS_HOME_FLOOR);
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForLandmark)) {
    s.Set(specifics.address_landmark(), ADDRESS_HOME_LANDMARK);
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForBetweenStreets)) {
    s.Set(specifics.address_between_streets(), ADDRESS_HOME_BETWEEN_STREETS);
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForAdminLevel2)) {
    s.Set(specifics.address_admin_level_2(), ADDRESS_HOME_ADMIN_LEVEL2);
  }

  // Set email, phone and company values and statuses.
  s.Set(specifics.email_address(), EMAIL_ADDRESS);
  s.Set(specifics.company_name(), COMPANY_NAME);
  s.Set(specifics.phone_home_whole_number(), PHONE_HOME_WHOLE_NUMBER);

  // Set birthdate-related values and statuses.
  s.Set(specifics.birthdate_day(), BIRTHDATE_DAY);
  s.Set(specifics.birthdate_month(), BIRTHDATE_MONTH);
  s.Set(specifics.birthdate_year(), BIRTHDATE_4_DIGIT_YEAR);

  profile->FinalizeAfterImport();
  return profile;
}

bool AreContactInfoSpecificsValid(
    const sync_pb::ContactInfoSpecifics& specifics) {
  return base::Uuid::ParseLowercase(specifics.guid()).is_valid();
}

sync_pb::ContactInfoSpecifics TrimContactInfoSpecificsDataForCaching(
    const sync_pb::ContactInfoSpecifics& contact_info_specifics) {
  sync_pb::ContactInfoSpecifics trimmed_specifics =
      sync_pb::ContactInfoSpecifics(contact_info_specifics);

  trimmed_specifics.clear_guid();
  trimmed_specifics.clear_use_count();
  trimmed_specifics.clear_use_date_unix_epoch_seconds();
  trimmed_specifics.clear_date_modified_unix_epoch_seconds();
  trimmed_specifics.clear_language_code();
  trimmed_specifics.clear_profile_label();
  trimmed_specifics.clear_initial_creator_id();
  trimmed_specifics.clear_last_modifier_id();

  EntryTokenDeleter d;
  // Delete name-related values and statuses.
  if (d.Delete(trimmed_specifics.mutable_name_honorific())) {
    trimmed_specifics.clear_name_honorific();
  }

  if (d.Delete(trimmed_specifics.mutable_name_first())) {
    trimmed_specifics.clear_name_first();
  }

  if (d.Delete(trimmed_specifics.mutable_name_middle())) {
    trimmed_specifics.clear_name_middle();
  }

  if (d.Delete(trimmed_specifics.mutable_name_last())) {
    trimmed_specifics.clear_name_last();
  }

  if (d.Delete(trimmed_specifics.mutable_name_last_first())) {
    trimmed_specifics.clear_name_last_first();
  }

  if (d.Delete(trimmed_specifics.mutable_name_last_conjunction())) {
    trimmed_specifics.clear_name_last_conjunction();
  }

  if (d.Delete(trimmed_specifics.mutable_name_last_second())) {
    trimmed_specifics.clear_name_last_second();
  }

  if (d.Delete(trimmed_specifics.mutable_name_full())) {
    trimmed_specifics.clear_name_full();
  }

  if (d.Delete(trimmed_specifics.mutable_name_full_with_honorific())) {
    trimmed_specifics.clear_name_full_with_honorific();
  }

  // Delete address-related values and statuses.;
  if (d.Delete(trimmed_specifics.mutable_address_city())) {
    trimmed_specifics.clear_address_city();
  }

  if (d.Delete(trimmed_specifics.mutable_address_state())) {
    trimmed_specifics.clear_address_state();
  }

  if (d.Delete(trimmed_specifics.mutable_address_zip())) {
    trimmed_specifics.clear_address_zip();
  }

  if (d.Delete(trimmed_specifics.mutable_address_country())) {
    trimmed_specifics.clear_address_country();
  }

  if (d.Delete(trimmed_specifics.mutable_address_street_address())) {
    trimmed_specifics.clear_address_street_address();
  }

  if (d.Delete(trimmed_specifics.mutable_address_sorting_code())) {
    trimmed_specifics.clear_address_sorting_code();
  }

  if (d.Delete(trimmed_specifics.mutable_address_dependent_locality())) {
    trimmed_specifics.clear_address_dependent_locality();
  }

  if (d.Delete(trimmed_specifics.mutable_address_thoroughfare_name())) {
    trimmed_specifics.clear_address_thoroughfare_name();
  }

  if (d.Delete(trimmed_specifics.mutable_address_thoroughfare_number())) {
    trimmed_specifics.clear_address_thoroughfare_number();
  }

  if (d.Delete(trimmed_specifics.mutable_address_street_location())) {
    trimmed_specifics.clear_address_street_location();
  }

  if (d.Delete(trimmed_specifics.mutable_address_subpremise_name())) {
    trimmed_specifics.clear_address_subpremise_name();
  }

  if (d.Delete(trimmed_specifics.mutable_address_apt_num())) {
    trimmed_specifics.clear_address_apt_num();
  }

  if (d.Delete(trimmed_specifics.mutable_address_floor())) {
    trimmed_specifics.clear_address_floor();
  }

  if (d.Delete(trimmed_specifics.mutable_address_landmark())) {
    trimmed_specifics.clear_address_landmark();
  }

  if (d.Delete(trimmed_specifics.mutable_address_between_streets())) {
    trimmed_specifics.clear_address_between_streets();
  }

  if (d.Delete(trimmed_specifics.mutable_address_admin_level_2())) {
    trimmed_specifics.clear_address_admin_level_2();
  }

  // Delete email, phone and company values and statuses.
  if (d.Delete(trimmed_specifics.mutable_email_address())) {
    trimmed_specifics.clear_email_address();
  }

  if (d.Delete(trimmed_specifics.mutable_company_name())) {
    trimmed_specifics.clear_company_name();
  }

  if (d.Delete(trimmed_specifics.mutable_phone_home_whole_number())) {
    trimmed_specifics.clear_phone_home_whole_number();
  }

  // Delete birthdate-related values and statuses.
  if (d.Delete(trimmed_specifics.mutable_birthdate_day())) {
    trimmed_specifics.clear_birthdate_day();
  }

  if (d.Delete(trimmed_specifics.mutable_birthdate_month())) {
    trimmed_specifics.clear_birthdate_month();
  }

  if (d.Delete(trimmed_specifics.mutable_birthdate_year())) {
    trimmed_specifics.clear_birthdate_year();
  }

  return trimmed_specifics;
}

}  // namespace autofill
