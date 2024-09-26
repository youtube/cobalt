// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_IBAN_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_IBAN_H_

#include <string>

#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_data_model.h"

namespace autofill {

// A form group that stores IBAN information.
// Note: Only local IBAN is supported for now.
class IBAN : public AutofillDataModel {
 public:
  explicit IBAN(const std::string& guid);

  IBAN();
  IBAN(const IBAN&);
  ~IBAN() override;

  IBAN& operator=(const IBAN& iban);

  // Returns true if IBAN value is valid. This method is case-insensitive.
  // The validation follows the below steps:
  // 1. The IBAN consists of 16 to 33 alphanumeric characters, the first two
  //    letters are country code.
  // 2. Check that the total IBAN length is correct as per the country.
  // 3. Move the four initial characters to the end of the string and replace
  //    each letter in the rearranged string with two digits, thereby expanding
  //    the string, where 'A' = 10, 'B' = 11, ..., 'Z' = 35.
  // 4. Interpret the string as a decimal integer and compute the remainder of
  //    the number on division by 97, returning true if the remainder is 1.
  //
  // The validation algorithm is from:
  // https://en.wikipedia.org/wiki/International_Bank_Account_Number#Algorithms
  static bool IsValid(const std::u16string& value);

  // Returns true if `country_code` is in the IBAN-supported country list.
  static bool IsIbanApplicableInCountry(const std::string& country_code);

  // AutofillDataModel:
  AutofillMetadata GetMetadata() const override;
  bool SetMetadata(const AutofillMetadata& metadata) override;

  // Whether the IBAN is deletable. Always returns false for now as IBAN
  // never expires.
  bool IsDeletable() const override;

  std::u16string GetRawInfo(ServerFieldType type) const override;
  void SetRawInfoWithVerificationStatus(ServerFieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;

  // Returns true if there are no values (field types) set.
  bool IsEmpty(const std::string& app_locale) const;

  // Comparison for Sync. Returns 0 if |iban| is the same as this, or < 0,
  // or > 0 if it is different. The implied ordering can be used for culling
  // duplicates. The ordering is based on the collation order of the textual
  // contents of the fields.
  // GUIDs, origins, and server id are not compared, only the values of
  // the IBANs themselves.
  int Compare(const IBAN& iban) const;

  // Equality operators compare GUIDs, origins, |value_|,
  // |nickname_| and the |server_id_|.
  bool operator==(const IBAN& iban) const;
  bool operator!=(const IBAN& iban) const;

  // Returns the ID assigned by the server. |server_id_| is empty if it's a
  // local IBAN.
  const std::string& server_id() const { return server_id_; }

  // Returns the value (the actual bank account number) of IBAN.
  const std::u16string& value() const { return value_; }
  void set_value(const std::u16string& value);

  const std::u16string& nickname() const { return nickname_; }
  // Set the |nickname_| with the processed input (replace all tabs and newlines
  // with whitespaces, condense multiple whitespaces into a single one, and
  // trim leading/trailing whitespaces).
  void set_nickname(const std::u16string& nickname);

  // Converts value (E.g., CH12 1234 1234 1234 1234) of IBAN to a partially
  // masked text formatted by the following rules:
  // 1. Always reveal the first and the last four characters.
  // 2. Mask the remaining digits if `is_value_masked` is true, otherwise,
  //    display the digits.
  // 3. The identifier string will be arranged in groups of four with a space
  //    between each group.
  //
  // Here are some examples:
  // BE71 0961 2345 6769 will be shown as: BE71 **** **** 6769.
  // CH56 0483 5012 3456 7800 9 will be shown as: CH56 **** **** **** *800 9.
  // DE91 1000 0000 0123 4567 89 will be shown as: DE91 **** **** **** **67 89.
  std::u16string GetIdentifierStringForAutofillDisplay(
      bool is_value_masked = true) const;

  // Returns a version of |value_| which does not have any separator characters
  // (e.g., '-' and ' ').
  // TODO(crbug.com/1422672): Cleanup and use value().
  std::u16string GetStrippedValue() const;

 private:
  // This is the ID assigned by the server to uniquely identify this IBAN.
  // Note: server_id is empty for now as only local IBAN is supported.
  std::string server_id_;

  // The IBAN's value, i.e., the actual bank account number.
  std::u16string value_;

  // The nickname of the IBAN. May be empty.
  std::u16string nickname_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_IBAN_H_
