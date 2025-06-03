// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_IBAN_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_IBAN_MANAGER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autofill_subject.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/single_field_form_filler.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace autofill {

class AutofillClient;
class PersonalDataManager;
struct SuggestionsContext;

// Per-profile IBAN Manager. This class handles IBAN-related functionality
// such as retrieving IBAN data, managing IBAN suggestions, filling IBAN fields,
// and handling form submission data when there is an IBAN field present.
class IbanManager : public SingleFieldFormFiller,
                    public KeyedService,
                    public AutofillSubject {
 public:
  // Initializes the instance with the given parameters. `personal_data_manager`
  // is a profile-scope data manager used to retrieve IBAN data from the
  // local autofill table.
  explicit IbanManager(PersonalDataManager* personal_data_manager);

  IbanManager(const IbanManager&) = delete;
  IbanManager& operator=(const IbanManager&) = delete;

  ~IbanManager() override;

  // SingleFieldFormFiller overrides:
  [[nodiscard]] bool OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource trigger_source,
      const FormFieldData& field,
      const AutofillClient& client,
      base::WeakPtr<SuggestionsHandler> handler,
      const SuggestionsContext& context) override;
  void OnWillSubmitFormWithFields(const std::vector<FormFieldData>& fields,
                                  bool is_autocomplete_enabled) override {}
  void CancelPendingQueries(const SuggestionsHandler* handler) override {}
  void OnRemoveCurrentSingleFieldSuggestion(
      const std::u16string& field_name,
      const std::u16string& value,
      PopupItemId popup_item_id) override {}
  void OnSingleFieldSuggestionSelected(const std::u16string& value,
                                       PopupItemId popup_item_id) override;

  base::WeakPtr<IbanManager> GetWeakPtr();

 private:
  // Records metrics related to the IBAN suggestions popup.
  class UmaRecorder {
   public:
    void OnIbanSuggestionsShown(FieldGlobalId field_global_id);
    void OnIbanSuggestionSelected();

   private:
    // The global id of the field that most recently had IBAN suggestions shown.
    FieldGlobalId most_recent_suggestions_shown_field_global_id_;

    // The global id of the field that most recently had an IBAN suggestion
    // selected.
    FieldGlobalId most_recent_suggestion_selected_field_global_id_;
  };

  // Sends suggestions for |ibans| to the |query_handler|'s handler for display
  // in the associated Autofill popup.
  void SendIbanSuggestions(const std::vector<Iban*>& ibans,
                           const QueryHandler& query_handler);

  const raw_ptr<PersonalDataManager> personal_data_manager_;

  UmaRecorder uma_recorder_;

  base::WeakPtrFactory<IbanManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_IBAN_MANAGER_H_
