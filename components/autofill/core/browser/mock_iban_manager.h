// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_IBAN_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_IBAN_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/iban_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockIBANManager : public IBANManager {
 public:
  explicit MockIBANManager(PersonalDataManager* personal_data_manager);

  ~MockIBANManager() override;

  MOCK_METHOD(bool,
              OnGetSingleFieldSuggestions,
              (AutoselectFirstSuggestion autoselect_first_suggestion,
               const FormFieldData& field,
               const AutofillClient& client,
               base::WeakPtr<IBANManager::SuggestionsHandler> handler,
               const SuggestionsContext& context),
              (override));
  MOCK_METHOD(void,
              OnWillSubmitFormWithFields,
              (const std::vector<FormFieldData>& fields,
               bool is_autocomplete_enabled),
              (override));
  MOCK_METHOD(void,
              CancelPendingQueries,
              (const IBANManager::SuggestionsHandler*),
              (override));
  MOCK_METHOD(void,
              OnRemoveCurrentSingleFieldSuggestion,
              (const std::u16string&, const std::u16string&, int),
              (override));
  MOCK_METHOD(void,
              OnSingleFieldSuggestionSelected,
              (const std::u16string&, int),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_IBAN_MANAGER_H_
