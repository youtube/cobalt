// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESS_FORM_LABEL_FORMATTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESS_FORM_LABEL_FORMATTER_H_

#include <string>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ui/label_formatter.h"

namespace autofill {

// A LabelFormatter that creates Suggestions' disambiguating labels for forms
// with name and address fields and without email or phone fields.
class AddressFormLabelFormatter : public LabelFormatter {
 public:
  AddressFormLabelFormatter(const std::vector<AutofillProfile*>& profiles,
                            const std::string& app_locale,
                            ServerFieldType focused_field_type,
                            uint32_t groups,
                            const std::vector<ServerFieldType>& field_types);

  ~AddressFormLabelFormatter() override;

  std::u16string GetLabelForProfile(
      const AutofillProfile& profile,
      FieldTypeGroup focused_group) const override;

 private:
  // True if this formatter's associated form has a street address field. A
  // form may have an address-related field, e.g. zip code, without having a
  // street address field. If a form does not include a street address field,
  // street addresses should not appear in labels.
  bool form_has_street_address_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESS_FORM_LABEL_FORMATTER_H_
