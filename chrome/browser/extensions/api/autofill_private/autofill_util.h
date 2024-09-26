// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_UTIL_H_

#include <map>
#include <memory>

#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

namespace autofill_util {

using AddressEntryList = std::vector<api::autofill_private::AddressEntry>;
using CountryEntryList = std::vector<api::autofill_private::CountryEntry>;
using CreditCardEntryList = std::vector<api::autofill_private::CreditCardEntry>;
using IbanEntryList = std::vector<api::autofill_private::IbanEntry>;

// Uses |personal_data| to generate a list of up-to-date AddressEntry objects.
AddressEntryList GenerateAddressList(
    const autofill::PersonalDataManager& personal_data);

// Uses |personal_data| to generate a list of up-to-date CountryEntry objects.
CountryEntryList GenerateCountryList(
    const autofill::PersonalDataManager& personal_data);

// Uses |personal_data| to generate a list of up-to-date CreditCardEntry
// objects.
CreditCardEntryList GenerateCreditCardList(
    const autofill::PersonalDataManager& personal_data);

// Uses |personal_data| to generate a list of up-to-date IbanEntry
// objects.
IbanEntryList GenerateIbanList(
    const autofill::PersonalDataManager& personal_data);

// Uses |personal_data| to get primary account info.
absl::optional<api::autofill_private::AccountInfo> GetAccountInfo(
    const autofill::PersonalDataManager& personal_data);

}  // namespace autofill_util

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_UTIL_H_
