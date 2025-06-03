// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_filler.h"

#include <stdint.h>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/cxx20_erase_vector.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_normalizer.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/credit_card_field.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/browser/proto/states.pb.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "ui/base/l10n/l10n_util.h"

using base::StringToInt;

namespace autofill {

namespace {

// Returns true if the value was successfully set, meaning `value` was found in
// the list of select options in `field`. Optionally, the caller may pass
// `best_match_index` which will be set to the index of the best match.
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetSelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill,
    size_t* best_match_index = nullptr) {
  l10n::CaseInsensitiveCompare compare;

  std::u16string best_match;
  for (size_t i = 0; i < field_options.size(); ++i) {
    const SelectOption& option = field_options[i];
    if (value == option.value || value == option.content) {
      // An exact match, use it.
      best_match = option.value;
      if (best_match_index) {
        *best_match_index = i;
      }
      break;
    }

    if (compare.StringsEqual(value, option.value) ||
        compare.StringsEqual(value, option.content)) {
      // A match, but not in the same case. Save it in case an exact match is
      // not found.
      best_match = option.value;
      if (best_match_index) {
        *best_match_index = i;
      }
    }
  }

  if (best_match.empty()) {
    if (failure_to_fill) {
      *failure_to_fill +=
          "Did not find value to fill in select control element. ";
    }
    return std::nullopt;
  }
  return best_match;
}

// Like GetSelectControlValue, but searches within the field values and options
// for `value`. For example, "NC - North Carolina" would match "north carolina".
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetSelectControlValueSubstringMatch(
    const std::u16string& value,
    bool ignore_whitespace,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  if (int best_match = FieldFiller::FindShortestSubstringMatchInSelect(
          value, ignore_whitespace, field_options);
      best_match >= 0) {
    return field_options[best_match].value;
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Did not find substring match for filling select control element. ";
  }
  return std::nullopt;
}

// Like GetSelectControlValue, but searches within the field values and options
// for `value`. First it tokenizes the options, then tries to match against
// tokens. For example, "NC - North Carolina" would match "nc" but not "ca".
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetSelectControlValueTokenMatch(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  const auto tokenize = [](const std::u16string& str) {
    return base::SplitString(str, base::kWhitespaceASCIIAs16,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  };
  l10n::CaseInsensitiveCompare compare;
  const auto equals_value = [&](const std::u16string& rhs) {
    return compare.StringsEqual(value, rhs);
  };
  for (const SelectOption& option : field_options) {
    if (base::ranges::any_of(tokenize(option.value), equals_value) ||
        base::ranges::any_of(tokenize(option.content), equals_value)) {
      return option.value;
    }
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Did not find token match for filling select control element. ";
  }
  return std::nullopt;
}

// Helper method to normalize the `admin_area` for the given `country_code`.
// The value in `admin_area` will be overwritten.
bool NormalizeAdminAreaForCountryCode(std::u16string* admin_area,
                                      const std::string& country_code,
                                      AddressNormalizer* address_normalizer) {
  DCHECK(address_normalizer);
  DCHECK(admin_area);
  if (admin_area->empty() || country_code.empty()) {
    return false;
  }

  AutofillProfile tmp_profile;
  tmp_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(country_code));
  tmp_profile.SetRawInfo(ADDRESS_HOME_STATE, *admin_area);
  if (!address_normalizer->NormalizeAddressSync(&tmp_profile)) {
    return false;
  }

  *admin_area = tmp_profile.GetRawInfo(ADDRESS_HOME_STATE);
  return true;
}

// Will normalize `value` and the options in `field` with `address_normalizer`
// (which should not be null), and return whether the fill was successful.
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetNormalizedStateSelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    const std::string& country_code,
    AddressNormalizer* address_normalizer,
    std::string* failure_to_fill) {
  DCHECK(address_normalizer);
  // We attempt to normalize a copy of the field value. If normalization was not
  // successful, it means the rules were probably not loaded. Give up. Note that
  // the normalizer will fetch the rule for next time it's called.
  // TODO(crbug.com/788417): We should probably sanitize |value| before
  // normalizing.
  std::u16string field_value = value;
  if (!NormalizeAdminAreaForCountryCode(&field_value, country_code,
                                        address_normalizer)) {
    if (failure_to_fill) {
      *failure_to_fill += "Could not normalize admin area for country code. ";
    }
    return std::nullopt;
  }

  // If successful, try filling the normalized value with the existing field
  // |options|.
  if (std::optional<std::u16string> select_control_value =
          GetSelectControlValue(field_value, field_options, failure_to_fill)) {
    return select_control_value;
  }

  // Normalize `field_options` using a copy.
  // TODO(crbug.com/788417): We should probably sanitize the values in
  // `field_options_copy` before normalizing.
  bool normalized = false;
  std::vector<SelectOption> field_options_copy(field_options.begin(),
                                               field_options.end());
  for (SelectOption& option : field_options_copy) {
    normalized |= NormalizeAdminAreaForCountryCode(&option.value, country_code,
                                                   address_normalizer);
    normalized |= NormalizeAdminAreaForCountryCode(
        &option.content, country_code, address_normalizer);
  }

  // Try filling the normalized value with the existing `field_options_copy`.
  size_t best_match_index = 0;
  if (normalized && GetSelectControlValue(field_value, field_options_copy,
                                          failure_to_fill, &best_match_index)) {
    // `best_match_index` now points to the option in `field->options`
    // that corresponds to our best match.
    return field_options[best_match_index].value;
  }
  if (failure_to_fill) {
    *failure_to_fill += "Could not set normalized state in control element. ";
  }
  return std::nullopt;
}

// Gets the numeric `value` to fill into `field`.
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetNumericSelectControlValue(
    int value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  for (const SelectOption& option : field_options) {
    int num;
    if ((StringToInt(option.value, &num) && num == value) ||
        (StringToInt(option.content, &num) && num == value)) {
      return option.value;
    }
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Did not find numeric value to fill in select control element. ";
  }
  return std::nullopt;
}

// Gets the state value to fill in a select control.
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetStateSelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    const std::string& country_code,
    AddressNormalizer* address_normalizer,
    std::string* failure_to_fill) {
  std::vector<std::u16string> abbreviations;
  std::vector<std::u16string> full_names;

  // Fetch the corresponding entry from AlternativeStateNameMap.
  absl::optional<StateEntry> state_entry =
      AlternativeStateNameMap::GetInstance()->GetEntry(
          AlternativeStateNameMap::CountryCode(country_code),
          AlternativeStateNameMap::StateName(value));

  // State abbreviations will be empty for non-US countries.
  if (state_entry) {
    for (const std::string& abbreviation : state_entry->abbreviations()) {
      if (!abbreviation.empty()) {
        abbreviations.push_back(base::UTF8ToUTF16(abbreviation));
      }
    }
    if (state_entry->has_canonical_name()) {
      full_names.push_back(base::UTF8ToUTF16(state_entry->canonical_name()));
    }
    for (const std::string& alternative_name :
         state_entry->alternative_names()) {
      full_names.push_back(base::UTF8ToUTF16(alternative_name));
    }
  } else {
    if (value.size() > 2) {
      full_names.push_back(value);
    } else if (!value.empty()) {
      abbreviations.push_back(value);
    }
  }

  std::u16string state_name;
  std::u16string state_abbreviation;
  state_names::GetNameAndAbbreviation(value, &state_name, &state_abbreviation);

  full_names.push_back(std::move(state_name));
  if (!state_abbreviation.empty()) {
    abbreviations.push_back(std::move(state_abbreviation));
  }

  // Remove `abbreviations` from the `full_names` as a precautionary measure in
  // case the `AlternativeStateNameMap` contains bad data.
  base::ranges::sort(abbreviations);
  base::EraseIf(full_names, [&](const std::u16string& full_name) {
    return full_name.empty() ||
           base::ranges::binary_search(abbreviations, full_name);
  });

  // Try an exact match of the abbreviation first.
  for (const std::u16string& abbreviation : abbreviations) {
    if (std::optional<std::u16string> select_control_value =
            GetSelectControlValue(abbreviation, field_options,
                                  failure_to_fill)) {
      return select_control_value;
    }
  }

  // Try an exact match of the full name.
  for (const std::u16string& full : full_names) {
    if (std::optional<std::u16string> select_control_value =
            GetSelectControlValue(full, field_options, failure_to_fill)) {
      return select_control_value;
    }
  }

  // Try an inexact match of the full name.
  for (const std::u16string& full : full_names) {
    if (std::optional<std::u16string> select_control_value =
            GetSelectControlValueSubstringMatch(
                full, /*ignore_whitespace=*/false, field_options,
                failure_to_fill)) {
      return select_control_value;
    }
  }

  // Try an inexact match of the abbreviation name.
  for (const std::u16string& abbreviation : abbreviations) {
    if (std::optional<std::u16string> select_control_value =
            GetSelectControlValueTokenMatch(abbreviation, field_options,
                                            failure_to_fill)) {
      return select_control_value;
    }
  }

  if (!address_normalizer) {
    if (failure_to_fill) {
      *failure_to_fill += "Could not fill state in select control element. ";
    }
    return std::nullopt;
  }

  // Try to match a normalized `value` of the state and the `field_options`.
  return GetNormalizedStateSelectControlValue(
      value, field_options, country_code, address_normalizer, failure_to_fill);
}

// Gets the country value to fill in a select control.
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetCountrySelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  std::string country_code = CountryNames::GetInstance()->GetCountryCode(value);
  if (country_code.empty()) {
    if (failure_to_fill) {
      *failure_to_fill += "Cannot fill empty country code. ";
    }
    return std::nullopt;
  }

  for (const SelectOption& option : field_options) {
    // Canonicalize each <option> value to a country code, and compare to the
    // target country code.
    if (country_code ==
            CountryNames::GetInstance()->GetCountryCode(option.value) ||
        country_code ==
            CountryNames::GetInstance()->GetCountryCode(option.content)) {
      return option.value;
    }
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Did not find country to fill in select control element. ";
  }
  return std::nullopt;
}

// Gets the expiration month `value` inside the <select> or <selectlist>
// `field`. Since `value` is well defined but the website's `field` option
// values may not be, some heuristics are run to cover all observed cases.
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetExpirationMonthSelectControlValue(
    const std::u16string& value,
    const std::string& app_locale,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  // |value| is defined to be between 1 and 12, inclusively.
  int month = 0;
  if (!StringToInt(value, &month) || month < 1 || month > 12) {
    if (failure_to_fill) {
      *failure_to_fill += "Cannot parse month, or value is < 1 or >12. ";
    }
    return std::nullopt;
  }

  // Trim the whitespace and specific prefixes used in AngularJS from the
  // select values before attempting to convert them to months.
  std::vector<std::u16string> trimmed_values(field_options.size());
  const std::u16string kNumberPrefix = u"number:";
  const std::u16string kStringPrefix = u"string:";
  for (size_t i = 0; i < field_options.size(); ++i) {
    base::TrimWhitespace(field_options[i].value, base::TRIM_ALL,
                         &trimmed_values[i]);
    base::ReplaceFirstSubstringAfterOffset(&trimmed_values[i], 0, kNumberPrefix,
                                           u"");
    base::ReplaceFirstSubstringAfterOffset(&trimmed_values[i], 0, kStringPrefix,
                                           u"");
  }

  if (trimmed_values.size() == 12) {
    // The select presumable only contains the year's months.
    // If the first value of the select is 0, decrement the value of `month` so
    // January is associated with 0 instead of 1.
    int first_value;
    if (StringToInt(trimmed_values[0], &first_value) && first_value == 0) {
      --month;
    }
  } else if (trimmed_values.size() == 13) {
    // The select presumably uses the first value as a placeholder.
    int first_value;
    // If the first value is not a number or is a negative one, check the second
    // value and apply the same logic as if there was no placeholder.
    if (!StringToInt(trimmed_values[0], &first_value) || first_value < 0) {
      int second_value;
      if (StringToInt(trimmed_values[1], &second_value) && second_value == 0) {
        --month;
      }
    } else if (first_value == 1) {
      // If the first value of the select is 1, increment the value of |month|
      // to skip the placeholder value (January = 2).
      ++month;
    }
  }

  // Attempt to match the user's `month` with the field's value attributes.
  for (size_t i = 0; i < trimmed_values.size(); ++i) {
    int converted_value = 0;
    // We use the trimmed value to match with `month`, but the original select
    // value to fill the field (otherwise filling wouldn't work).
    if (data_util::ParseExpirationMonth(trimmed_values[i], app_locale,
                                        &converted_value) &&
        month == converted_value) {
      return field_options[i].value;
    }
  }

  // Attempt to match with each of the options' content.
  for (const SelectOption& option : field_options) {
    int converted_contents = 0;
    if (data_util::ParseExpirationMonth(option.content, app_locale,
                                        &converted_contents) &&
        month == converted_contents) {
      return option.value;
    }
  }
  return GetNumericSelectControlValue(month, field_options, failure_to_fill);
}

// Returns true if the last two digits in `year` match those in `str`.
bool LastTwoDigitsMatch(const std::u16string& year,
                        const std::u16string& option) {
  int year_int, option_int;
  return StringToInt(year, &year_int) && StringToInt(option, &option_int) &&
         (year_int % 100) == (option_int % 100);
}

// Gets the year `value` in a select control to fill into the given `field` by
// comparing the last two digits of the year to the field's options.
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetYearSelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  if (value.size() != 2U && value.size() != 4U) {
    if (failure_to_fill) {
      *failure_to_fill += "Year to fill does not have length 2 or 4. ";
    }
    return std::nullopt;
  }

  for (const SelectOption& option : field_options) {
    if (LastTwoDigitsMatch(value, option.value) ||
        LastTwoDigitsMatch(value, option.content)) {
      return option.value;
    }
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Year to fill was not found in select control element. ";
  }
  return std::nullopt;
}

// Gets the credit card type `value` (Visa, Mastercard, etc.) to fill into the
// given `field`. We ignore whitespace when filling credit card types to
// allow for cases such as "Master card".
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetCreditCardTypeSelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  if (std::optional<std::u16string> select_control_value =
          GetSelectControlValueSubstringMatch(value, /*ignore_whitespace=*/true,
                                              field_options, failure_to_fill)) {
    return select_control_value;
  }
  if (value == l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_AMEX)) {
    // For American Express, also try filling as "AmEx".
    return GetSelectControlValueSubstringMatch(u"AmEx",
                                               /*ignore_whitespace=*/true,
                                               field_options, failure_to_fill);
  }

  if (failure_to_fill) {
    *failure_to_fill += "Failed to fill credit card type. ";
  }
  return std::nullopt;
}

std::u16string TruncateCardNumberIfNecessary(size_t card_number_offset,
                                             uint64_t field_max_length,
                                             const std::u16string& value) {
  // Take the substring of the credit card number starting from the offset
  // and ending at the field's max_length (or the entire string if
  // max_length is 0).
  // If the offset is greater than the length of the string, then the entire
  // number should be returned;
  return card_number_offset < value.length()
             ? value.substr(card_number_offset, field_max_length > 0
                                                    ? field_max_length
                                                    : std::u16string::npos)
             : value;
}

// Returns the appropriate credit card number from `credit_card`. Truncates the
// credit card number to be split across HTML form input fields depending on if
// 'field.credit_card_number_offset()' is less than the length of the credit
// card number.
std::u16string GetCreditCardNumberForInput(
    const CreditCard& credit_card,
    size_t card_number_offset,
    uint64_t field_max_length,
    const std::string& app_locale,
    mojom::ActionPersistence action_persistence) {
  std::u16string value;
  if (action_persistence == mojom::ActionPersistence::kPreview) {
    // A single field is detected when the offset begins at 0 and the field's
    // max_length can hold the entire obfuscated credit card number.
    bool is_single_field =
        (card_number_offset == 0 &&
         (field_max_length == 0 ||
          field_max_length >=
              credit_card.ObfuscatedNumberWithVisibleLastFourDigits()
                  .length()));

    // If previewing a credit card number that needs to be split, pad the number
    // to 16 digits rather than displaying a fancy string with RTL support. This
    // also returns 16 digits if there is only one field and it cannot fit the
    // longer version CC number.
    value =
        is_single_field
            ? credit_card.ObfuscatedNumberWithVisibleLastFourDigits()
            : credit_card
                  .ObfuscatedNumberWithVisibleLastFourDigitsForSplitFields();
  } else {
    value = credit_card.GetInfo(CREDIT_CARD_NUMBER, app_locale);
  }
  // Check to truncate card number based on the field's credit card number
  // offset and length of the credit card number.
  return TruncateCardNumberIfNecessary(card_number_offset, field_max_length,
                                       value);
}

// Returns the appropriate credit card number from `virtual_card`. Truncates the
// credit card number to be split across HTML form input fields depending on if
// 'field.credit_card_number_offset()' is less than the length of the credit
// card number.
std::u16string GetVirtualCardNumberForPreviewInput(
    const CreditCard& virtual_card,
    size_t card_number_offset,
    uint64_t field_max_length) {
  std::u16string value =
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE) +
      u" " + virtual_card.CardNameAndLastFourDigits();

  // |field|'s max_length truncates the credit card number to fit within.
  if (card_number_offset < value.length()) {
    // A single field is detected when the offset begins at 0 and the field's
    // max_length can hold the entire obfuscated credit card number.
    if (card_number_offset != 0 ||
        (field_max_length != 0 && field_max_length < value.length())) {
      value = virtual_card
                  .ObfuscatedNumberWithVisibleLastFourDigitsForSplitFields();
    }
    // Check to truncate card number based on the field's credit card number
    // offset and length of the credit card number.
    return TruncateCardNumberIfNecessary(card_number_offset, field_max_length,
                                         value);
  }
  return value;
}

// Returns the credit card CVC for Preview or Fill.
std::u16string GetCreditCardVerificationCodeForInput(
    const CreditCard& credit_card,
    mojom::ActionPersistence action_persistence,
    const std::u16string& cvc) {
  const std::u16string cvc_candidate =
      credit_card.cvc().empty() ? cvc : credit_card.cvc();
  if (cvc_candidate.empty()) {
    return {};
  }
  switch (action_persistence) {
    case mojom::ActionPersistence::kFill:
      return cvc_candidate;
    // For preview, we will mask CVC with dots.
    case mojom::ActionPersistence::kPreview:
      return CreditCard::GetMidlineEllipsisDots(cvc_candidate.length());
  }
}

// Gets the `value` in the select control to fill in `field`. If an exact match
// is not found, falls back to alternate filling strategies based on the `type`.
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetSelectOrSelectListControlValue(
    const AutofillType& type,
    const std::u16string& value,
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    const std::string& app_locale,
    base::span<const SelectOption> field_options,
    AddressNormalizer* address_normalizer,
    std::string* failure_to_fill) {
  ServerFieldType storable_type = type.GetStorableType();

  // Credit card expiration month is checked first since an exact match on value
  // may not be correct.
  if (storable_type == CREDIT_CARD_EXP_MONTH) {
    return GetExpirationMonthSelectControlValue(value, app_locale,
                                                field_options, failure_to_fill);
  }
  // Search for exact matches.
  if (std::optional<std::u16string> select_control_value =
          GetSelectControlValue(value, field_options, failure_to_fill)) {
    return select_control_value;
  }
  // If that fails, try specific fallbacks based on the field type.
  switch (storable_type) {
    case ADDRESS_HOME_STATE: {
      CHECK(absl::holds_alternative<const AutofillProfile*>(
          profile_or_credit_card));
      const std::string country_code = data_util::GetCountryCodeWithFallback(
          *absl::get<const AutofillProfile*>(profile_or_credit_card),
          app_locale);
      return GetStateSelectControlValue(value, field_options, country_code,
                                        address_normalizer, failure_to_fill);
    }
    case ADDRESS_HOME_COUNTRY:
      return GetCountrySelectControlValue(value, field_options,
                                          failure_to_fill);
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return GetYearSelectControlValue(value, field_options, failure_to_fill);
    case CREDIT_CARD_TYPE:
      return GetCreditCardTypeSelectControlValue(value, field_options,
                                                 failure_to_fill);
    default:
      return std::nullopt;
  }
}

// Gets the appropriate expiration date from the |card| for a month control
// field. (i.e. a <input type="month">)
std::u16string GetExpirationForMonthControl(const CreditCard& card) {
  return base::StrCat({card.Expiration4DigitYearAsString(), u"-",
                       card.Expiration2DigitMonthAsString()});
}

// Returns appropriate street address for `field`. Translates newlines into
// equivalent separators when necessary, i.e. when filling a single-line field.
// The separators depend on `address_language_code`.
std::u16string GetStreetAddressForInput(
    const std::u16string& address_value,
    const std::string& address_language_code,
    FormControlType form_control_type) {
  if (form_control_type == FormControlType::kTextArea) {
    return address_value;
  }
  ::i18n::addressinput::AddressData address_data;
  address_data.language_code = address_language_code;
  address_data.address_line =
      base::SplitString(base::UTF16ToUTF8(address_value), "\n",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::string line;
  ::i18n::addressinput::GetStreetAddressLinesAsSingleLine(address_data, &line);
  return base::UTF8ToUTF16(line);
}

// Returns appropriate state value that matches `field`.
// The canonical state is checked if it fits in the field and at last the
// abbreviations are tried. Does not return a state if neither |state_value| nor
// the canonical state name nor its abbreviation fit into the field.
std::u16string GetStateTextForInput(const std::u16string& state_value,
                                    const std::string& country_code,
                                    uint64_t field_max_length,
                                    std::string* failure_to_fill) {
  if (field_max_length == 0 || field_max_length >= state_value.size()) {
    // Return the state value directly.
    return state_value;
  }
  absl::optional<StateEntry> state =
      AlternativeStateNameMap::GetInstance()->GetEntry(
          AlternativeStateNameMap::CountryCode(country_code),
          AlternativeStateNameMap::StateName(state_value));
  if (state) {
    // Return the canonical state name if possible.
    if (state->has_canonical_name() && !state->canonical_name().empty() &&
        field_max_length >= state->canonical_name().size()) {
      return base::UTF8ToUTF16(state->canonical_name());
    }
    // Return the abbreviation if possible.
    for (const auto& abbreviation : state->abbreviations()) {
      if (!abbreviation.empty() && field_max_length >= abbreviation.size()) {
        return base::i18n::ToUpper(base::UTF8ToUTF16(abbreviation));
      }
    }
  }
  // Return with the state abbreviation.
  std::u16string abbreviation;
  state_names::GetNameAndAbbreviation(state_value, nullptr, &abbreviation);
  if (!abbreviation.empty() && field_max_length >= abbreviation.size()) {
    return base::i18n::ToUpper(abbreviation);
  }
  if (failure_to_fill) {
    *failure_to_fill += "Could not fit raw state nor abbreviation. ";
  }
  return std::u16string();
}

// Returns the appropriate expiration year from `credit_card` for the field.
// Uses the `field`'s type and the `field`'s max_length attribute to
// determine if the year needs to be truncated.
std::u16string GetExpirationYearForInput(const CreditCard& credit_card,
                                         ServerFieldType field_type,
                                         uint64_t field_max_length) {
  const size_t year_length = DetermineExpirationYearLength(field_type);
  std::u16string value = year_length == 2
                             ? credit_card.Expiration2DigitYearAsString()
                             : credit_card.Expiration4DigitYearAsString();
  // In case the field's max_length is less than the length of the year, shorten
  // the year to field.max_length.
  return field_max_length != 0 && field_max_length < value.length()
             ? value.substr(value.length() - field_max_length, field_max_length)
             : value;
}

// Returns the appropriate virtual card expiration year for the field. Uses the
// `field_type` and the `field`'s max_length attribute to determine if the year
// needs to be truncated.
std::u16string GetExpirationYearForVirtualCardPreviewInput(
    ServerFieldType storable_type,
    uint64_t field_max_length) {
  if (storable_type == CREDIT_CARD_EXP_2_DIGIT_YEAR &&
      (field_max_length == 2 ||
       field_max_length == FormFieldData::kDefaultMaxLength)) {
    return CreditCard::GetMidlineEllipsisDots(2);
  } else if (storable_type == CREDIT_CARD_EXP_4_DIGIT_YEAR &&
             (field_max_length == 4 ||
              field_max_length == FormFieldData::kDefaultMaxLength)) {
    return CreditCard::GetMidlineEllipsisDots(4);
  }

  return field_max_length > 4
             ? CreditCard::GetMidlineEllipsisDots(4)
             : CreditCard::GetMidlineEllipsisDots(field_max_length);
}

// Returns the appropriate expiration date from `credit_card` for the field
// based on the `field_type`. If the field contains a recognized date format
// string, the function follows that format. Otherwise, it uses the `field`'s
// max_length attribute to determine if the `value` needs to be truncated or
// padded. Returns an empty string in case of a failure.
std::u16string GetExpirationDateForInput(const CreditCard& credit_card,
                                         const AutofillField& field,
                                         std::string* failure_to_fill) {
  std::u16string mm = credit_card.Expiration2DigitMonthAsString();
  std::u16string yy = credit_card.Expiration2DigitYearAsString();
  std::u16string yyyy = credit_card.Expiration4DigitYearAsString();

  ServerFieldType field_type = field.Type().GetStorableType();
  // At this point the field type is determined, so we pass it even as
  // `forced_field_type`.
  CreditCardField::ExpirationDateFormat format;
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableExpirationDateImprovements)) {
    format = CreditCardField::DetermineExpirationDateFormat(
        field, /*fallback_type=*/field_type,
        /*server_hint=*/field_type, /*forced_field_type=*/field_type);
  } else {
    // Before the experiment, the type was not fully determined yet. That
    // happened at field filling time like in this else-branch.
    ServerFieldType server_hint = field.server_type();
    ServerFieldType forced_field_type =
        field.server_type_prediction_is_override() ? server_hint
                                                   : NO_SERVER_DATA;
    ServerFieldType fallback_type = field.Type().GetStorableType();
    format = CreditCardField::DetermineExpirationDateFormat(
        field, fallback_type, server_hint, forced_field_type);
  }

  std::u16string expiration_candidate =
      base::StrCat({mm, format.separator,
                    format.digits_in_expiration_year == 4 ? yyyy : yy});
  if (field.max_length != 0 &&
      expiration_candidate.length() > field.max_length) {
    if (failure_to_fill) {
      *failure_to_fill +=
          "Field to fill must have a max length of at least 4. ";
    }
    return {};
  }
  return expiration_candidate;
}

// Returns the appropriate virtual_card expiration date from for the field based
// on the `field`'s max_length. Returns an empty string in case of a failure.
std::u16string GetExpirationDateForVirtualCardPreviewInput(
    uint64_t field_max_length,
    std::string* failure_to_fill) {
  switch (field_max_length) {
    case 1:
    case 2:
    case 3:
      if (failure_to_fill) {
        *failure_to_fill +=
            "Field to fill must have a max length of at least 4. ";
      }
      return {};
    case 4:
      // Expects MMYY
      return CreditCard::GetMidlineEllipsisDots(4);
    case 5:
      // Expects MM/YY
      return CreditCard::GetMidlineEllipsisDots(2) + u"/" +
             CreditCard::GetMidlineEllipsisDots(2);
    case 6:
      // Expects MMYYYY
      return CreditCard::GetMidlineEllipsisDots(2) +
             CreditCard::GetMidlineEllipsisDots(4);
    default:
      // Return MM/YYYY for default case
      return CreditCard::GetMidlineEllipsisDots(2) + u"/" +
             CreditCard::GetMidlineEllipsisDots(4);
  }
}

std::u16string RemoveWhitespace(const std::u16string& value) {
  std::u16string stripped_value;
  base::RemoveChars(value, base::kWhitespaceUTF16, &stripped_value);
  return stripped_value;
}

// Finds the best suitable option in the `field` that corresponds to the
// `country_code`.
// If the exact match is not found, extracts the digits (ignoring leading '00'
// or '+') from each option and compares them with the `country_code`.
std::u16string GetPhoneCountryCodeSelectControlForInput(
    const std::u16string& country_code,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  if (country_code.empty()) {
    return {};
  }
  // Find the option that exactly matches the |country_code|.
  if (std::optional<std::u16string> select_control_value =
          GetSelectControlValue(country_code, field_options, failure_to_fill)) {
    return *select_control_value;
  }
  for (const SelectOption& option : field_options) {
    std::u16string cc_candidate_in_value =
        data_util::FindPossiblePhoneCountryCode(RemoveWhitespace(option.value));
    std::u16string cc_candidate_in_content =
        data_util::FindPossiblePhoneCountryCode(
            RemoveWhitespace(option.content));
    if (cc_candidate_in_value == country_code ||
        cc_candidate_in_content == country_code) {
      return option.value;
    }
  }
  return {};
}

// Returns the appropriate `credit_card` value based on `storable_type` to fill
// into `field`.
std::u16string GetValueForCreditCardForInput(
    const CreditCard& credit_card,
    const std::u16string& cvc,
    const std::string& app_locale,
    mojom::ActionPersistence action_persistence,
    const AutofillField& field,
    std::string* failure_to_fill) {
  if (field.form_control_type == FormControlType::kInputMonth) {
    return GetExpirationForMonthControl(credit_card);
  }
  switch (ServerFieldType storable_type = field.Type().GetStorableType()) {
    case CREDIT_CARD_VERIFICATION_CODE:
    case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
      return GetCreditCardVerificationCodeForInput(credit_card,
                                                   action_persistence, cvc);
    case CREDIT_CARD_NUMBER:
      return GetCreditCardNumberForInput(
          credit_card, field.credit_card_number_offset(), field.max_length,
          app_locale, action_persistence);
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return GetExpirationDateForInput(credit_card, field, failure_to_fill);
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return GetExpirationYearForInput(credit_card, storable_type,
                                       field.max_length);
    default:
      // All other cases handled here.
      return credit_card.GetInfo(storable_type, app_locale);
  }
}

// Returns the appropriate `virtual_card` value based on the type of `field` to
// preview into `field`.
std::u16string GetValueForVirtualCardInputPreview(
    const CreditCard& virtual_card,
    const std::string& app_locale,
    const AutofillField& field,
    std::string* failure_to_fill) {
  CHECK_EQ(virtual_card.record_type(), CreditCard::RecordType::kVirtualCard);
  switch (ServerFieldType storable_type = field.Type().GetStorableType()) {
    case CREDIT_CARD_VERIFICATION_CODE:
    case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
      // For preview virtual card CVC, return three dots unless for American
      // Express, which uses 4-digit CVCs.
      return virtual_card.network() == kAmericanExpressCard
                 ? CreditCard::GetMidlineEllipsisDots(4)
                 : CreditCard::GetMidlineEllipsisDots(3);
    case CREDIT_CARD_NUMBER:
      return GetVirtualCardNumberForPreviewInput(
          virtual_card, field.credit_card_number_offset(), field.max_length);
    case CREDIT_CARD_EXP_MONTH:
      // Expects MM
      return CreditCard::GetMidlineEllipsisDots(2);
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return GetExpirationYearForVirtualCardPreviewInput(storable_type,
                                                         field.max_length);
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return GetExpirationDateForVirtualCardPreviewInput(field.max_length,
                                                         failure_to_fill);
    default:
      // All other cases handled here.
      return virtual_card.GetInfo(storable_type, app_locale);
  }
}

std::optional<std::u16string> GetValueForCreditCard(
    const CreditCard& credit_card,
    const std::u16string& cvc,
    const std::string& app_locale,
    mojom::ActionPersistence action_persistence,
    const AutofillField& field,
    std::string* failure_to_fill) {
  return credit_card.record_type() == CreditCard::RecordType::kVirtualCard &&
                 action_persistence == mojom::ActionPersistence::kPreview
             ? GetValueForVirtualCardInputPreview(credit_card, app_locale,
                                                  field, failure_to_fill)
             : GetValueForCreditCardForInput(credit_card, cvc, app_locale,
                                             action_persistence, field,
                                             failure_to_fill);
}

}  // namespace

FieldFiller::FieldFiller(const std::string& app_locale,
                         AddressNormalizer* address_normalizer)
    : app_locale_(app_locale), address_normalizer_(address_normalizer) {}

FieldFiller::~FieldFiller() = default;

// static
std::u16string FieldFiller::GetValueForProfile(const AutofillProfile& profile,
                                               const std::string& app_locale,
                                               const AutofillType& field_type,
                                               const FormFieldData* field_data,
                                               std::string* failure_to_fill) {
  const std::u16string value = profile.GetInfo(field_type, app_locale);
  if (field_type.group() == FieldTypeGroup::kPhone) {
    return field_data->IsSelectOrSelectListElement() &&
                   field_type.GetStorableType() == PHONE_HOME_COUNTRY_CODE
               ? GetPhoneCountryCodeSelectControlForInput(
                     value, field_data->options, failure_to_fill)
               : FieldFiller::GetPhoneNumberValueForInput(
                     field_data->max_length, value,
                     profile.GetInfo(PHONE_HOME_CITY_AND_NUMBER, app_locale));
  }
  if (field_type.GetStorableType() == ADDRESS_HOME_STREET_ADDRESS) {
    return GetStreetAddressForInput(value, profile.language_code(),
                                    field_data->form_control_type);
  }
  if (field_type.GetStorableType() == ADDRESS_HOME_STATE) {
    return GetStateTextForInput(
        value, data_util::GetCountryCodeWithFallback(profile, app_locale),
        field_data->max_length, failure_to_fill);
  }
  return value;
}

std::u16string FieldFiller::GetValueForFilling(
    const AutofillField& field,
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    const FormFieldData* field_data,
    const std::u16string& cvc,
    mojom::ActionPersistence action_persistence,
    std::string* failure_to_fill) {
  std::u16string value;
  CHECK(field_data);
  if (absl::holds_alternative<const CreditCard*>(profile_or_credit_card)) {
    const CreditCard* credit_card =
        absl::get<const CreditCard*>(profile_or_credit_card);
    return GetValueForCreditCard(*credit_card, cvc, app_locale_,
                                 action_persistence, field, failure_to_fill)
        .value_or(u"");
  }
  // Grab AutofillProfile data.
  CHECK(
      absl::holds_alternative<const AutofillProfile*>(profile_or_credit_card));
  const AutofillProfile* profile =
      absl::get<const AutofillProfile*>(profile_or_credit_card);
  return GetValueForProfile(*profile, app_locale_, field.Type(), field_data,
                            failure_to_fill);
}

bool FieldFiller::FillFormField(
    const AutofillField& field,
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    const std::map<FieldGlobalId, std::u16string>& forced_fill_values,
    FormFieldData* field_data,
    const std::u16string& cvc,
    mojom::ActionPersistence action_persistence,
    std::string* failure_to_fill) {
  auto it = forced_fill_values.find(field.global_id());
  bool value_is_an_override = it != forced_fill_values.end();
  std::u16string value =
      value_is_an_override
          ? it->second
          : GetValueForFilling(field, profile_or_credit_card, field_data, cvc,
                               action_persistence, failure_to_fill);

  // Do not attempt to fill empty values as it would skew the metrics.
  if (value.empty()) {
    if (failure_to_fill) {
      *failure_to_fill += "No value to fill available. ";
    }
    return false;
  }
  if (field.IsSelectOrSelectListElement()) {
    std::optional<std::u16string> select_control_value =
        GetSelectOrSelectListControlValue(
            field.Type(), value, profile_or_credit_card, app_locale_,
            field_data->options, address_normalizer_, failure_to_fill);
    if (select_control_value) {
      field_data->value = *select_control_value;
    }
    return select_control_value.has_value();
  }
  field_data->value = value;
  if (value_is_an_override) {
    field_data->force_override = true;
  }
  return true;
}

// static
std::u16string FieldFiller::GetPhoneNumberValueForInput(
    uint64_t field_max_length,
    const std::u16string& number,
    const std::u16string& city_and_number) {
  // If no max length was specified, return the complete number.
  if (field_max_length == 0) {
    return number;
  }

  if (number.length() > field_max_length) {
    // Try after removing the country code, if |number| exceeds the maximum size
    // of the field.
    if (city_and_number.length() <= field_max_length) {
      return city_and_number;
    }

    // If `number` exceeds the maximum size of the field, cut the first part to
    // provide a valid number for the field. For example, the number 15142365264
    // with a field with a max length of 10 would return 5142365264, thus
    // filling in the last `field_data.max_length` characters from the `number`.
    return number.substr(number.length() - field_max_length, field_max_length);
  }

  return number;
}

// static
int FieldFiller::FindShortestSubstringMatchInSelect(
    const std::u16string& value,
    bool ignore_whitespace,
    base::span<const SelectOption> field_options) {
  int best_match = -1;

  std::u16string value_stripped =
      ignore_whitespace ? RemoveWhitespace(value) : value;
  base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents searcher(
      value_stripped);
  for (size_t i = 0; i < field_options.size(); ++i) {
    const SelectOption& option = field_options[i];
    std::u16string option_value =
        ignore_whitespace ? RemoveWhitespace(option.value) : option.value;
    std::u16string option_content =
        ignore_whitespace ? RemoveWhitespace(option.content) : option.content;
    if (searcher.Search(option_value, nullptr, nullptr) ||
        searcher.Search(option_content, nullptr, nullptr)) {
      if (best_match == -1 ||
          field_options[best_match].value.size() > option.value.size()) {
        best_match = i;
      }
    }
  }
  return best_match;
}

}  // namespace autofill
