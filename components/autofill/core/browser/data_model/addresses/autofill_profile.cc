// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <ranges>
#include <set>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/hash/sha1.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/addresses/address.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/addresses/contact_info.h"
#include "components/autofill/core/browser/data_model/addresses/phone_number.h"
#include "components/autofill/core/browser/data_model/usage_history_information.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_token_quality.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/addresses/autofill_address_util.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/chromium/addressinput_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/autofill/android/main_autofill_jni_headers/AutofillProfile_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;

namespace autofill {

namespace {

constexpr char kAddressComponentsDefaultLocality[] = "en-US";

// Like |AutofillType::GetStorableType()|, but also returns |NAME_FULL| for
// first, middle, and last name field types, and groups phone number types
// similarly.
FieldType GetStorableTypeCollapsingGroupsForPartialType(FieldType type) {
  if (GroupTypeOfFieldType(type) == FieldTypeGroup::kName) {
    return NAME_FULL;
  }
  if (GroupTypeOfFieldType(type) == FieldTypeGroup::kPhone) {
    return PHONE_HOME_WHOLE_NUMBER;
  }
  return type;
}

// Like `GetStorableTypeCollapsingGroupsForPartialType()`, but also similarly
// groups types which include address line 1.
//
// `GetStorableTypeCollapsingGroups()` serves this purpose:
// If `ADDRESS_HOME_STREET_ADDRESS` is an excluded field, we also want to
// exclude `ADDRESS_HOME_LINE1`, because it doesn't add extra relevant
// information. Names and phone numbers also behave like this for the same
// reason. i.e. if `NAME_FIRST` is excluded, we also exclude `NAME_LAST`.
//
// `GetStorableTypeCollapsingGroupsForPartialType()` serves the purpose of
// including `NAME_FULL` in the label candidates, as a last resort, if a partial
// name field is excluded. Similar for phone numbers. For more details, check
// the comment where `GetStorableTypeCollapsingGroupsForPartialType()` is used.
// This does not apply to `ADDRESS_HOME_LINE1`, because if a field is
// `ADDRESS_HOME_STREET_ADDRESS` and we don't want to accidentally include back
// `ADDRESS_HOME_LINE1` in the label candidates.
FieldType GetStorableTypeCollapsingGroups(FieldType type,
                                          bool use_improved_labels_order) {
  if ((type == ADDRESS_HOME_LINE1 || type == ADDRESS_HOME_STREET_ADDRESS) &&
      use_improved_labels_order) {
    return ADDRESS_HOME_LINE1;
  }
  return GetStorableTypeCollapsingGroupsForPartialType(type);
}

// Returns a value that represents specificity/privacy of the given type. This
// is used for prioritizing which data types are shown in inferred labels. For
// example, if the profile is going to fill ADDRESS_HOME_ZIP, it should
// prioritize showing that over ADDRESS_HOME_STATE in the suggestion sublabel.
int SpecificityForType(FieldType type, bool use_improved_labels_order) {
  // TODO(crbug.com/380273791): Clean up after launch. To make `kDefaultOrder`
  // and `kImprovedOrder` have the same size/type, an `EMPTY_TYPE` dummy value
  // is added to the end of `kImprovedOrder`. It can be removed together with
  // the CHECK() after launch.
  static constexpr auto kDefaultOrder =
      std::to_array({ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, EMAIL_ADDRESS,
                     PHONE_HOME_WHOLE_NUMBER, NAME_FULL, ADDRESS_HOME_ZIP,
                     ADDRESS_HOME_SORTING_CODE, COMPANY_NAME, ADDRESS_HOME_CITY,
                     ADDRESS_HOME_STATE, ADDRESS_HOME_COUNTRY});
  static constexpr auto kImprovedOrder =
      std::to_array({ADDRESS_HOME_LINE1, NAME_FULL, EMAIL_ADDRESS,
                     PHONE_HOME_WHOLE_NUMBER, ADDRESS_HOME_ZIP,
                     ADDRESS_HOME_SORTING_CODE, COMPANY_NAME, ADDRESS_HOME_CITY,
                     ADDRESS_HOME_STATE, ADDRESS_HOME_COUNTRY, EMPTY_TYPE});
  CHECK_NE(type, EMPTY_TYPE);
  const auto& order =
      use_improved_labels_order ? kImprovedOrder : kDefaultOrder;
  if (auto it = std::ranges::find(order, type); it != order.end()) {
    return it - order.begin();
  }
  // The priority of other types is arbitrary, but deterministic.
  return 100 + type;
}

// Fills `distinguishing_fields` with a list of fields to use when creating
// labels that can help to distinguish between two profiles. Draws fields from
// `suggested_fields` if it is non-NULL; otherwise returns a default list.
// If `suggested_fields` is non-NULL, does not include `excluded_fields` in the
// list. Otherwise, `excluded_fields` is ignored, and should be set to
// an empty list by convention. The resulting list of fields is sorted in
// decreasing order of importance.
void GetFieldsForDistinguishingProfiles(
    const std::vector<FieldType>* suggested_fields,
    FieldTypeSet excluded_fields,
    std::vector<FieldType>* distinguishing_fields,
    bool use_improved_labels_order) {
  std::vector<FieldType> default_fields;
  if (!suggested_fields) {
    default_fields.assign(
        AutofillProfile::kDefaultDistinguishingFieldsForLabels.begin(),
        AutofillProfile::kDefaultDistinguishingFieldsForLabels.end());
    if (excluded_fields.empty()) {
      distinguishing_fields->swap(default_fields);
      return;
    }
    suggested_fields = &default_fields;
  }

  // Keep track of which fields we've seen so that we avoid duplicate entries.
  // Always ignore fields of unknown type and those part of `excluded_fields`.
  FieldTypeSet seen_fields;
  seen_fields.insert(UNKNOWN_TYPE);
  for (FieldType excluded_field : excluded_fields) {
    seen_fields.insert(GetStorableTypeCollapsingGroups(
        excluded_field, use_improved_labels_order));
  }

  distinguishing_fields->clear();
  for (const FieldType& it : *suggested_fields) {
    FieldType suggested_type =
        GetStorableTypeCollapsingGroups(it, use_improved_labels_order);
    if (seen_fields.insert(suggested_type).second)
      distinguishing_fields->push_back(suggested_type);
  }
  std::sort(distinguishing_fields->begin(), distinguishing_fields->end(),
            [use_improved_labels_order](FieldType type1, FieldType type2) {
              return SpecificityForType(type1, use_improved_labels_order) <
                     SpecificityForType(type2, use_improved_labels_order);
            });

  // Special case: If one of the excluded fields is a partial name (e.g.
  // `NAME_FIRST`) or phone number (e.g `PHONE_HOME_CITY_CODE`) and the
  // suggested fields include other name or phone fields fields, include
  // `NAME_FULL` or `PHONE_HOME_WHOLE_NUMBER` in the list of distinguishing
  // fields as a last-ditch fallback. This allows us to distinguish between
  // profiles that are identical except for the name or phone number.
  // TODO(crbug.com/380273791): Clean up this special case. It might be possible
  // to just append `PHONE_HOME_WHOLE_NUMBER` at the end.
  for (FieldType excluded_field : excluded_fields) {
    FieldType effective_excluded_type =
        GetStorableTypeCollapsingGroupsForPartialType(excluded_field);
    if (excluded_field == effective_excluded_type) {
      continue;
    }
    for (const FieldType& it : *suggested_fields) {
      if (it != excluded_field && GetStorableTypeCollapsingGroupsForPartialType(
                                      it) == effective_excluded_type) {
        distinguishing_fields->push_back(effective_excluded_type);
        break;
      }
    }
  }
}

#if BUILDFLAG(IS_ANDROID)
// Constructs an AutofillProfile using the provided `existing_profile` as a
// foundation. In case that the `existing_profile` is invalid, an empty profile
// with a unique identifier (GUID) corresponding to the Java profile
// (`jprofile`) is initialized.
AutofillProfile CreateStarterProfile(
    const base::android::JavaParamRef<jobject>& jprofile,
    JNIEnv* env,
    const AutofillProfile* existing_profile) {
  std::string guid = Java_AutofillProfile_getGUID(env, jprofile);
  if (!existing_profile) {
    AutofillProfile::RecordType record_type =
        Java_AutofillProfile_getRecordType(env, jprofile);
    AddressCountryCode country_code =
        AddressCountryCode(Java_AutofillProfile_getCountryCode(env, jprofile));
    AutofillProfile profile = AutofillProfile(record_type, country_code);
    // Only set the guid if CreateStartProfile is called on an existing profile
    // (java guid not empty). Otherwise, keep the generated one.
    // TODO(crbug.com/40282123): `guid` should be always empty when existing
    // profile is not set. CHECK should be added when this condition holds.
    if (!guid.empty()) {
      profile.set_guid(guid);
    }
    return profile;
  }

  CHECK_EQ(existing_profile->guid(), guid);
  return *existing_profile;
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

AutofillProfile::AutofillProfile(const std::string& guid,
                                 RecordType record_type,
                                 AddressCountryCode country_code)
    : guid_(guid),
      phone_number_(this),
      address_(country_code),
      record_type_(record_type),
      initial_creator_id_(kInitialCreatorOrModifierChrome),
      last_modifier_id_(kInitialCreatorOrModifierChrome),
      token_quality_(this),
      usage_history_information_(/*usage_history_size=*/3) {}

AutofillProfile::AutofillProfile(RecordType record_type,
                                 AddressCountryCode country_code)
    : AutofillProfile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                      record_type,
                      country_code) {}

AutofillProfile::AutofillProfile(AddressCountryCode country_code)
    : AutofillProfile(RecordType::kLocalOrSyncable, country_code) {}

AutofillProfile::AutofillProfile(const AutofillProfile& profile)
    : phone_number_(this),
      address_(profile.GetAddress()),
      token_quality_(this),
      usage_history_information_(profile.usage_history_information_) {
  operator=(profile);
}

AutofillProfile::~AutofillProfile() = default;

AutofillProfile& AutofillProfile::operator=(const AutofillProfile& profile) {
  if (this == &profile)
    return *this;

  usage_history_information_.set_use_count(
      profile.usage_history_information_.use_count());
  for (size_t i = 1; i <= usage_history_information_.usage_history_size();
       i++) {
    usage_history_information_.set_use_date(
        profile.usage_history_information_.use_date(i), i);
  }
  usage_history_information_.set_modification_date(
      profile.usage_history_information_.modification_date());

  set_guid(profile.guid());

  set_profile_label(profile.profile_label());

  name_ = profile.name_;
  email_ = profile.email_;
  company_ = profile.company_;
  phone_number_ = profile.phone_number_;
  phone_number_.set_profile(this);

  address_ = profile.address_;
  set_language_code(profile.language_code());

  record_type_ = profile.record_type_;
  initial_creator_id_ = profile.initial_creator_id_;
  last_modifier_id_ = profile.last_modifier_id_;

  token_quality_ = profile.token_quality_;
  token_quality_.set_profile(this);

  return *this;
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> AutofillProfile::CreateJavaObject(
    const std::string& app_locale) const {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jprofile =
      Java_AutofillProfile_Constructor(
          env, guid(), static_cast<jint>(record_type()), language_code());

  for (FieldType type : AutofillProfile::kDatabaseStoredTypes) {
    auto status = static_cast<jint>(GetVerificationStatus(type));
    // TODO(crbug.com/40278253): Reconcile usage of GetInfo and GetRawInfo
    // below.
    if (type == NAME_FULL) {
      Java_AutofillProfile_setInfo(env, jprofile, static_cast<jint>(type),
                                   GetInfo(type, app_locale), status);
    } else {
      Java_AutofillProfile_setInfo(env, jprofile, static_cast<jint>(type),
                                   GetRawInfo(type), status);
    }
  }
  return jprofile;
}

// static
AutofillProfile AutofillProfile::CreateFromJavaObject(
    const base::android::JavaParamRef<jobject>& jprofile,
    const AutofillProfile* existing_profile,
    const std::string& app_locale) {
  JNIEnv* env = base::android::AttachCurrentThread();
  AutofillProfile profile =
      CreateStarterProfile(jprofile, env, existing_profile);

  std::vector<int> field_types =
      Java_AutofillProfile_getFieldTypes(env, jprofile);

  std::vector<std::tuple<FieldType, std::u16string, VerificationStatus>>
      modified_fields;
  for (int int_field_type : field_types) {
    FieldType field_type = ToSafeFieldType(int_field_type, NO_SERVER_DATA);
    CHECK(field_type != NO_SERVER_DATA);
    VerificationStatus status =
        Java_AutofillProfile_getInfoStatus(env, jprofile, field_type);
    std::u16string value =
        Java_AutofillProfile_getInfo(env, jprofile, field_type);

    if (base::FeatureList::IsEnabled(
            features::kAutofillFixEmptyFieldAndroidSettingsBug)) {
      if (value != profile.GetInfo(field_type, app_locale) ||
          status != profile.GetVerificationStatus(field_type)) {
        modified_fields.emplace_back(field_type, value, status);
      }
    } else {
      if (!value.empty()) {
        modified_fields.emplace_back(field_type, value, status);
      }
    }
  }

  for (const auto& field_data : modified_fields) {
    const auto& [field_type, value, status] = field_data;
    // TODO(crbug.com/40278253): Reconcile usage of GetInfo and GetRawInfo
    // below.
    if (field_type == NAME_FULL || field_type == ADDRESS_HOME_COUNTRY) {
      profile.SetInfoWithVerificationStatus(field_type, value, app_locale,
                                            status);
    } else {
      profile.SetRawInfoWithVerificationStatus(field_type, value, status);
    }
  }

  profile.set_language_code(
      Java_AutofillProfile_getLanguageCode(env, jprofile));
  profile.FinalizeAfterImport();

  return profile;
}
#endif  // BUILDFLAG(IS_ANDROID)

double AutofillProfile::GetRankingScore(base::Time current_time,
                                        bool use_frecency) const {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableRankingFormulaAddressProfiles) &&
      !use_frecency) {
    // Exponentially decay the use count by the days since the data model was
    // last used.
    return log10(usage_history_information_.use_count() + 1) *
           exp(-usage_history_information_.GetDaysSinceLastUse(current_time) /
               features::kAutofillRankingFormulaAddressProfilesUsageHalfLife
                   .Get());
  }
  // Default to legacy frecency scoring.
  return usage_history_information_.GetRankingScore(current_time);
}

bool AutofillProfile::HasGreaterRankingThan(const AutofillProfile* other,
                                            base::Time comparison_time,
                                            bool use_frecency) const {
  const double score = GetRankingScore(comparison_time, use_frecency);
  const double other_score =
      other->GetRankingScore(comparison_time, use_frecency);
  return usage_history_information_.CompareRankingScores(
      score, other_score, other->usage_history().use_date());
}

void AutofillProfile::GetMatchingTypes(const std::u16string& text,
                                       const std::string& app_locale,
                                       FieldTypeSet* matching_types) const {
  FieldTypeSet matching_types_in_this_profile;
  for (const auto* form_group : FormGroups()) {
    form_group->GetMatchingTypes(text, app_locale,
                                 &matching_types_in_this_profile);
  }

  for (auto type : matching_types_in_this_profile) {
    matching_types->insert(type);
  }
}

std::u16string AutofillProfile::GetRawInfo(FieldType type) const {
  const FormGroup* form_group = FormGroupForType(type);
  if (!form_group)
    return std::u16string();
  return form_group->GetRawInfo(type);
}

void AutofillProfile::SetRawInfoWithVerificationStatus(
    FieldType type,
    const std::u16string& value,
    VerificationStatus status) {
  FormGroup* form_group = MutableFormGroupForType(type);
  if (form_group) {
    form_group->SetRawInfoWithVerificationStatus(type, value, status);
  }
}

FieldTypeSet AutofillProfile::GetSupportedTypes() const {
  FieldTypeSet supported_types;
  for (const FormGroup* form_group : FormGroups()) {
    supported_types.insert_all(form_group->GetSupportedTypes());
  }
  return supported_types;
}

FieldType AutofillProfile::GetStorableTypeOf(FieldType type) const {
  const FieldTypeGroup group = GroupTypeOfFieldType(type);
  if (group == FieldTypeGroup::kAddress) {
    return address_.GetRoot().GetStorableTypeOf(type).value_or(type);
  } else if (group == FieldTypeGroup::kName) {
    return name_.GetStructuredName().GetStorableTypeOf(type).value_or(type);
  } else if (group == FieldTypeGroup::kPhone) {
    // The only storable phone number type is PHONE_HOME_WHOLE_NUMBER.
    return PHONE_HOME_WHOLE_NUMBER;
  } else {
    // The other FieldTypeGroups only support storable types.
    return type;
  }
}

FieldTypeSet AutofillProfile::GetUserVisibleTypes() const {
  FieldTypeSet visible_types{PHONE_HOME_WHOLE_NUMBER, EMAIL_ADDRESS};
  std::string components_language_code;
  for (const AutofillAddressUIComponent& component :
       GetAddressComponents(GetAddressCountryCode().value(),
                            kAddressComponentsDefaultLocality,
                            /*enable_field_labels_localization=*/false,
                            /*include_literals=*/false,
                            components_language_code)) {
    visible_types.insert(component.field);
  }

  return visible_types;
}

bool AutofillProfile::IsEmpty(const std::string& app_locale) const {
  FieldTypeSet types;
  GetNonEmptyTypes(app_locale, &types);
  return types.empty();
}

bool AutofillProfile::IsPresentButInvalid(FieldType type) const {
  std::string country = base::UTF16ToUTF8(GetRawInfo(ADDRESS_HOME_COUNTRY));
  std::u16string data = GetRawInfo(type);
  if (data.empty())
    return false;

  switch (type) {
    case ADDRESS_HOME_STATE:
      return country == "US" && !IsValidState(data);

    case ADDRESS_HOME_ZIP:
      return country == "US" && !IsValidZip(data);

    case PHONE_HOME_WHOLE_NUMBER:
      return !i18n::PhoneObject(data, country, /*infer_country_code=*/false)
                  .IsValidNumber();

    case EMAIL_ADDRESS:
      return !IsValidEmailAddress(data);

    default:
      NOTREACHED();
  }
}

int AutofillProfile::Compare(const AutofillProfile& profile) const {
  static constexpr auto kTypes =
      std::to_array<FieldType>({NAME_FULL,
                                NAME_FIRST,
                                NAME_MIDDLE,
                                NAME_LAST,
                                NAME_LAST_PREFIX,
                                NAME_LAST_CORE,
                                NAME_LAST_FIRST,
                                NAME_LAST_SECOND,
                                NAME_LAST_CONJUNCTION,
                                ALTERNATIVE_FULL_NAME,
                                ALTERNATIVE_GIVEN_NAME,
                                ALTERNATIVE_FAMILY_NAME,
                                COMPANY_NAME,
                                ADDRESS_HOME_STREET_ADDRESS,
                                ADDRESS_HOME_DEPENDENT_LOCALITY,
                                ADDRESS_HOME_CITY,
                                ADDRESS_HOME_STATE,
                                ADDRESS_HOME_ZIP,
                                ADDRESS_HOME_SORTING_CODE,
                                ADDRESS_HOME_COUNTRY,
                                ADDRESS_HOME_LANDMARK,
                                ADDRESS_HOME_OVERFLOW,
                                ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY,
                                ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
                                ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
                                ADDRESS_HOME_BETWEEN_STREETS,
                                ADDRESS_HOME_BETWEEN_STREETS_1,
                                ADDRESS_HOME_BETWEEN_STREETS_2,
                                ADDRESS_HOME_ADMIN_LEVEL2,
                                ADDRESS_HOME_HOUSE_NUMBER,
                                ADDRESS_HOME_STREET_NAME,
                                ADDRESS_HOME_SUBPREMISE,
                                ADDRESS_HOME_STREET_LOCATION,
                                ADDRESS_HOME_APT,
                                ADDRESS_HOME_APT_NUM,
                                ADDRESS_HOME_APT_TYPE,
                                ADDRESS_HOME_FLOOR,
                                EMAIL_ADDRESS,
                                PHONE_HOME_WHOLE_NUMBER});

  // When adding field types, ensure that they don't need to be added here and
  // update the last checked value.
  static_assert(FieldType::MAX_VALID_FIELD_TYPE == 190,
                "New field type needs to be reviewed for inclusion in the "
                "profile comparison logic.");

  for (FieldType type : kTypes) {
    int comparison = GetRawInfo(type).compare(profile.GetRawInfo(type));
    if (comparison != 0) {
      return comparison;
    }

    // If the value is empty, the verification status can be ambiguous because
    // the value could be either build from its empty child nodes or parsed
    // from its parent. Therefore, it should not be considered when evaluating
    // the similarity of two profiles.
    if (profile.GetRawInfo(type).empty())
      continue;

    if (IsLessSignificantVerificationStatus(
            GetVerificationStatus(type), profile.GetVerificationStatus(type))) {
      return -1;
    }
    if (IsLessSignificantVerificationStatus(profile.GetVerificationStatus(type),
                                            GetVerificationStatus(type))) {
      return 1;
    }
  }

  return 0;
}

bool AutofillProfile::EqualsForLegacySyncPurposes(
    const AutofillProfile& profile) const {
  return usage_history_information_.use_count() ==
             profile.usage_history_information_.use_count() &&
         usage_history_information_.UseDateEqualsInSeconds(
             profile.usage_history()) &&
         EqualsSansGuid(profile);
}

bool AutofillProfile::EqualsForUpdatePurposes(
    const AutofillProfile& new_profile) const {
  return usage_history_information_.use_count() ==
             new_profile.usage_history_information_.use_count() &&
         usage_history_information_.UseDateEqualsInSeconds(
             new_profile.usage_history()) &&
         language_code() == new_profile.language_code() &&
         token_quality() == new_profile.token_quality() &&
         Compare(new_profile) == 0;
}

bool AutofillProfile::operator==(const AutofillProfile& profile) const {
  return guid() == profile.guid() && EqualsSansGuid(profile);
}

bool AutofillProfile::IsSubsetOf(const AutofillProfileComparator& comparator,
                                 const AutofillProfile& profile) const {
  return IsSubsetOfForFieldSet(comparator, profile,
                               AutofillProfile::kDatabaseStoredTypes);
}

bool AutofillProfile::IsSubsetOfForFieldSet(
    const AutofillProfileComparator& comparator,
    const AutofillProfile& profile,
    const FieldTypeSet& types) const {
  const std::string& app_locale = comparator.app_locale();
  const AddressComponent& address = GetAddress().GetRoot();
  const AddressComponent& other_address = profile.GetAddress().GetRoot();

  for (FieldType type : types) {
    // Prefer GetInfo over GetRawInfo so that a reasonable value is retrieved
    // when the raw data is empty or unnormalized. For example, suppose a
    // profile's first and last names are set but its full name is not set.
    // GetInfo for the NAME_FULL type returns the constituent name parts;
    // however, GetRawInfo returns an empty string.
    const std::u16string value = GetInfo(type, app_locale);
    if (value.empty()) {
      continue;
    }
    // TODO(crbug.com/40257475): Use rewriter rules for all kAddressHome types.
    if (type == ADDRESS_HOME_STREET_ADDRESS || type == ADDRESS_HOME_LINE1 ||
        type == ADDRESS_HOME_LINE2 || type == ADDRESS_HOME_LINE3) {
      // This will compare street addresses after applying appropriate address
      // rewriter rules to both values, so that for example US streets like
      // `Main Street` and `main st` evaluate to equal.
      if (address.GetValueForComparisonForType(type, other_address) !=
          other_address.GetValueForComparisonForType(type, address)) {
        return false;
      }
    } else if (type == NAME_FULL) {
      if (!comparator.IsNameVariantOf(
              AutofillProfileComparator::NormalizeForComparison(
                  profile.GetInfo(NAME_FULL, app_locale)),
              AutofillProfileComparator::NormalizeForComparison(value))) {
        // Check whether the full name of |this| can be derived from the full
        // name of |profile| if the form contains a full name field.
        //
        // Suppose the full name of |this| is Mia Park and |profile|'s full name
        // is Mia L Park. Mia Park can be derived from Mia L Park, so |this|
        // could be a subset of |profile|.
        //
        // If the form contains fields for a name's constituent parts, e.g.
        // NAME_FIRST, then these values are compared according to the
        // conditions that follow.
        return false;
      }
    } else if (type == PHONE_HOME_WHOLE_NUMBER ||
               type == PHONE_HOME_CITY_AND_NUMBER) {
      if (!i18n::PhoneNumbersMatch(
              value, profile.GetInfo(type, app_locale),
              base::UTF16ToASCII(GetRawInfo(ADDRESS_HOME_COUNTRY)),
              app_locale)) {
        return false;
      }
    } else if (!comparator.Compare(value, profile.GetInfo(type, app_locale))) {
      return false;
    }
  }
  return true;
}

bool AutofillProfile::IsStrictSupersetOf(
    const AutofillProfileComparator& comparator,
    const AutofillProfile& profile) const {
  return profile.IsSubsetOf(comparator, *this) &&
         !IsSubsetOf(comparator, profile);
}

AddressCountryCode AutofillProfile::GetAddressCountryCode() const {
  return GetAddress().GetAddressCountryCode();
}

bool AutofillProfile::IsAccountProfile() const {
  switch (record_type()) {
    case RecordType::kLocalOrSyncable:
      return false;
    case RecordType::kAccount:
    case RecordType::kAccountHome:
    case RecordType::kAccountWork:
      return true;
  }
  NOTREACHED();
}

void AutofillProfile::OverwriteDataFromForLegacySync(
    const AutofillProfile& profile) {
  DCHECK_EQ(guid(), profile.guid());

  // Some fields should not got overwritten by empty values; back-up the
  // values.
  std::string language_code_value = language_code();

  // Structured names should not be simply overwritten but it should be
  // attempted to merge the names.
  bool is_structured_name_mergeable = false;
  NameInfo name_info = GetNameInfo();
  is_structured_name_mergeable =
      name_info.IsStructuredNameMergeable(profile.GetNameInfo());
  name_info.MergeStructuredName(profile.GetNameInfo());

  // ProfileTokenQuality is not synced through legacy sync - and as a result,
  // `profile` has no observations. Make sure that observations for token values
  // that haven't changed are kept.
  ProfileTokenQuality token_quality = std::move(token_quality_);
  token_quality.ResetObservationsForDifferingTokens(profile);

  *this = profile;

  if (language_code().empty())
    set_language_code(language_code_value);

  // For structured names, use the merged name if possible.
  // If the full name of |profile| is empty, maintain the complete name
  // structure. Note, this should only happen if the complete name is empty. For
  // the legacy implementation, set the full name if |profile| does not contain
  // a full name.
  if (is_structured_name_mergeable || !HasRawInfo(NAME_FULL)) {
    name_ = std::move(name_info);
  }

  token_quality_ = std::move(token_quality);
}

bool AutofillProfile::MergeDataFrom(const AutofillProfile& profile,
                                    const std::string& app_locale) {
  AutofillProfileComparator comparator(app_locale);
  DCHECK(comparator.AreMergeable(*this, profile));

  NameInfo name;
  EmailInfo email;
  CompanyInfo company;
  PhoneNumber phone_number(this);
  Address address(profile.GetAddressCountryCode());

  DVLOG(1) << "Merging profiles:\nSource = " << profile << "\nDest = " << *this;

  // The comparator's merge operations are biased to prefer the data in the
  // first profile parameter when the data is the same modulo case. We expect
  // the caller to pass the incoming profile in this position to prefer
  // accepting updates instead of preserving the original data. I.e., passing
  // the incoming profile first accepts case and diacritic changes, for example,
  // the other ways does not.
  if (!comparator.MergeNames(profile, *this, name) ||
      !comparator.MergeEmailAddresses(profile, *this, email) ||
      !comparator.MergeCompanyNames(profile, *this, company) ||
      !comparator.MergePhoneNumbers(profile, *this, phone_number) ||
      !comparator.MergeAddresses(profile, *this, address)) {
    DUMP_WILL_BE_NOTREACHED();
    return false;
  }

  set_language_code(profile.language_code());

  // Update the use-count to be the max of the two merge-counts. Alternatively,
  // we could have summed the two merge-counts. We don't sum because it skews
  // the ranking score value on merge and double counts usage on profile reuse.
  // Profile reuse is accounted for on RecordUseOf() on selection of a profile
  // in the autofill drop-down; we don't need to account for that here. Further,
  // a similar, fully-typed submission that merges to an existing profile should
  // not be counted as a re-use of that profile.
  usage_history_information_.set_use_count(
      std::max(profile.usage_history_information_.use_count(),
               usage_history_information_.use_count()));
  usage_history_information_.MergeUseDates(profile.usage_history());

  // Update the fields which need to be modified, if any. Note: that we're
  // comparing the fields for representational equality below (i.e., are the
  // values byte for byte the same).

  bool modified = false;

  if (name_ != name) {
    MergeFormGroupTokenQuality(name, profile);
    name_ = name;
    modified = true;
  }

  if (email_ != email) {
    MergeFormGroupTokenQuality(email, profile);
    email_ = email;
    modified = true;
  }

  if (company_ != company) {
    MergeFormGroupTokenQuality(company, profile);
    company_ = company;
    modified = true;
  }

  if (phone_number_ != phone_number) {
    MergeFormGroupTokenQuality(phone_number, profile);
    phone_number_ = phone_number;
    modified = true;
  }

  if (address_ != address) {
    MergeFormGroupTokenQuality(address, profile);
    address_ = address;
    modified = true;
  }

  return modified;
}

void AutofillProfile::MergeFormGroupTokenQuality(
    const FormGroup& merged_group,
    const AutofillProfile& other_profile) {
  for (FieldType type : merged_group.GetSupportedTypes()) {
    const std::u16string& merged_value = merged_group.GetRawInfo(type);
    if (!AutofillProfile::kDatabaseStoredTypes.contains(type) ||
        merged_value == GetRawInfo(type)) {
      // Quality information is only tracked for stored types. If the merged
      // value matches the existing value, its token quality is kept.
      continue;
    }
    if (merged_value == other_profile.GetRawInfo(type)) {
      // The merged value comes from the `other_profile`, so its token quality
      // is carried over.
      token_quality_.CopyObservationsForStoredType(
          type, other_profile.token_quality_);
    } else {
      // The `merged_value` matches neither `*this` nor the `other_profile`'s
      // value, because the values were combined in some way. This generally
      // doesn't happen, because the merging logic only merges values if one is
      // a subset/substring of the other. However, in some cases, formatting
      // differences can make this case reachable. For example, merging the
      // phone numbers "5550199" and "555.0199" gives "555-0199".
      // Since observations cannot be merged, reset the token quality.
      token_quality_.ResetObservationsForStoredType(type);
    }
  }
}

// static
std::vector<std::u16string> AutofillProfile::CreateDifferentiatingLabels(
    const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
        profiles,
    const std::string& app_locale) {
  const size_t kMinimalFieldsShown = 2;
  return CreateInferredLabels(profiles, /*suggested_fields=*/std::nullopt,
                              /*triggering_field_type=*/std::nullopt,
                              /*excluded_fields=*/{}, kMinimalFieldsShown,
                              app_locale);
}

// static
std::vector<std::u16string> AutofillProfile::CreateInferredLabels(
    const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
        profiles,
    const std::optional<FieldTypeSet> suggested_fields,
    std::optional<FieldType> triggering_field_type,
    FieldTypeSet excluded_fields,
    size_t minimal_fields_shown,
    const std::string& app_locale,
    bool use_improved_labels_order) {
  // TODO(crbug.com/380273791): Clean up after launch.
  CHECK(!triggering_field_type ||
        base::FeatureList::IsEnabled(features::kAutofillImprovedLabels));
  CHECK(!use_improved_labels_order ||
        base::FeatureList::IsEnabled(features::kAutofillImprovedLabels));

  std::vector<FieldType> fields_to_use;
  std::vector<FieldType> suggested_fields_types =
      suggested_fields
          ? std::vector(suggested_fields->begin(), suggested_fields->end())
          : std::vector<FieldType>();
  GetFieldsForDistinguishingProfiles(
      suggested_fields ? &suggested_fields_types : nullptr, excluded_fields,
      &fields_to_use, use_improved_labels_order);

  // Construct the default label for each profile. Also construct a map that
  // associates each (main_text, label) pair with the profiles that have this
  // info. This map is then used to detect which labels need further
  // differentiating fields.
  // Note that the actual displayed main text might slightly differ due to
  // formatting, but it is not needed to format the text for differentiating the
  // labels.
  std::map<std::pair<std::u16string, std::u16string>, std::list<size_t>>
      labels_to_profiles;
  for (size_t i = 0; i < profiles.size(); ++i) {
    std::u16string label = profiles[i]->ConstructInferredLabel(
        fields_to_use, minimal_fields_shown, app_locale);
    std::u16string main_text =
        triggering_field_type
            ? profiles[i]->GetInfo(*triggering_field_type, app_locale)
            : u"";
    labels_to_profiles[{main_text, label}].push_back(i);
  }

  std::vector<std::u16string> labels;
  labels.resize(profiles.size());
  for (auto& it : labels_to_profiles) {
    if (it.second.size() == 1) {
      // This label is unique, so use it without any further ado.
      std::u16string label = it.first.second;
      size_t profile_index = it.second.front();
      labels[profile_index] = label;
    } else {
      // We have more than one profile with the same label, so add
      // differentiating fields.
      CreateInferredLabelsHelper(
          profiles, it.second, fields_to_use, minimal_fields_shown, app_locale,
          use_improved_labels_order &&
              features::
                  kAutofillImprovedLabelsParamWithDifferentiatingLabelsInFrontParam
                      .Get(),
          labels);
    }
  }
  return labels;
}

std::u16string AutofillProfile::ConstructInferredLabel(
    base::span<const FieldType> included_fields,
    size_t num_fields_to_use,
    const std::string& app_locale) const {
  // TODO(estade): use libaddressinput?
  std::u16string separator =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);

  const std::u16string& profile_region_code =
      GetInfo(AutofillType(HtmlFieldType::kCountryCode), app_locale);
  std::string address_region_code = base::UTF16ToUTF8(profile_region_code);

  // A copy of |this| pruned down to contain only data for the address fields in
  // |included_fields|.
  AutofillProfile trimmed_profile(guid(), RecordType::kLocalOrSyncable,
                                  GetAddressCountryCode());
  trimmed_profile.SetInfo(AutofillType(HtmlFieldType::kCountryCode),
                          profile_region_code, app_locale);
  trimmed_profile.set_language_code(language_code());
  AutofillCountry country(address_region_code);

  std::vector<FieldType> remaining_fields;
  for (size_t i = 0; i < included_fields.size() && num_fields_to_use > 0; ++i) {
    if (!country.IsAddressFieldSettingAccessible(included_fields[i]) ||
        included_fields[i] == ADDRESS_HOME_COUNTRY) {
      remaining_fields.push_back(included_fields[i]);
      continue;
    }

    std::u16string field_value = GetInfo(included_fields[i], app_locale);
    if (field_value.empty()) {
      continue;
    }
    trimmed_profile.SetInfo(included_fields[i], field_value, app_locale);
    --num_fields_to_use;
  }

  std::unique_ptr<AddressData> address_data =
      i18n::CreateAddressDataFromAutofillProfile(trimmed_profile, app_locale);
  std::string address_line;
  ::i18n::addressinput::GetFormattedNationalAddressLine(*address_data,
                                                        &address_line);
  std::u16string label = base::UTF8ToUTF16(address_line);

  for (std::vector<FieldType>::const_iterator it = remaining_fields.begin();
       it != remaining_fields.end() && num_fields_to_use > 0; ++it) {
    std::u16string field_value;
    // Special case whole numbers: we want the user-formatted (raw) version, not
    // the canonicalized version we'll fill into the page.
    if (*it == PHONE_HOME_WHOLE_NUMBER)
      field_value = GetRawInfo(*it);
    else
      field_value = GetInfo(*it, app_locale);
    if (field_value.empty())
      continue;

    if (!label.empty()) {
      label.append(separator);
    }

    label.append(field_value);
    --num_fields_to_use;
  }

  // If country code is missing, libaddressinput won't be used to format the
  // address. In this case the suggestion might include a multi-line street
  // address which needs to be flattened.
  base::ReplaceChars(label, u"\n", separator, &label);

  return label;
}

void AutofillProfile::RecordAndLogUse() {
  const base::Time now = AutofillClock::Now();
  const base::TimeDelta time_since_last_used =
      now - usage_history_information_.use_date();
  usage_history_information_.RecordUseDate(now);
  // Ensure that use counts are not skewed by multiple filling operations of the
  // form. This is especially important for forms fully annotated with
  // autocomplete=unrecognized. For such forms, keyboard accessory chips only
  // fill a single field at a time as per
  // `AutofillSuggestionsForAutocompleteUnrecognizedFieldsOnMobile`.
  if (time_since_last_used.InSeconds() >= 60) {
    if (usage_history_information_.use_count() == 1) {
      // The max is the number of days a profile wasn't used before it gets
      // deleted (see `kDisusedDataModelDeletionTimeDelta`).
      base::UmaHistogramCustomCounts("Autofill.DaysUntilFirstUsage.Profile",
                                     time_since_last_used.InDays(), 1, 395,
                                     100);
    }
    usage_history_information_.set_use_count(
        usage_history_information_.use_count() + 1);
    UMA_HISTOGRAM_COUNTS_1000("Autofill.DaysSinceLastUse.Profile",
                              time_since_last_used.InDays());
  }
  LogVerificationStatuses();
}

void AutofillProfile::LogVerificationStatuses() {
  AutofillMetrics::LogVerificationStatusOfNameTokensOnProfileUsage(*this);
  AutofillMetrics::LogVerificationStatusOfAddressTokensOnProfileUsage(*this);
}

VerificationStatus AutofillProfile::GetVerificationStatus(
    const FieldType type) const {
  const FormGroup* form_group = FormGroupForType(type);
  if (!form_group)
    return VerificationStatus::kNoStatus;

  return form_group->GetVerificationStatus(type);
}

std::u16string AutofillProfile::GetInfo(const AutofillType& type,
                                        const std::string& app_locale) const {
  const FormGroup* form_group = FormGroupForType(type.GetStorableType());
  if (!form_group) {
    return std::u16string();
  }
  return form_group->GetInfo(type, app_locale);
}

bool AutofillProfile::SetInfoWithVerificationStatus(
    const AutofillType& type,
    const std::u16string& value,
    const std::string& app_locale,
    VerificationStatus status) {
  FormGroup* form_group = MutableFormGroupForType(type.GetStorableType());
  if (!form_group) {
    return false;
  }
  std::u16string trimmed_value;
  base::TrimWhitespace(value, base::TRIM_ALL, &trimmed_value);

  return form_group->SetInfoWithVerificationStatus(type, trimmed_value,
                                                   app_locale, status);
}

bool AutofillProfile::SetInfoWithVerificationStatus(
    FieldType type,
    const std::u16string& value,
    const std::string& app_locale,
    VerificationStatus status) {
  return SetInfoWithVerificationStatus(AutofillType(type), value, app_locale,
                                       status);
}

// static
void AutofillProfile::CreateInferredLabelsHelper(
    const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
        profiles,
    const std::list<size_t>& indices,
    const std::vector<FieldType>& field_types,
    size_t num_fields_to_include,
    const std::string& app_locale,
    bool force_differentiating_label_in_front,
    std::vector<std::u16string>& labels) {
  // For efficiency, we first construct a map of fields to their text values and
  // each value's frequency.
  std::map<FieldType, std::map<std::u16string, size_t>>
      field_text_frequencies_by_field;
  for (const FieldType& field_type : field_types) {
    std::map<std::u16string, size_t>& field_text_frequencies =
        field_text_frequencies_by_field[field_type];

    for (const auto& it : indices) {
      const AutofillProfile* profile = profiles[it];
      std::u16string field_text = profile->GetInfo(field_type, app_locale);

      // If this label is not already in the map, add it with frequency 0.
      if (!field_text_frequencies.contains(field_text)) {
        field_text_frequencies[field_text] = 0;
      }

      // Now, increment the frequency for this label.
      ++field_text_frequencies[field_text];
    }
  }

  // Now comes the meat of the algorithm. For each profile, we scan the list of
  // fields to use, looking for two things:
  //  1. A (non-empty) field that differentiates the profile from all others
  //  2. At least |num_fields_to_include| non-empty fields
  // Before we've satisfied condition (2), we include all fields, even ones that
  // are identical across all the profiles. Once we've satisfied condition (2),
  // we only include fields that that have at last two distinct values.
  for (const auto& it : indices) {
    const AutofillProfile* profile = profiles[it];

    std::vector<FieldType> label_fields;
    bool found_differentiating_field = false;
    std::u16string first_differentiating_field_text;
    for (FieldType field_type : field_types) {
      // Skip over empty fields.
      std::u16string field_text = profile->GetInfo(field_type, app_locale);
      if (field_text.empty())
        continue;

      std::map<std::u16string, size_t>& field_text_frequencies =
          field_text_frequencies_by_field[field_type];

      bool current_field_is_differentiating =
          !field_text_frequencies.contains(u"") &&
          field_text_frequencies[field_text] == 1;
      found_differentiating_field |= current_field_is_differentiating;

      // Once we've found enough non-empty fields, skip over any remaining
      // fields that are identical across all the profiles.
      if (label_fields.size() + !first_differentiating_field_text.empty() >=
              num_fields_to_include &&
          field_text_frequencies.size() == 1) {
        continue;
      }

      // Only the first differentiating label is moved to the front. This is
      // because `field_types` are ordered by relevance, so the first
      // differentiating label found is the most relevant. There is no need to
      // move more differentiating labels to the front, especially given that
      // the order established later by `ConstructInferredLabel` shouldn't be
      // broken more than necessary.
      if (force_differentiating_label_in_front &&
          current_field_is_differentiating &&
          first_differentiating_field_text.empty()) {
        first_differentiating_field_text = field_text;
      } else {
        label_fields.push_back(field_type);
      }

      // If we've (1) found a differentiating field and (2) found at least
      // |num_fields_to_include| non-empty fields, we're done!
      if (found_differentiating_field &&
          label_fields.size() >= num_fields_to_include)
        break;
    }

    // The final order of the `label_fields` is established by a third party
    // library: libaddressinput. Libaddressinput has a different order for each
    // country, depending on what makes sense in that country. Chrome code has
    // no control over the final order of the labels.
    labels[it] = profile->ConstructInferredLabel(
        label_fields, label_fields.size(), app_locale);
    // Manually append the differentiating label in front.
    if (!first_differentiating_field_text.empty()) {
      std::u16string separator =
          l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);
      labels[it] = labels[it].empty() ? first_differentiating_field_text
                                      : first_differentiating_field_text +
                                            separator + labels[it];
    }
  }
}

const FormGroup* AutofillProfile::FormGroupForType(FieldType type) const {
  return const_cast<AutofillProfile*>(this)->MutableFormGroupForType(type);
}

FormGroup* AutofillProfile::MutableFormGroupForType(FieldType type) {
  switch (GroupTypeOfFieldType(type)) {
    case FieldTypeGroup::kName:
      return &name_;
    case FieldTypeGroup::kEmail:
      return &email_;
    case FieldTypeGroup::kCompany:
      return &company_;
    case FieldTypeGroup::kPhone:
      return &phone_number_;
    case FieldTypeGroup::kAddress:
      return &address_;
    case FieldTypeGroup::kNoGroup:
    case FieldTypeGroup::kCreditCard:
    case FieldTypeGroup::kIban:
    case FieldTypeGroup::kPasswordField:
    case FieldTypeGroup::kUsernameField:
    case FieldTypeGroup::kTransaction:
    case FieldTypeGroup::kStandaloneCvcField:
    case FieldTypeGroup::kUnfillable:
    case FieldTypeGroup::kAutofillAi:
    case FieldTypeGroup::kLoyaltyCard:
      return nullptr;
  }
  NOTREACHED();
}

bool AutofillProfile::EqualsSansGuid(const AutofillProfile& profile) const {
  return language_code() == profile.language_code() &&
         profile_label() == profile.profile_label() &&
         record_type() == profile.record_type() && Compare(profile) == 0;
}

std::ostream& operator<<(std::ostream& os, const AutofillProfile& profile) {
  os << profile.guid() << " label: " << profile.profile_label() << " "
     << profile.usage_history().use_count() << " "
     << profile.usage_history().use_date() << " " << profile.language_code()
     << std::endl;

  for (FieldType type : profile.GetSupportedTypes()) {
    os << FieldTypeToStringView(type) << ": " << profile.GetRawInfo(type) << "("
       << profile.GetVerificationStatus(type) << ")" << std::endl;
  }

  return os;
}

bool AutofillProfile::FinalizeAfterImport() {
  bool success = true;
  success &= name_.FinalizeAfterImport();
  success &= address_.FinalizeAfterImport();
  return success;
}

AutofillProfile AutofillProfile::ConvertToAccountProfile() const {
  DCHECK_EQ(record_type(), RecordType::kLocalOrSyncable);
  AutofillProfile account_profile = *this;
  // Since GUIDs are assumed to be unique across all profile record types, a new
  // GUID is assigned.
  account_profile.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  account_profile.record_type_ = RecordType::kAccount;
  // Initial creator and last modifier are unused for kLocalOrSyncable profiles.
  account_profile.initial_creator_id_ = kInitialCreatorOrModifierChrome;
  account_profile.last_modifier_id_ = kInitialCreatorOrModifierChrome;
  return account_profile;
}

FieldTypeSet AutofillProfile::FindInaccessibleProfileValues() const {
  FieldTypeSet inaccessible_fields;
  const std::string stored_country =
      base::UTF16ToUTF8(GetRawInfo(ADDRESS_HOME_COUNTRY));
  // Consider only AddressFields which are invisible in the settings for some
  // countries.
  for (const AddressField& adress_field :
       {AddressField::ADMIN_AREA, AddressField::LOCALITY,
        AddressField::DEPENDENT_LOCALITY, AddressField::POSTAL_CODE,
        AddressField::SORTING_CODE}) {
    FieldType field_type = i18n::TypeForField(adress_field);
    CHECK_EQ(GroupTypeOfFieldType(field_type), FieldTypeGroup::kAddress);
    if (HasRawInfo(field_type) &&
        !GetAddress().IsAddressFieldSettingAccessible(field_type)) {
      inaccessible_fields.insert(field_type);
    }
  }
  return inaccessible_fields;
}

void AutofillProfile::ClearFields(const FieldTypeSet& fields) {
  for (FieldType field_type : fields) {
    SetRawInfoWithVerificationStatus(field_type, u"",
                                     VerificationStatus::kNoStatus);
  }
}

UsageHistoryInformation& AutofillProfile::usage_history() {
  return usage_history_information_;
}
const UsageHistoryInformation& AutofillProfile::usage_history() const {
  return usage_history_information_;
}

}  // namespace autofill
