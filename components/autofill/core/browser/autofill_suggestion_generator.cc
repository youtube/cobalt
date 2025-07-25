// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_suggestion_generator.h"

#include <string>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/autofill_optimization_guide.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/field_filler.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/address_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/label_formatter.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace autofill {

namespace {

// Returns the credit card field |value| trimmed from whitespace and with stop
// characters removed.
std::u16string SanitizeCreditCardFieldValue(const std::u16string& value) {
  std::u16string sanitized;
  // We remove whitespace as well as some invisible unicode characters.
  base::TrimWhitespace(value, base::TRIM_ALL, &sanitized);
  base::TrimString(sanitized,
                   std::u16string({base::i18n::kRightToLeftMark,
                                   base::i18n::kLeftToRightMark}),
                   &sanitized);
  // Some sites have ____-____-____-____ in their credit card number fields, for
  // example.
  base::RemoveChars(sanitized, u"-_", &sanitized);
  return sanitized;
}

// Returns the card-linked offers map with credit card guid as the key and the
// pointer to the linked AutofillOfferData as the value.
std::map<std::string, AutofillOfferData*> GetCardLinkedOffers(
    AutofillClient* autofill_client) {
  AutofillOfferManager* offer_manager =
      autofill_client->GetAutofillOfferManager();
  if (offer_manager) {
    return offer_manager->GetCardLinkedOffersMap(
        autofill_client->GetLastCommittedPrimaryMainFrameURL());
  }
  return {};
}

int GetObfuscationLength() {
#if BUILDFLAG(IS_ANDROID)
  // On Android, the obfuscation length is 2.
  return 2;
#elif BUILDFLAG(IS_IOS)
  return base::FeatureList::IsEnabled(
             features::kAutofillUseTwoDotsForLastFourDigits)
             ? 2
             : 4;
#else
  return 4;
#endif
}

bool ShouldSplitCardNameAndLastFourDigits() {
#if BUILDFLAG(IS_IOS)
  return false;
#else
  return base::FeatureList::IsEnabled(
             features::kAutofillEnableVirtualCardMetadata) &&
         base::FeatureList::IsEnabled(features::kAutofillEnableCardProductName);
#endif
}

Suggestion GetEditAddressProfileSuggestion(Suggestion::BackendId backend_id) {
  Suggestion suggestion(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_EDIT_ADDRESS_PROFILE_POPUP_OPTION_SELECTED));
  suggestion.popup_item_id = PopupItemId::kEditAddressProfile;
  suggestion.icon = "editIcon";
  suggestion.payload = backend_id;
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_EDIT_ADDRESS_PROFILE_POPUP_OPTION_SELECTED);
  return suggestion;
}

// Creates the suggestion that will open the delete address profile dialog.
Suggestion GetDeleteAddressProfileSuggestion(Suggestion::BackendId backend_id) {
  Suggestion suggestion(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_DELETE_ADDRESS_PROFILE_POPUP_OPTION_SELECTED));
  suggestion.popup_item_id = PopupItemId::kDeleteAddressProfile;
  suggestion.icon = "deleteIcon";
  suggestion.payload = backend_id;
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_DELETE_ADDRESS_PROFILE_POPUP_OPTION_SELECTED);
  return suggestion;
}

// Creates the suggestion that will fill all address related fields.
Suggestion GetFillFullAddressSuggestion(Suggestion::BackendId backend_id) {
  Suggestion suggestion(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FILL_ADDRESS_GROUP_POPUP_OPTION_SELECTED));
  suggestion.popup_item_id = PopupItemId::kFillFullAddress;
  suggestion.payload = backend_id;
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_FILL_ADDRESS_GROUP_POPUP_OPTION_SELECTED);
  return suggestion;
}

// Creates the suggestion that will fill all name related fields.
Suggestion GetFillFullNameSuggestion(Suggestion::BackendId backend_id) {
  Suggestion suggestion(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FILL_NAME_GROUP_POPUP_OPTION_SELECTED));
  suggestion.popup_item_id = PopupItemId::kFillFullName;
  suggestion.payload = backend_id;
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_FILL_NAME_GROUP_POPUP_OPTION_SELECTED);

  return suggestion;
}

// Creates the suggestion that will fill the whole form for the profile. This
// suggestion is displayed once the users is on group filling level or field by
// field level. It is used as a way to allow users to go back to filling the
// whole form.
Suggestion GetFillEverythingFromAddressProfileSuggestion(
    Suggestion::BackendId backend_id) {
  Suggestion suggestion(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FILL_EVERYTHING_FROM_ADDRESS_PROFILE_POPUP_OPTION_SELECTED));
  suggestion.popup_item_id = PopupItemId::kFillEverythingFromAddressProfile;
  suggestion.icon = "magicIcon";
  suggestion.payload = backend_id;
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_FILL_EVERYTHING_FROM_ADDRESS_PROFILE_POPUP_OPTION_SELECTED);
  return suggestion;
}

// Append new suggestions to `suggestions` based on the `ServerFieldType` list
// provided. Suggestions are not added if their info is not found in the
// provided `profile`. Returns true if any suggestion was added.
bool AddFieldByFieldSuggestions(const std::vector<ServerFieldType>& types,
                                const AutofillProfile& profile,
                                const std::string& app_locale,
                                std::vector<Suggestion>& suggestions) {
  bool any_suggestion_added = false;
  for (auto type : types) {
    std::u16string value = profile.GetInfo(type, app_locale);
    if (!value.empty()) {
      suggestions.emplace_back(value, PopupItemId::kFieldByFieldFilling);
      any_suggestion_added = true;
    }
  }
  return any_suggestion_added;
}

// Given an address `type` and `sub_type`, returns whether the `sub_type` info
// stored in `profile` is a substring of the info stored in `profile` for
// `type`.
bool CheckIfTypeContainsSubtype(ServerFieldType type,
                                ServerFieldType sub_type,
                                const AutofillProfile& profile,
                                const std::string& app_locale) {
  if (!profile.HasInfo(type) || !profile.HasInfo(sub_type)) {
    return false;
  }

  std::u16string value = profile.GetInfo(type, app_locale);
  std::u16string sub_value = profile.GetInfo(sub_type, app_locale);
  return value != sub_value && value.find(sub_value) != std::u16string::npos;
}

// Adds name related child suggestions to build autofill popup submenu.
// The param `type` refers to the triggering field type (clicked by the users)
// and is used to define  whether the `PopupItemId::kFillFullName` suggestion
// will be available.
void AddNameChildSuggestions(FieldTypeGroup trigger_field_type_group,
                             const AutofillProfile& profile,
                             const std::string& app_locale,
                             Suggestion& suggestion) {
  if (trigger_field_type_group == FieldTypeGroup::kName) {
    // Note that this suggestion can only be added if name infos exist in the
    // profile.
    suggestion.children.push_back(
        GetFillFullNameSuggestion(Suggestion::BackendId(profile.guid())));
  }
  if (AddFieldByFieldSuggestions({NAME_FIRST, NAME_MIDDLE, NAME_LAST}, profile,
                                 app_locale, suggestion.children)) {
    suggestion.children.push_back(
        AutofillSuggestionGenerator::CreateSeparator());
  };
}

// Adds address line suggestions (ADDRESS_HOME_LINE1 and/or
// ADDRESS_HOME_LINE2) to `suggestions.children`. It potentially includes
// sub-children if one of the added suggestions contains
// ADDRESS_HOME_HOUSE_NUMBER and/or ADDRESS_HOME_STREET_NAME. Returns true if at
// least one suggestion was appended to `suggestions.children`.
bool AddAddressLineChildSuggestions(const AutofillProfile& profile,
                                    const std::string& app_locale,
                                    std::vector<Suggestion>& suggestions) {
  auto add_address_line = [&](ServerFieldType type) -> bool {
    CHECK(type == ADDRESS_HOME_LINE1 || type == ADDRESS_HOME_LINE2);

    if (!AddFieldByFieldSuggestions({type}, profile, app_locale, suggestions)) {
      return false;
    }

    if (CheckIfTypeContainsSubtype(type, ADDRESS_HOME_HOUSE_NUMBER, profile,
                                   app_locale) &&
        AddFieldByFieldSuggestions({ADDRESS_HOME_HOUSE_NUMBER}, profile,
                                   app_locale, suggestions.back().children)) {
      Suggestion& address_line_suggestion = suggestions.back().children.back();
      address_line_suggestion.labels = {
          {Suggestion::Text(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_HOUSE_NUMBER_SUGGESTION_SECONDARY_TEXT))}};
      address_line_suggestion
          .acceptance_a11y_announcement = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_HOUSE_NUMBER_SUGGESTION_SECONDARY_TEXT_OPTION_SELECTED);
    }
    if (CheckIfTypeContainsSubtype(type, ADDRESS_HOME_STREET_NAME, profile,
                                   app_locale) &&
        AddFieldByFieldSuggestions({ADDRESS_HOME_STREET_NAME}, profile,
                                   app_locale, suggestions.back().children)) {
      Suggestion& address_line_suggestion = suggestions.back().children.back();
      address_line_suggestion.labels = {
          {Suggestion::Text(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_STREET_NAME_SUGGESTION_SECONDARY_TEXT))}};
      address_line_suggestion
          .acceptance_a11y_announcement = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_STREET_NAME_SUGGESTION_SECONDARY_TEXT_OPTION_SELECTED);
    }

    return true;
  };
  bool added_address_line1 = add_address_line(ADDRESS_HOME_LINE1);
  bool added_address_line2 = add_address_line(ADDRESS_HOME_LINE2);
  return added_address_line1 || added_address_line2;
}

// Adds address related child suggestions to build autofill popup submenu.
// The param `type` refers to the triggering field type (clicked by the users)
// and is used to define  whether the `PopupItemId::kFillFullAddress` suggestion
// will be available.
void AddAddressChildSuggestions(FieldTypeGroup trigger_field_type_group,
                                const AutofillProfile& profile,
                                const std::string& app_locale,
                                Suggestion& suggestion) {
  if (trigger_field_type_group == FieldTypeGroup::kAddress) {
    // Note that this suggestion can only be added if address infos exist in the
    // profile.
    suggestion.children.push_back(
        GetFillFullAddressSuggestion(Suggestion::BackendId(profile.guid())));
  }

  bool added_any_address_line =
      AddAddressLineChildSuggestions(profile, app_locale, suggestion.children);
  bool added_zip = AddFieldByFieldSuggestions({ADDRESS_HOME_ZIP}, profile,
                                              app_locale, suggestion.children);
  if (added_any_address_line || added_zip) {
    suggestion.children.push_back(
        AutofillSuggestionGenerator::CreateSeparator());
  }
}

// Adds contact related child suggestions (i.e email and phone number) to
// build autofill popup submenu. The param `trigger_field_type_group` refers to
// the triggering field group (clicked by the users) and is used to define
// whether the phone number and email suggestions  will behave as
// `PopupItemId::kFieldByFieldFilling` or as
// `PopupItemId::kFillFullPhoneNumber`/`PopupItemId::kFillFullEmail`
// respectively. When the triggering field group matches the type of the field
// we are adding, the suggestion will be of group filling type, other field by
// field.
void AddContactChildSuggestions(FieldTypeGroup trigger_field_type_group,
                                const AutofillProfile& profile,
                                const std::string& app_locale,
                                Suggestion& suggestion) {
  bool phone_number_suggestion_added = false;
  if (profile.HasInfo(PHONE_HOME_WHOLE_NUMBER)) {
    Suggestion phone_number_suggestion(
        profile.GetInfo(PHONE_HOME_WHOLE_NUMBER, app_locale));
    const bool is_phone_field =
        trigger_field_type_group == FieldTypeGroup::kPhone;
    phone_number_suggestion.popup_item_id =
        is_phone_field ? PopupItemId::kFillFullPhoneNumber
                       : PopupItemId::kFieldByFieldFilling;
    phone_number_suggestion.payload = Suggestion::BackendId(profile.guid());
    suggestion.children.push_back(std::move(phone_number_suggestion));
    phone_number_suggestion_added = true;
  }

  bool email_address_suggestion_added = false;
  if (profile.HasInfo(EMAIL_ADDRESS)) {
    Suggestion email_address_suggestion(
        profile.GetInfo(EMAIL_ADDRESS, app_locale));
    const bool is_email_field =
        trigger_field_type_group == FieldTypeGroup::kEmail;
    email_address_suggestion.popup_item_id =
        is_email_field ? PopupItemId::kFillFullEmail
                       : PopupItemId::kFieldByFieldFilling;
    email_address_suggestion.payload = Suggestion::BackendId(profile.guid());
    suggestion.children.push_back(std::move(email_address_suggestion));
    email_address_suggestion_added = true;
  }

  if (email_address_suggestion_added || phone_number_suggestion_added) {
    suggestion.children.push_back(
        AutofillSuggestionGenerator::CreateSeparator());
  }
}

// Adds footer child suggestions to build autofill popup submenu.
void AddFooterChildSuggestions(
    const AutofillProfile& profile,
    absl::optional<ServerFieldTypeSet> last_targeted_fields,
    Suggestion& suggestion) {
  // If the last filling granularity was not full form, add the
  // `PopupItemId::kFillEverythingFromAddressProfile` suggestion. This allows
  // the user to go back to filling the whole form once in a more fine grained
  // filling experience.
  if (!last_targeted_fields || *last_targeted_fields != kAllServerFieldTypes) {
    suggestion.children.push_back(GetFillEverythingFromAddressProfileSuggestion(
        Suggestion::BackendId(profile.guid())));
  }
  suggestion.children.push_back(
      GetEditAddressProfileSuggestion(Suggestion::BackendId(profile.guid())));
  suggestion.children.push_back(
      GetDeleteAddressProfileSuggestion(Suggestion::BackendId(profile.guid())));
}

// Sets the `popup_item_id` for `suggestion` depending on
// `last_filling_granularity`.
// `last_targeted_fields` specified the last set of fields target by the user.
// When not present, we default to full form.
PopupItemId GetProfileSuggestionPopupItemId(
    absl::optional<ServerFieldTypeSet> optional_last_targeted_fields,
    FieldTypeGroup triggering_field_type_group) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillGranularFillingAvailable)) {
    return PopupItemId::kAddressEntry;
  }

  // Lambda to return the expected `PopupItemId` when
  // `optional_last_targeted_fields` matches one of the granular filling groups.
  auto get_popup_item_id_for_group_filling = [&] {
    switch (triggering_field_type_group) {
      case FieldTypeGroup::kName:
        return PopupItemId::kFillFullName;
      case FieldTypeGroup::kAddress:
      case FieldTypeGroup::kCompany:
        return PopupItemId::kFillFullAddress;
      case FieldTypeGroup::kPhone:
        return PopupItemId::kFillFullPhoneNumber;
      case FieldTypeGroup::kEmail:
        return PopupItemId::kFillFullEmail;
      default:
        // If the 'current_granularity' is group filling, BUT the current
        // focused field is not one for which group we offer group filling,
        // we default back to fill full form behaviour/pre-granular filling
        // popup id.
        return PopupItemId::kAddressEntry;
    }
  };

  switch (GetFillingMethodFromTargetedFields(
      optional_last_targeted_fields.value_or(kAllServerFieldTypes))) {
    case AutofillFillingMethod::kGroupFilling:
      return get_popup_item_id_for_group_filling();
    case AutofillFillingMethod::kFullForm:
      return PopupItemId::kAddressEntry;
    case AutofillFillingMethod::kFieldByFieldFilling:
      return PopupItemId::kFieldByFieldFilling;
    case AutofillFillingMethod::kNone:
      NOTREACHED_NORETURN();
  }
}

// Creates a specific granular filling label when the `last_filling_granularity`
// for a certain form was group filling. This is done to give users feedback
// about the filling behaviour.
std::optional<Suggestion::Text> GetGranularFillingLabel(
    absl::optional<ServerFieldTypeSet> optional_last_targeted_fields,
    FieldTypeGroup triggering_field_type_group) {
  if (!optional_last_targeted_fields ||
      !AreFieldsGranularFillingGroup(*optional_last_targeted_fields)) {
    return absl::nullopt;
  }
  switch (triggering_field_type_group) {
    case FieldTypeGroup::kName:
      return Suggestion::Text(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_FILL_NAME_GROUP_POPUP_OPTION_SELECTED));
    case FieldTypeGroup::kCompany:
    case FieldTypeGroup::kAddress:
      return Suggestion::Text(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_FILL_ADDRESS_GROUP_POPUP_OPTION_SELECTED));
    case FieldTypeGroup::kNoGroup:
    case FieldTypeGroup::kPhone:
    case FieldTypeGroup::kEmail:
    case FieldTypeGroup::kCreditCard:
    case FieldTypeGroup::kPasswordField:
    case FieldTypeGroup::kTransaction:
    case FieldTypeGroup::kUsernameField:
    case FieldTypeGroup::kUnfillable:
    case FieldTypeGroup::kBirthdateField:
    case FieldTypeGroup::kIban:
      return absl::nullopt;
  }
}

// Returns for each profile in `profiles` label `Suggestion::Text`s to be used
// as a secondary text in the corresponding suggestion bubble. The last label in
// the label's vector is used to differentiate profiles, while the others are a
// granular filling specific label and an optional separator label ("-"), which
// exists when both a granular filling and a differentiating label exists. The
// Granular filling label is added depending on `optional_last_targeted_fields`
// and the `trigger_field_type`. See `GetProfileSuggestionLabels()` for details.
std::vector<std::vector<Suggestion::Text>> GetProfileSuggestionLabels(
    const std::vector<const AutofillProfile*>& profiles,
    const ServerFieldTypeSet& field_types,
    ServerFieldType trigger_field_type,
    absl::optional<ServerFieldTypeSet> optional_last_targeted_fields,
    const std::string& app_locale) {
  std::unique_ptr<LabelFormatter> formatter;
  bool use_formatter;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  use_formatter = base::FeatureList::IsEnabled(
      features::kAutofillUseImprovedLabelDisambiguation);
#else
  use_formatter = base::FeatureList::IsEnabled(
      features::kAutofillUseMobileLabelDisambiguation);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // The formatter stores a constant reference to |profiles|.
  // This is safe since the formatter is destroyed when this function returns.
  formatter = use_formatter
                  ? LabelFormatter::Create(profiles, app_locale,
                                           trigger_field_type, field_types)
                  : nullptr;

  // Generate disambiguating labels based on the list of matches.
  std::vector<std::u16string> differentiating_labels;
  if (formatter) {
    differentiating_labels = formatter->GetLabels();
  } else {
    AutofillProfile::CreateInferredLabels(profiles, field_types,
                                          trigger_field_type, 1, app_locale,
                                          &differentiating_labels);
  }

  if (use_formatter && !profiles.empty()) {
    AutofillMetrics::LogProfileSuggestionsMadeWithFormatter(formatter !=
                                                            nullptr);
  }

  std::optional<Suggestion::Text> granular_filling_label =
      base::FeatureList::IsEnabled(features::kAutofillGranularFillingAvailable)
          ? GetGranularFillingLabel(
                optional_last_targeted_fields,
                GroupTypeOfServerFieldType(trigger_field_type))
          : absl::nullopt;
  // Creates a list of `Suggestion::Text` to be used as labels.
  std::vector<std::vector<Suggestion::Text>> suggestions_labels;
  for (const std::u16string& differentiating_label : differentiating_labels) {
    suggestions_labels.emplace_back();
    // Add granular filling specific labels if it exists.
    if (granular_filling_label) {
      suggestions_labels.back().emplace_back(
          std::move(*granular_filling_label));
      // Add a separator if the `differentiating_label` is not empty.
      // This will lead to a suggestion rendered as:
      // "Fill address - Sansa Stark".
      if (!differentiating_label.empty()) {
        suggestions_labels.back().emplace_back(u"-");
      }
    }
    if (!differentiating_label.empty()) {
      suggestions_labels.back().emplace_back(differentiating_label);
    }
  }

  return suggestions_labels;
}

// Assigns for each suggestion labels to be used as secondary text in the
// suggestion bubble, and deduplicates suggestions having the same main text
// and label. For each vector in `labels`, the last value is used to
// differentiate profiles, while the others are granular filling specific
// labels, see `GetGranularFillingLabel()`. In the case where `labels` is empty,
// we have no differentiating label for the profile.
void AssignLabelsAndDeduplicate(
    std::vector<Suggestion>& suggestions,
    const std::vector<std::vector<Suggestion::Text>>& labels,
    const std::string& app_locale) {
  DCHECK_EQ(suggestions.size(), labels.size());
  std::set<std::u16string> suggestion_text;
  size_t index_to_add_suggestion = 0;
  const AutofillProfileComparator comparator(app_locale);

  // Dedupes Suggestions to show in the dropdown once values and labels have
  // been created. This is useful when LabelFormatters make Suggestions' labels.
  //
  // Suppose profile A has the data John, 400 Oak Rd, and (617) 544-7411 and
  // profile B has the data John, 400 Oak Rd, (508) 957-5009. If a formatter
  // puts only 400 Oak Rd in the label, then there will be two Suggestions with
  // the normalized text "john400oakrd", and the Suggestion with the lower
  // ranking should be discarded.
  for (size_t i = 0; i < labels.size(); ++i) {
    // If there are no labels, consider the `differentiating_label` as an empty
    // string.
    const std::u16string& differentiating_label =
        !labels[i].empty() ? labels[i].back().value : std::u16string();

    // For example, a Suggestion with the value "John" and the label "400 Oak
    // Rd" has the normalized text "john400oakrd".
    bool text_inserted =
        suggestion_text
            .insert(AutofillProfileComparator::NormalizeForComparison(
                suggestions[i].main_text.value + differentiating_label,
                AutofillProfileComparator::DISCARD_WHITESPACE))
            .second;

    if (text_inserted) {
      if (index_to_add_suggestion != i) {
        suggestions[index_to_add_suggestion] = suggestions[i];
      }
      // The given |suggestions| are already sorted from highest to lowest
      // ranking. Suggestions with lower indices have a higher ranking and
      // should be kept.
      //
      // We check whether the value and label are the same because in certain
      // cases, e.g. when a credit card form contains a zip code field and the
      // user clicks on the zip code, a suggestion's value and the label
      // produced for it may both be a zip code.
      if (!comparator.Compare(
              suggestions[index_to_add_suggestion].main_text.value,
              differentiating_label)) {
        if (!base::FeatureList::IsEnabled(
                features::kAutofillGranularFillingAvailable)) {
          if (!differentiating_label.empty()) {
            suggestions[index_to_add_suggestion].labels = {
                {Suggestion::Text(differentiating_label)}};
          }
        } else {
          // Note that `labels[i]` can be empty, this is possible for example in
          // the field by field filling case.
          suggestions[index_to_add_suggestion].labels.emplace_back(
              std::move(labels[i]));
        }
      }
      ++index_to_add_suggestion;
    }
  }

  if (index_to_add_suggestion < suggestions.size()) {
    suggestions.resize(index_to_add_suggestion);
  }
}

// Returns whether the `suggestion_canon` is a valid match given
// `field_contents_canon`.
bool IsValidSuggestionForFieldContents(std::u16string suggestion_canon,
                                       std::u16string field_contents_canon,
                                       ServerFieldType trigger_field_type,
                                       bool is_masked_server_card,
                                       bool field_is_autofilled) {
  // Phones should do a substring match because they can be trimmed to remove
  // the first parts (e.g. country code or prefix).
  if (GroupTypeOfServerFieldType(trigger_field_type) ==
          FieldTypeGroup::kPhone &&
      suggestion_canon.find(field_contents_canon) != std::u16string::npos) {
    return true;
  }

  // For card number fields, suggest the card if:
  // - the number matches any part of the card, or
  // - it's a masked card and there are 6 or fewer typed so far.
  // - it's a masked card, field is autofilled, and the last 4 digits in the
  // field match the last 4 digits of the card.
  if (trigger_field_type == CREDIT_CARD_NUMBER) {
    if (suggestion_canon.find(field_contents_canon) != std::u16string::npos) {
      return true;
    }

    if (is_masked_server_card) {
      if (field_contents_canon.length() < 6) {
        return true;
      }
      if (field_is_autofilled) {
        int field_contents_length = field_contents_canon.length();
        DCHECK(field_contents_length >= 4);
        if (suggestion_canon.find(field_contents_canon.substr(
                field_contents_length - 4, field_contents_length)) !=
            std::u16string::npos) {
          return true;
        }
      }
    }

    return false;
  }

  return base::StartsWith(suggestion_canon, field_contents_canon,
                          base::CompareCase::SENSITIVE);
}

// Normalizes text for comparison based on the type of the field `text` was
// entered into.
std::u16string NormalizeForComparisonForType(const std::u16string& text,
                                             ServerFieldType type) {
  if (GroupTypeOfServerFieldType(type) == FieldTypeGroup::kEmail) {
    // For emails, keep special characters so that if the user has two emails
    // `test@foo.xyz` and `test1@foo.xyz` saved, only the first one is suggested
    // upon entering `test@` into the email field.
    return RemoveDiacriticsAndConvertToLowerCase(text);
  }
  return AutofillProfileComparator::NormalizeForComparison(text);
}

absl::optional<Suggestion> GetSuggestionForTestAddresses(
    base::span<const AutofillProfile> test_addresses,
    const std::string& locale) {
  if (test_addresses.empty()) {
    return absl::nullopt;
  }

  Suggestion suggestion(u"Devtools", PopupItemId::kDevtoolsTestAddresses);
  suggestion.labels = {{Suggestion::Text(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_TEST_DATA))}};
  suggestion.icon = "codeIcon";
  for (const AutofillProfile& test_address : test_addresses) {
    const std::u16string test_address_country =
        test_address.GetInfo(ADDRESS_HOME_COUNTRY, locale);
    suggestion.children.emplace_back(test_address_country,
                                     PopupItemId::kDevtoolsTestAddressEntry);
    suggestion.children.back().payload =
        Suggestion::BackendId(test_address.guid());
    suggestion.children.back().acceptance_a11y_announcement =
        l10n_util::GetStringFUTF16(IDS_AUTOFILL_TEST_ADDRESS_SELECTED_A11Y_HINT,
                                   test_address_country);
  }
  return suggestion;
}

}  // namespace

AutofillSuggestionGenerator::AutofillSuggestionGenerator(
    AutofillClient* autofill_client,
    PersonalDataManager* personal_data)
    : autofill_client_(autofill_client), personal_data_(personal_data) {}

AutofillSuggestionGenerator::~AutofillSuggestionGenerator() = default;

std::vector<Suggestion> AutofillSuggestionGenerator::GetSuggestionsForProfiles(
    const ServerFieldTypeSet& field_types,
    const FormFieldData& triggering_field,
    ServerFieldType triggering_field_type,
    absl::optional<ServerFieldTypeSet> last_targeted_fields,
    AutofillSuggestionTriggerSource trigger_source) {
  // If the user manually triggered suggestions from the context menu, all
  // available profiles should be shown. Selecting a suggestion overwrites the
  // triggering field's value.
  const std::u16string field_value_for_filtering =
      trigger_source != AutofillSuggestionTriggerSource::kManualFallbackAddress
          ? triggering_field.value
          : u"";

  std::vector<const AutofillProfile*> profiles_to_suggest =
      GetProfilesToSuggest(triggering_field_type, field_value_for_filtering,
                           triggering_field.is_autofilled, field_types);

  return CreateSuggestionsFromProfiles(
      profiles_to_suggest, field_types, last_targeted_fields,
      triggering_field_type, triggering_field.max_length);
}

std::vector<const AutofillProfile*>
AutofillSuggestionGenerator::GetProfilesToSuggest(
    ServerFieldType trigger_field_type,
    const std::u16string& field_contents,
    bool field_is_autofilled,
    const ServerFieldTypeSet& field_types) {
  std::u16string field_contents_canon =
      NormalizeForComparisonForType(field_contents, trigger_field_type);

  // Get the profiles to suggest, which are already sorted.
  std::vector<AutofillProfile*> sorted_profiles =
      personal_data_->GetProfilesToSuggest();

  // When suggesting with no prefix to match, suppress disused address
  // suggestions as well as those based on invalid profile data.
  if (field_contents_canon.empty()) {
    const base::Time min_last_used =
        AutofillClock::Now() - kDisusedDataModelTimeDelta;
    RemoveProfilesNotUsedSinceTimestamp(min_last_used, sorted_profiles);
  }

  std::vector<const AutofillProfile*> matched_profiles =
      GetPrefixMatchedProfiles(sorted_profiles, trigger_field_type,
                               field_contents, field_contents_canon,
                               field_is_autofilled);

  const AutofillProfileComparator comparator(personal_data_->app_locale());
  // Don't show two suggestions if one is a subset of the other.
  // Duplicates across sources are resolved in favour of `kAccount` profiles.
  std::vector<const AutofillProfile*> unique_matched_profiles =
      DeduplicatedProfilesForSuggestions(matched_profiles, trigger_field_type,
                                         field_types, comparator);

  return unique_matched_profiles;
}

std::vector<Suggestion>
AutofillSuggestionGenerator::CreateSuggestionsFromProfiles(
    const std::vector<const AutofillProfile*>& profiles,
    const ServerFieldTypeSet& field_types,
    absl::optional<ServerFieldTypeSet> last_targeted_fields,
    ServerFieldType trigger_field_type,
    uint64_t trigger_field_max_length) {
  std::vector<Suggestion> suggestions;
  std::string app_locale = personal_data_->app_locale();

  // This will be used to check if suggestions should be supported with icons.
  const bool contains_profile_related_fields =
      base::ranges::count_if(field_types, [](ServerFieldType field_type) {
        FieldTypeGroup field_type_group =
            GroupTypeOfServerFieldType(field_type);
        return field_type_group == FieldTypeGroup::kName ||
               field_type_group == FieldTypeGroup::kAddress ||
               field_type_group == FieldTypeGroup::kPhone ||
               field_type_group == FieldTypeGroup::kEmail;
      }) > 1;

  FieldTypeGroup trigger_field_type_group =
      GroupTypeOfServerFieldType(trigger_field_type);
  for (const AutofillProfile* profile : profiles) {
    // Compute the main text to be displayed in the suggestion bubble.
    std::u16string main_text =
        GetProfileSuggestionMainText(profile, trigger_field_type);
    if (trigger_field_type_group == FieldTypeGroup::kPhone) {
      main_text = FieldFiller::GetPhoneNumberValueForInput(
          trigger_field_max_length, main_text,
          profile->GetInfo(PHONE_HOME_CITY_AND_NUMBER, app_locale));
    }

    suggestions.emplace_back(main_text);
    suggestions.back().payload = Suggestion::BackendId(profile->guid());
    suggestions.back().acceptance_a11y_announcement =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM);
    suggestions.back().popup_item_id = GetProfileSuggestionPopupItemId(
        last_targeted_fields, trigger_field_type_group);

    // We add an icon to the address (profile) suggestion if there is more than
    // one profile related field in the form.
    if (contains_profile_related_fields) {
      // TODO(crbug.com/1459990): Remove this hardcoding once the last filling
      // granularity is available to this method. Filling granularies different
      // than full form will not have an icon.
      const bool fill_full_form = true;
      if (base::FeatureList::IsEnabled(
              features::kAutofillGranularFillingAvailable)) {
        suggestions.back().icon = fill_full_form ? "locationIcon" : "";
      } else {
        suggestions.back().icon = "accountIcon";
      }
    }

    if (profile && profile->source() == AutofillProfile::Source::kAccount &&
        profile->initial_creator_id() !=
            AutofillProfile::kInitialCreatorOrModifierChrome) {
      suggestions.back().feature_for_iph =
          feature_engagement::
              kIPHAutofillExternalAccountProfileSuggestionFeature.name;
    }

    if (base::FeatureList::IsEnabled(
            features::kAutofillGranularFillingAvailable)) {
      AddGranularFillingChildSuggestions(trigger_field_type_group,
                                         last_targeted_fields, *profile,
                                         suggestions.back());
    }
  }

  AssignLabelsAndDeduplicate(
      suggestions,
      GetProfileSuggestionLabels(profiles, field_types, trigger_field_type,
                                 last_targeted_fields, app_locale),
      app_locale);

  // Add devtools test addresses suggestion if it exists. A suggestion will
  // exist if devtools is open and therefore test addresses were set.
  if (absl::optional<Suggestion> test_addresses_suggestion =
          GetSuggestionForTestAddresses(personal_data_->test_addresses(),
                                        app_locale)) {
    std::vector<Suggestion> suggestions_with_test_address;
    suggestions_with_test_address.push_back(
        std::move(*test_addresses_suggestion));
    suggestions_with_test_address.insert(suggestions_with_test_address.end(),
                                         suggestions.begin(),
                                         suggestions.end());
    return suggestions_with_test_address;
  }

  return suggestions;
}

// TODO(crbug.com/1417975): Remove `trigger_field_type` when
// `kAutofillUseAddressRewriterInProfileSubsetComparison` launches.
std::vector<const AutofillProfile*>
AutofillSuggestionGenerator::DeduplicatedProfilesForSuggestions(
    const std::vector<const AutofillProfile*>& matched_profiles,
    ServerFieldType trigger_field_type,
    const ServerFieldTypeSet& field_types,
    const AutofillProfileComparator& comparator) {
  // TODO(crbug.com/1417975): Remove when
  // `kAutofillUseAddressRewriterInProfileSubsetComparison` launches.
  std::vector<std::u16string> suggestion_main_text;
  for (const AutofillProfile* profile : matched_profiles) {
    suggestion_main_text.push_back(
        GetProfileSuggestionMainText(profile, trigger_field_type));
  }

  std::vector<const AutofillProfile*> unique_matched_profiles;
  // Limit number of unique profiles as having too many makes the
  // browser hang due to drawing calculations (and is also not
  // very useful for the user).
  for (size_t a = 0;
       a < matched_profiles.size() &&
       unique_matched_profiles.size() < kMaxUniqueSuggestedProfilesCount;
       ++a) {
    bool include = true;
    const AutofillProfile* profile_a = matched_profiles[a];
    for (size_t b = 0; b < matched_profiles.size(); ++b) {
      const AutofillProfile* profile_b = matched_profiles[b];

      // TODO(crbug.com/1417975): Remove when
      // `kAutofillUseAddressRewriterInProfileSubsetComparison` launches.
      if (profile_a == profile_b ||
          !comparator.Compare(suggestion_main_text[a],
                              suggestion_main_text[b])) {
        continue;
      }

      if (!profile_a->IsSubsetOfForFieldSet(comparator, *profile_b,
                                            field_types)) {
        continue;
      }

      if (!profile_b->IsSubsetOfForFieldSet(comparator, *profile_a,
                                            field_types)) {
        // One-way subset. Don't include profile A.
        include = false;
        break;
      }

      // The profiles are identical and only one should be included.
      // Prefer `kAccount` profiles over `kLocalOrSyncable` ones. In case the
      // profiles have the same source, prefer the earlier one (since the
      // profiles are pre-sorted by their relevance).
      const bool prefer_a_over_b =
          profile_a->source() == profile_b->source()
              ? a < b
              : profile_a->source() == AutofillProfile::Source::kAccount;
      if (!prefer_a_over_b) {
        include = false;
        break;
      }
    }
    if (include) {
      unique_matched_profiles.push_back(profile_a);
    }
  }
  return unique_matched_profiles;
}

std::vector<const AutofillProfile*>
AutofillSuggestionGenerator::GetPrefixMatchedProfiles(
    const std::vector<AutofillProfile*>& profiles,
    ServerFieldType trigger_field_type,
    const std::u16string& raw_field_contents,
    const std::u16string& field_contents_canon,
    bool field_is_autofilled) {
  std::vector<const AutofillProfile*> matched_profiles;
  for (const AutofillProfile* profile : profiles) {
    if (matched_profiles.size() == kMaxSuggestedProfilesCount) {
      break;
    }
    // Don't offer to fill the exact same value again. If detailed suggestions
    // with different secondary data is available, it would appear to offer
    // refilling the whole form with something else. E.g. the same name with a
    // work and a home address would appear twice but a click would be a noop.
    // TODO(fhorschig): Consider refilling form instead (at least on Android).
#if BUILDFLAG(IS_ANDROID)
    if (field_is_autofilled &&
        profile->GetRawInfo(trigger_field_type) == raw_field_contents) {
      continue;
    }
#endif  // BUILDFLAG(IS_ANDROID)

    std::u16string main_text =
        GetProfileSuggestionMainText(profile, trigger_field_type);

    // Discard profiles that do not have a value for the trigger field.
    if (main_text.empty()) {
      continue;
    }

    std::u16string suggestion_canon =
        NormalizeForComparisonForType(main_text, trigger_field_type);
    if (IsValidSuggestionForFieldContents(
            suggestion_canon, field_contents_canon, trigger_field_type,
            /*is_masked_server_card=*/false, field_is_autofilled)) {
      matched_profiles.push_back(profile);
    }
  }
  return matched_profiles;
}

void AutofillSuggestionGenerator::RemoveProfilesNotUsedSinceTimestamp(
    base::Time min_last_used,
    std::vector<AutofillProfile*>& profiles) {
  const size_t original_size = profiles.size();
  base::EraseIf(profiles, [min_last_used](const AutofillProfile* profile) {
    return profile->use_date() <= min_last_used;
  });
  const size_t num_profiles_suppressed = original_size - profiles.size();
  AutofillMetrics::LogNumberOfAddressesSuppressedForDisuse(
      num_profiles_suppressed);
}

std::u16string AutofillSuggestionGenerator::GetProfileSuggestionMainText(
    const AutofillProfile* profile,
    ServerFieldType trigger_field_type) {
  std::string app_locale = personal_data_->app_locale();
  if (trigger_field_type == ADDRESS_HOME_STREET_ADDRESS) {
    std::string street_address_line;
    ::i18n::addressinput::GetStreetAddressLinesAsSingleLine(
        *i18n::CreateAddressDataFromAutofillProfile(*profile, app_locale),
        &street_address_line);
    return base::UTF8ToUTF16(street_address_line);
  }

  return profile->GetInfo(trigger_field_type, app_locale);
}

void AutofillSuggestionGenerator::AddGranularFillingChildSuggestions(
    FieldTypeGroup trigger_field_type_group,
    absl::optional<ServerFieldTypeSet> last_targeted_fields,
    const AutofillProfile& profile,
    Suggestion& suggestion) {
  std::string app_locale = personal_data_->app_locale();
  AddNameChildSuggestions(trigger_field_type_group, profile, app_locale,
                          suggestion);
  AddAddressChildSuggestions(trigger_field_type_group, profile, app_locale,
                             suggestion);
  AddContactChildSuggestions(trigger_field_type_group, profile, app_locale,
                             suggestion);
  AddFooterChildSuggestions(profile, last_targeted_fields, suggestion);
}

std::vector<Suggestion>
AutofillSuggestionGenerator::GetSuggestionsForCreditCards(
    const FormFieldData& field,
    ServerFieldType trigger_field_type,
    bool& should_display_gpay_logo,
    bool& with_offer,
    autofill_metrics::CardMetadataLoggingContext& metadata_logging_context) {
  DCHECK(GroupTypeOfServerFieldType(trigger_field_type) ==
         FieldTypeGroup::kCreditCard);
  std::vector<Suggestion> suggestions;
  const std::string& app_locale = personal_data_->app_locale();

  std::map<std::string, AutofillOfferData*> card_linked_offers_map =
      GetCardLinkedOffers(autofill_client_);
  with_offer = !card_linked_offers_map.empty();

  // The field value is sanitized before attempting to match it to the user's
  // data.
  auto field_contents = SanitizeCreditCardFieldValue(field.value);

  std::vector<CreditCard> cards_to_suggest =
      GetOrderedCardsToSuggest(autofill_client_, field_contents.empty());

  std::u16string field_contents_lower = base::i18n::ToLower(field_contents);

  metadata_logging_context =
      autofill_metrics::GetMetadataLoggingContext(cards_to_suggest);

  // Set `should_display_gpay_logo` to true if all cards are server cards, and
  // to false if any of the card is a local card.
  should_display_gpay_logo = base::ranges::all_of(
      cards_to_suggest, base::not_fn([](const CreditCard& card) {
        return CreditCard::IsLocalCard(&card);
      }));

  for (const CreditCard& credit_card : cards_to_suggest) {
    // The value of the stored data for this field type in the |credit_card|.
    std::u16string creditcard_field_value =
        credit_card.GetInfo(trigger_field_type, app_locale);
    if (creditcard_field_value.empty())
      continue;

    if (IsValidSuggestionForFieldContents(
            base::i18n::ToLower(creditcard_field_value), field_contents_lower,
            trigger_field_type,
            credit_card.record_type() ==
                CreditCard::RecordType::kMaskedServerCard,
            field.is_autofilled)) {
      bool card_linked_offer_available =
          base::Contains(card_linked_offers_map, credit_card.guid());
      if (ShouldShowVirtualCardOption(&credit_card)) {
        suggestions.push_back(CreateCreditCardSuggestion(
            credit_card, trigger_field_type,
            /*virtual_card_option=*/true, card_linked_offer_available));
      }
      suggestions.push_back(CreateCreditCardSuggestion(
          credit_card, trigger_field_type,
          /*virtual_card_option=*/false, card_linked_offer_available));
    }
  }

  return suggestions;
}

std::vector<Suggestion>
AutofillSuggestionGenerator::GetSuggestionsForVirtualCardStandaloneCvc(
    autofill_metrics::CardMetadataLoggingContext& metadata_logging_context,
    base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>&
        virtual_card_guid_to_last_four_map) {
  // TODO(crbug.com/1453739): Refactor credit card suggestion code by moving
  // duplicate logic to helper functions.
  std::vector<Suggestion> suggestions;
  std::vector<CreditCard> cards_to_suggest = GetOrderedCardsToSuggest(
      autofill_client_, /*suppress_disused_cards=*/true);
  metadata_logging_context =
      autofill_metrics::GetMetadataLoggingContext(cards_to_suggest);

  for (const CreditCard& credit_card : cards_to_suggest) {
    auto it = virtual_card_guid_to_last_four_map.find(credit_card.guid());
    if (it == virtual_card_guid_to_last_four_map.end()) {
      continue;
    }
    const std::u16string& virtual_card_last_four = *it->second;

    Suggestion suggestion;
    suggestion.icon = credit_card.CardIconStringForAutofillSuggestion();
    suggestion.popup_item_id = PopupItemId::kVirtualCreditCardEntry;
    suggestion.payload = Suggestion::BackendId(credit_card.guid());
    suggestion.feature_for_iph =
        feature_engagement::kIPHAutofillVirtualCardCVCSuggestionFeature.name;
    SetCardArtURL(suggestion, credit_card, /*virtual_card_option=*/true);
    suggestion.main_text.value =
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_VIRTUAL_CARD_STANDALONE_CVC_SUGGESTION_TITLE) +
        u" " +
        CreditCard::GetObfuscatedStringForCardDigits(/*obfuscation_length=*/4,
                                                     virtual_card_last_four);
    suggestion.labels = {
        {Suggestion::Text(credit_card.CardNameForAutofillDisplay())}};
    suggestions.push_back(suggestion);
  }
  return suggestions;
}

bool AutofillSuggestionGenerator::WasProfileSuggestionPreviouslyHidden(
    const FormStructure& form,
    const AutofillField& field,
    Suggestion::BackendId backend_id,
    const std::vector<FieldFillingSkipReason>& skip_reasons) {
  constexpr ServerFieldTypeSet street_address_field_types = {
      ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2,
      ADDRESS_HOME_LINE3};
  if (street_address_field_types.contains(field.Type().GetStorableType())) {
    // Autofill already considers suggestions as different if the suggestion's
    // main text, to be filled in the triggering field, differs regardless of
    // the other fields.
    return false;
  }
  ServerFieldTypeSet suggestion_field_types_without_address_types;
  for (size_t i = 0; i < form.field_count(); ++i) {
    // Include only non-street-address types, since those types were originally
    // ignored.
    ServerFieldType type = form.field(i)->Type().GetStorableType();
    if (skip_reasons[i] == FieldFillingSkipReason::kNotSkipped &&
        !street_address_field_types.count(type)) {
      suggestion_field_types_without_address_types.insert(type);
    }
  }

  // Get the profiles to be suggested when we remove address field types. This
  // way if the profile represented by `backend_id` is not included we can
  // conclude that it was hidden previously and is only showing now because
  // Autofill is considering address field types.
  std::vector<const AutofillProfile*> profiles_to_suggest =
      GetProfilesToSuggest(field.Type().GetStorableType(), field.value,
                           field.is_autofilled,
                           suggestion_field_types_without_address_types);

  return base::ranges::find_if(
             profiles_to_suggest, [backend_id](const AutofillProfile* profile) {
               return Suggestion::BackendId(profile->guid()) == backend_id;
             }) == profiles_to_suggest.end();
}

// static
Suggestion AutofillSuggestionGenerator::CreateSeparator() {
  Suggestion suggestion;
  suggestion.popup_item_id = PopupItemId::kSeparator;
  return suggestion;
}

// static
Suggestion AutofillSuggestionGenerator::CreateManagePaymentMethodsEntry() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS));
  suggestion.popup_item_id = PopupItemId::kAutofillOptions;
  suggestion.icon = "settingsIcon";
  return suggestion;
}

// static
std::vector<CreditCard> AutofillSuggestionGenerator::GetOrderedCardsToSuggest(
    AutofillClient* autofill_client,
    bool suppress_disused_cards) {
  DCHECK(autofill_client);
  std::map<std::string, AutofillOfferData*> card_linked_offers_map =
      GetCardLinkedOffers(autofill_client);

  PersonalDataManager* personal_data =
      autofill_client->GetPersonalDataManager();
  DCHECK(personal_data);
  std::vector<CreditCard*> available_cards =
      personal_data->GetCreditCardsToSuggest();

  // If a card has available card linked offers on the last committed url, rank
  // it to the top.
  if (!card_linked_offers_map.empty()) {
    base::ranges::stable_sort(
        available_cards,
        [&card_linked_offers_map](const CreditCard* a, const CreditCard* b) {
          return base::Contains(card_linked_offers_map, a->guid()) &&
                 !base::Contains(card_linked_offers_map, b->guid());
        });
  }

  // Suppress disused credit cards when triggered from an empty field.
  if (suppress_disused_cards) {
    const base::Time min_last_used =
        AutofillClock::Now() - kDisusedDataModelTimeDelta;
    AutofillSuggestionGenerator::
        RemoveExpiredLocalCreditCardsNotUsedSinceTimestamp(min_last_used,
                                                           available_cards);
  }

  std::vector<CreditCard> cards_to_suggest;
  cards_to_suggest.reserve(available_cards.size());
  for (const CreditCard* card : available_cards) {
    cards_to_suggest.push_back(*card);
  }
  return cards_to_suggest;
}

// static
std::vector<Suggestion> AutofillSuggestionGenerator::GetSuggestionsForIbans(
    const std::vector<const Iban*>& ibans) {
  std::vector<Suggestion> suggestions;
  suggestions.reserve(ibans.size() + 2);
  for (const Iban* iban : ibans) {
    Suggestion& suggestion =
        suggestions.emplace_back(iban->GetIdentifierStringForAutofillDisplay());
    suggestion.custom_icon =
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_AUTOFILL_IBAN);
    suggestion.popup_item_id = PopupItemId::kIbanEntry;
    if (iban->record_type() == Iban::kLocalIban) {
      suggestion.payload = Suggestion::ValueToFill(iban->GetStrippedValue());
    } else {
      CHECK(iban->record_type() == Iban::kServerIban);
      suggestion.payload = Suggestion::BackendId(iban->instrument_id());
    }
    if (!iban->nickname().empty())
      suggestion.labels = {{Suggestion::Text(iban->nickname())}};
  }

  if (suggestions.empty()) {
    return suggestions;
  }

  suggestions.push_back(CreateSeparator());
  suggestions.push_back(CreateManagePaymentMethodsEntry());
  return suggestions;
}

// static
std::vector<Suggestion>
AutofillSuggestionGenerator::GetPromoCodeSuggestionsFromPromoCodeOffers(
    const std::vector<const AutofillOfferData*>& promo_code_offers) {
  std::vector<Suggestion> suggestions;
  GURL footer_offer_details_url;
  for (const AutofillOfferData* promo_code_offer : promo_code_offers) {
    // For each promo code, create a suggestion.
    suggestions.emplace_back(
        base::ASCIIToUTF16(promo_code_offer->GetPromoCode()));
    Suggestion& suggestion = suggestions.back();
    if (!promo_code_offer->GetDisplayStrings().value_prop_text.empty()) {
      suggestion.labels = {{Suggestion::Text(base::ASCIIToUTF16(
          promo_code_offer->GetDisplayStrings().value_prop_text))}};
    }
    suggestion.payload = Suggestion::BackendId(
        base::NumberToString(promo_code_offer->GetOfferId()));
    suggestion.popup_item_id = PopupItemId::kMerchantPromoCodeEntry;

    // Every offer for a given merchant leads to the same GURL, so we grab the
    // first offer's offer details url as the payload for the footer to set
    // later.
    if (footer_offer_details_url.is_empty() &&
        !promo_code_offer->GetOfferDetailsUrl().is_empty() &&
        promo_code_offer->GetOfferDetailsUrl().is_valid()) {
      footer_offer_details_url = promo_code_offer->GetOfferDetailsUrl();
    }
  }

  // Ensure that there are suggestions and that we were able to find at least
  // one suggestion with a valid offer details url before adding the footer.
  DCHECK(suggestions.size() > 0);
  if (!footer_offer_details_url.is_empty()) {
    // Add the footer separator since we will now have a footer in the offers
    // suggestions popup.
    suggestions.push_back(CreateSeparator());

    // Add the footer suggestion that navigates the user to the promo code
    // details page in the offers suggestions popup.
    suggestions.emplace_back(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_PROMO_CODE_SUGGESTIONS_FOOTER_TEXT));
    Suggestion& suggestion = suggestions.back();
    suggestion.popup_item_id = PopupItemId::kSeePromoCodeDetails;

    // We set the payload for the footer as |footer_offer_details_url|, which is
    // the offer details url of the first offer we had for this merchant. We
    // will navigate to the url in |footer_offer_details_url| if the footer is
    // selected in AutofillExternalDelegate::DidAcceptSuggestion().
    suggestion.payload = std::move(footer_offer_details_url);
    suggestion.trailing_icon = "google";
  }
  return suggestions;
}

// static
void AutofillSuggestionGenerator::
    RemoveExpiredLocalCreditCardsNotUsedSinceTimestamp(
        base::Time min_last_used,
        std::vector<CreditCard*>& cards) {
  const size_t original_size = cards.size();
  base::EraseIf(cards, [comparison_time = AutofillClock::Now(),
                        min_last_used](const CreditCard* card) {
    return card->IsExpired(comparison_time) &&
           card->use_date() < min_last_used &&
           card->record_type() == CreditCard::RecordType::kLocalCard;
  });
  const size_t num_cards_suppressed = original_size - cards.size();
  AutofillMetrics::LogNumberOfCreditCardsSuppressedForDisuse(
      num_cards_suppressed);
}

std::u16string AutofillSuggestionGenerator::GetDisplayNicknameForCreditCard(
    const CreditCard& card) const {
  // Always prefer a local nickname if available.
  if (card.HasNonEmptyValidNickname() &&
      card.record_type() == CreditCard::RecordType::kLocalCard) {
    return card.nickname();
  }
  // Either the card a) has no nickname or b) is a server card and we would
  // prefer to use the nickname of a local card.
  std::vector<CreditCard*> candidates = personal_data_->GetCreditCards();
  for (CreditCard* candidate : candidates) {
    if (candidate->guid() != card.guid() &&
        candidate->MatchingCardDetails(card) &&
        candidate->HasNonEmptyValidNickname()) {
      return candidate->nickname();
    }
  }
  // Fall back to nickname of |card|, which may be empty.
  return card.nickname();
}

bool AutofillSuggestionGenerator::ShouldShowVirtualCardOption(
    const CreditCard* candidate_card) const {
  switch (candidate_card->record_type()) {
    case CreditCard::RecordType::kLocalCard:
      candidate_card =
          personal_data_->GetServerCardForLocalCard(candidate_card);

      // If we could not find a matching server duplicate, return false.
      if (!candidate_card) {
        return false;
      }
      ABSL_FALLTHROUGH_INTENDED;
    case CreditCard::RecordType::kMaskedServerCard:
      return ShouldShowVirtualCardOptionForServerCard(candidate_card);
    case CreditCard::RecordType::kFullServerCard:
      return false;
    case CreditCard::RecordType::kVirtualCard:
      // Should not happen since virtual card is not persisted.
      NOTREACHED();
      return false;
  }
}

// TODO(crbug.com/1346331): Separate logic for desktop, Android dropdown, and
// Keyboard Accessory.
Suggestion AutofillSuggestionGenerator::CreateCreditCardSuggestion(
    const CreditCard& credit_card,
    ServerFieldType trigger_field_type,
    bool virtual_card_option,
    bool card_linked_offer_available) const {
  DCHECK(GroupTypeOfServerFieldType(trigger_field_type) ==
         FieldTypeGroup::kCreditCard);

  Suggestion suggestion;
  suggestion.icon = credit_card.CardIconStringForAutofillSuggestion();
  CHECK(suggestion.popup_item_id == PopupItemId::kAutocompleteEntry);
  suggestion.popup_item_id = PopupItemId::kCreditCardEntry;
  suggestion.payload = Suggestion::BackendId(credit_card.guid());
#if BUILDFLAG(IS_ANDROID)
  // The card art icon should always be shown at the start of the suggestion.
  suggestion.is_icon_at_start = true;
#endif  // BUILDFLAG(IS_ANDROID)

  auto [main_text, minor_text] =
      GetSuggestionMainTextAndMinorTextForCard(credit_card, trigger_field_type);
  suggestion.main_text = std::move(main_text);
  suggestion.minor_text = std::move(minor_text);
  if (std::vector<Suggestion::Text> card_labels =
          GetSuggestionLabelsForCard(credit_card, trigger_field_type);
      !card_labels.empty()) {
    suggestion.labels.push_back(std::move(card_labels));
  }

  SetCardArtURL(suggestion, credit_card, virtual_card_option);

  // For virtual cards, make some adjustments for the suggestion contents.
  if (virtual_card_option) {
    // We don't show card linked offers for virtual card options.
    AdjustVirtualCardSuggestionContent(suggestion, credit_card,
                                       trigger_field_type);
  } else if (card_linked_offer_available) {
    // For Keyboard Accessory, set Suggestion::feature_for_iph and change the
    // suggestion icon only if card linked offers are also enabled.
    if (IsKeyboardAccessoryEnabled() &&
        base::FeatureList::IsEnabled(
            features::kAutofillEnableOffersInClankKeyboardAccessory)) {
#if BUILDFLAG(IS_ANDROID)
      suggestion.feature_for_iph =
          feature_engagement::kIPHKeyboardAccessoryPaymentOfferFeature.name;
      suggestion.icon = "offerTag";
#endif
    } else {
      // On Desktop/Android dropdown, populate an offer label.
      suggestion.labels.push_back(
          std::vector<Suggestion::Text>{Suggestion::Text(
              l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_CASHBACK))});
    }
  }

  suggestion.acceptance_a11y_announcement =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM);

  return suggestion;
}

std::pair<Suggestion::Text, Suggestion::Text>
AutofillSuggestionGenerator::GetSuggestionMainTextAndMinorTextForCard(
    const CreditCard& credit_card,
    ServerFieldType trigger_field_type) const {
  std::u16string main_text;
  std::u16string minor_text;
  if (trigger_field_type == CREDIT_CARD_NUMBER) {
    std::u16string nickname = GetDisplayNicknameForCreditCard(credit_card);
    if (ShouldSplitCardNameAndLastFourDigits()) {
      main_text = credit_card.CardNameForAutofillDisplay(nickname);
      minor_text = credit_card.ObfuscatedNumberWithVisibleLastFourDigits(
          GetObfuscationLength());
    } else {
      main_text = credit_card.CardNameAndLastFourDigits(nickname,
                                                        GetObfuscationLength());
    }
  } else if (trigger_field_type == CREDIT_CARD_VERIFICATION_CODE) {
    CHECK(!credit_card.cvc().empty());
    main_text =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_CVC_SUGGESTION_MAIN_TEXT);
  } else {
    main_text =
        credit_card.GetInfo(trigger_field_type, personal_data_->app_locale());
  }

  return {Suggestion::Text(main_text, Suggestion::Text::IsPrimary(true),
                           Suggestion::Text::ShouldTruncate(
                               ShouldSplitCardNameAndLastFourDigits())),
          // minor_text should also be shown in primary style, since it is also
          // on the first line.
          Suggestion::Text(minor_text, Suggestion::Text::IsPrimary(true))};
}

std::vector<Suggestion::Text>
AutofillSuggestionGenerator::GetSuggestionLabelsForCard(
    const CreditCard& credit_card,
    ServerFieldType trigger_field_type) const {
  DCHECK(GroupTypeOfServerFieldType(trigger_field_type) ==
         FieldTypeGroup::kCreditCard);
  const std::string& app_locale = personal_data_->app_locale();

  // If the focused field is a card number field.
  if (trigger_field_type == CREDIT_CARD_NUMBER) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    return {Suggestion::Text(
        credit_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale))};
#else
    return {Suggestion::Text(
        ShouldSplitCardNameAndLastFourDigits()
            ? credit_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale)
            : credit_card.DescriptiveExpiration(app_locale))};
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  }

  // If the focused field is not a card number field AND the card number is
  // empty (i.e. local cards added via settings page).
  std::u16string nickname = GetDisplayNicknameForCreditCard(credit_card);
  if (credit_card.number().empty()) {
    DCHECK_EQ(credit_card.record_type(), CreditCard::RecordType::kLocalCard);

    if (credit_card.HasNonEmptyValidNickname())
      return {Suggestion::Text(nickname)};

    if (trigger_field_type != CREDIT_CARD_NAME_FULL) {
      return {Suggestion::Text(
          credit_card.GetInfo(CREDIT_CARD_NAME_FULL, app_locale))};
    }
    return {};
  }

  // If the focused field is not a card number field AND the card number is NOT
  // empty.
  // On Android keyboard accessory, the label is formatted as "••1234".
  if (IsKeyboardAccessoryEnabled()) {
    return {
        Suggestion::Text(credit_card.ObfuscatedNumberWithVisibleLastFourDigits(
            GetObfuscationLength()))};
  }

  // On Desktop/Android dropdown, the label is formatted as
  // "Product Description/Nickname/Network  ••••1234". If the card name is too
  // long, it will be truncated from the tail.
  if (ShouldSplitCardNameAndLastFourDigits()) {
    return {
        Suggestion::Text(credit_card.CardNameForAutofillDisplay(nickname),
                         Suggestion::Text::IsPrimary(false),
                         Suggestion::Text::ShouldTruncate(true)),
        Suggestion::Text(credit_card.ObfuscatedNumberWithVisibleLastFourDigits(
            GetObfuscationLength()))};
  }

#if BUILDFLAG(IS_IOS)
  // On iOS, the label is formatted as either "••••1234" or "••1234", depending
  // on the obfuscation length.
  return {
      Suggestion::Text(credit_card.ObfuscatedNumberWithVisibleLastFourDigits(
          GetObfuscationLength()))};
#elif BUILDFLAG(IS_ANDROID)
  // On Android dropdown, the label is formatted as
  // "Nickname/Network  ••••1234".
  return {Suggestion::Text(credit_card.CardNameAndLastFourDigits(nickname))};
#else
  // On Desktop, the label is formatted as
  // "Product Description/Nickname/Network  ••••1234, expires on 01/25".
  return {Suggestion::Text(
      credit_card.CardIdentifierStringAndDescriptiveExpiration(app_locale))};
#endif
}

void AutofillSuggestionGenerator::AdjustVirtualCardSuggestionContent(
    Suggestion& suggestion,
    const CreditCard& credit_card,
    ServerFieldType trigger_field_type) const {
  if (credit_card.record_type() == CreditCard::RecordType::kLocalCard) {
    const CreditCard* server_duplicate_card =
        personal_data_->GetServerCardForLocalCard(&credit_card);
    DCHECK(server_duplicate_card);
    suggestion.payload = Suggestion::BackendId(server_duplicate_card->guid());
  }

  suggestion.popup_item_id = PopupItemId::kVirtualCreditCardEntry;
  suggestion.feature_for_iph =
      feature_engagement::kIPHAutofillVirtualCardSuggestionFeature.name;

  // Add virtual card labelling to suggestions. For keyboard accessory, it is
  // prefixed to the suggestion, and for the dropdown, it is shown as a label on
  // a separate line.
  const std::u16string& VIRTUAL_CARD_LABEL = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE);
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableVirtualCardMetadata)) {
    suggestion.minor_text.value = suggestion.main_text.value;
    suggestion.main_text.value = VIRTUAL_CARD_LABEL;
  } else if (IsKeyboardAccessoryEnabled()) {
    // The keyboard accessory chips can only accommodate 2 strings which are
    // displayed on a single row. The minor_text and the labels are
    // concatenated, so we have: String 1 = main_text, String 2 = minor_text +
    // labels.
    // There is a limit on the size of the keyboard accessory chips. When the
    // suggestion content exceeds this limit, the card name or the cardholder
    // name can be truncated, the last 4 digits should never be truncated.
    // Contents in the main_text are automatically truncated from the right end
    // on the Android side when the size limit is exceeded, so the card name and
    // the cardholder name is appended to the main_text.
    // Here we modify the `Suggestion` members to make it suitable for showing
    // on the keyboard accessory.
    // Card number field:
    // Before: main_text = card name, minor_text = last 4 digits, labels =
    // expiration date.
    // After: main_text = virtual card label + card name, minor_text = last 4
    // digits, labels = null.
    // Cardholder name field:
    // Before: main_text = cardholder name, minor_text = null, labels = last 4
    // digits.
    // After: main_text = virtual card label + cardholder name, minor_text =
    // null, labels = last 4 digits.
    if (ShouldSplitCardNameAndLastFourDigits()) {
      suggestion.main_text.value =
          base::StrCat({VIRTUAL_CARD_LABEL, u"  ", suggestion.main_text.value});
    } else {
      suggestion.minor_text.value = suggestion.main_text.value;
      suggestion.main_text.value = VIRTUAL_CARD_LABEL;
    }
    if (trigger_field_type == CREDIT_CARD_NUMBER) {
      // The expiration date is not shown for the card number field, so it is
      // removed.
      suggestion.labels = {};
    }
  } else {  // Desktop/Android dropdown.
    if (trigger_field_type == CREDIT_CARD_NUMBER) {
      // If the focused field is a credit card number field, reset all labels
      // and populate only the virtual card text.
      suggestion.labels = {{Suggestion::Text(VIRTUAL_CARD_LABEL)}};
    } else {
      // For other fields, add the virtual card text after the original label,
      // so it will be shown on the third line.
      suggestion.labels.push_back(
          std::vector<Suggestion::Text>{Suggestion::Text(VIRTUAL_CARD_LABEL)});
    }
  }
}

void AutofillSuggestionGenerator::SetCardArtURL(
    Suggestion& suggestion,
    const CreditCard& credit_card,
    bool virtual_card_option) const {
  const GURL card_art_url = personal_data_->GetCardArtURL(credit_card);

  if (card_art_url.is_empty() || !card_art_url.is_valid())
    return;

  // The Capital One icon for virtual cards is not card metadata, it only helps
  // distinguish FPAN from virtual cards when metadata is unavailable. FPANs
  // should only ever use the network logo or rich card art. The Capital One
  // logo is reserved for virtual cards only.
  if (!virtual_card_option && card_art_url == kCapitalOneCardArtUrl) {
    return;
  }

  // Only show card art if the experiment is enabled or if it is the Capital One
  // virtual card icon.
  if (base::FeatureList::IsEnabled(features::kAutofillEnableCardArtImage) ||
      card_art_url == kCapitalOneCardArtUrl) {
#if BUILDFLAG(IS_ANDROID)
    suggestion.custom_icon_url = card_art_url;
#else
    gfx::Image* image =
        personal_data_->GetCreditCardArtImageForUrl(card_art_url);
    if (image) {
      suggestion.custom_icon = *image;
    }
#endif
  }
}

bool AutofillSuggestionGenerator::ShouldShowVirtualCardOptionForServerCard(
    const CreditCard* card) const {
  CHECK(card);

  // If the card is not enrolled into virtual cards, we should not show a
  // virtual card suggestion for it.
  if (card->virtual_card_enrollment_state() !=
      CreditCard::VirtualCardEnrollmentState::kEnrolled) {
    return false;
  }

  // We should not show a suggestion for this card if the autofill
  // optimization guide returns that this suggestion should be blocked.
  if (auto* autofill_optimization_guide =
          autofill_client_->GetAutofillOptimizationGuide()) {
    bool blocked = autofill_optimization_guide->ShouldBlockFormFieldSuggestion(
        autofill_client_->GetLastCommittedPrimaryMainFrameOrigin().GetURL(),
        card);
    return !blocked;
  }

  // No conditions to prevent displaying a virtual card suggestion were
  // found, so return true.
  return true;
}

}  // namespace autofill
