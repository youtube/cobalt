// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/address_i18n.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_metadata.h"

namespace autofill {
namespace i18n {

namespace {

std::u16string GetInfoHelper(const AutofillProfile& profile,
                             const std::string& app_locale,
                             const AutofillType& type) {
  return profile.GetInfo(type, app_locale);
}

}  // namespace

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;

std::unique_ptr<AddressData> CreateAddressData(
    const base::RepeatingCallback<std::u16string(const AutofillType&)>&
        get_info) {
  auto address_data = std::make_unique<AddressData>();
  address_data->recipient =
      base::UTF16ToUTF8(get_info.Run(AutofillType(NAME_FULL)));
  address_data->organization =
      base::UTF16ToUTF8(get_info.Run(AutofillType(COMPANY_NAME)));
  address_data->region_code = base::UTF16ToUTF8(get_info.Run(
      AutofillType(HtmlFieldType::kCountryCode, HtmlFieldMode::kNone)));
  address_data->administrative_area =
      base::UTF16ToUTF8(get_info.Run(AutofillType(ADDRESS_HOME_STATE)));
  address_data->locality =
      base::UTF16ToUTF8(get_info.Run(AutofillType(ADDRESS_HOME_CITY)));
  address_data->dependent_locality = base::UTF16ToUTF8(
      get_info.Run(AutofillType(ADDRESS_HOME_DEPENDENT_LOCALITY)));
  address_data->sorting_code =
      base::UTF16ToUTF8(get_info.Run(AutofillType(ADDRESS_HOME_SORTING_CODE)));
  address_data->postal_code =
      base::UTF16ToUTF8(get_info.Run(AutofillType(ADDRESS_HOME_ZIP)));
  address_data->address_line = base::SplitString(
      base::UTF16ToUTF8(
          get_info.Run(AutofillType(ADDRESS_HOME_STREET_ADDRESS))),
      "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  return address_data;
}

std::unique_ptr<::i18n::addressinput::AddressData>
CreateAddressDataFromAutofillProfile(const AutofillProfile& profile,
                                     const std::string& app_locale) {
  std::unique_ptr<::i18n::addressinput::AddressData> address_data =
      i18n::CreateAddressData(
          base::BindRepeating(&GetInfoHelper, profile, app_locale));
  address_data->language_code = profile.language_code();
  return address_data;
}

ServerFieldType TypeForField(AddressField address_field) {
  switch (address_field) {
    case ::i18n::addressinput::COUNTRY:
      return ADDRESS_HOME_COUNTRY;
    case ::i18n::addressinput::ADMIN_AREA:
      return ADDRESS_HOME_STATE;
    case ::i18n::addressinput::LOCALITY:
      return ADDRESS_HOME_CITY;
    case ::i18n::addressinput::DEPENDENT_LOCALITY:
      return ADDRESS_HOME_DEPENDENT_LOCALITY;
    case ::i18n::addressinput::POSTAL_CODE:
      return ADDRESS_HOME_ZIP;
    case ::i18n::addressinput::SORTING_CODE:
      return ADDRESS_HOME_SORTING_CODE;
    case ::i18n::addressinput::STREET_ADDRESS:
      return ADDRESS_HOME_STREET_ADDRESS;
    case ::i18n::addressinput::ORGANIZATION:
      return COMPANY_NAME;
    case ::i18n::addressinput::RECIPIENT:
      return NAME_FULL;
  }
  NOTREACHED();
  return UNKNOWN_TYPE;
}

bool FieldForType(ServerFieldType server_type, AddressField* field) {
  switch (server_type) {
    case ADDRESS_HOME_COUNTRY:
      if (field)
        *field = ::i18n::addressinput::COUNTRY;
      return true;
    case ADDRESS_HOME_STATE:
      if (field)
        *field = ::i18n::addressinput::ADMIN_AREA;
      return true;
    case ADDRESS_HOME_CITY:
      if (field)
        *field = ::i18n::addressinput::LOCALITY;
      return true;
    case ADDRESS_HOME_DEPENDENT_LOCALITY:
      if (field)
        *field = ::i18n::addressinput::DEPENDENT_LOCALITY;
      return true;
    case ADDRESS_HOME_SORTING_CODE:
      if (field)
        *field = ::i18n::addressinput::SORTING_CODE;
      return true;
    case ADDRESS_HOME_ZIP:
      if (field)
        *field = ::i18n::addressinput::POSTAL_CODE;
      return true;
    case ADDRESS_HOME_STREET_ADDRESS:
    case ADDRESS_HOME_LINE1:
    case ADDRESS_HOME_LINE2:
      if (field)
        *field = ::i18n::addressinput::STREET_ADDRESS;
      return true;
    case COMPANY_NAME:
      if (field)
        *field = ::i18n::addressinput::ORGANIZATION;
      return true;
    case NAME_FULL:
      if (field)
        *field = ::i18n::addressinput::RECIPIENT;
      return true;
    default:
      return false;
  }
}

bool IsFieldRequired(ServerFieldType server_type,
                     const std::string& country_code) {
  ::i18n::addressinput::AddressField field_enum;
  if (FieldForType(server_type, &field_enum)) {
    return ::i18n::addressinput::IsFieldRequired(field_enum, country_code);
  }
  return false;
}

}  // namespace i18n
}  // namespace autofill
