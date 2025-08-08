// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/sync/base/model_type.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database.h"

class WebDatabaseService;

namespace base {
class SequencedTaskRunner;
}

namespace autofill {

class AutocompleteEntry;
class AutofillWebDataBackend;
class AutofillWebDataBackendImpl;
class AutofillWebDataServiceObserverOnDBSequence;
class AutofillWebDataServiceObserverOnUISequence;
class CreditCard;
class Iban;

// API for Autofill web data.
class AutofillWebDataService : public WebDataServiceBase {
 public:
  AutofillWebDataService(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> db_task_runner);
  AutofillWebDataService(
      scoped_refptr<WebDatabaseService> wdbs,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> db_task_runner);

  AutofillWebDataService(const AutofillWebDataService&) = delete;
  AutofillWebDataService& operator=(const AutofillWebDataService&) = delete;

  // WebDataServiceBase implementation.
  void ShutdownOnUISequence() override;

  // Schedules a task to add form fields to the web database.
  virtual void AddFormFields(const std::vector<FormFieldData>& fields);

  // Initiates the request for a vector of values which have been entered in
  // form input fields named |name|. The method OnWebDataServiceRequestDone of
  // |consumer| gets called back when the request is finished, with the vector
  // included in the argument |result|.
  virtual WebDataServiceBase::Handle GetFormValuesForElementName(
      const std::u16string& name,
      const std::u16string& prefix,
      int limit,
      WebDataServiceConsumer* consumer);

  // Removes form elements recorded for Autocomplete from the database.
  void RemoveFormElementsAddedBetween(const base::Time& delete_begin,
                                      const base::Time& delete_end);
  void RemoveFormValueForElementName(const std::u16string& name,
                                     const std::u16string& value);

  // Schedules a task to add an Autofill profile to the web database.
  void AddAutofillProfile(const AutofillProfile& profile);

  // Schedules a task to update an Autofill profile in the web database.
  void UpdateAutofillProfile(const AutofillProfile& profile);

  // Schedules a task to remove an Autofill profile from the web database.
  // |guid| is the identifier of the profile to remove.
  void RemoveAutofillProfile(const std::string& guid,
                             AutofillProfile::Source profile_source);

  // Initiates the request for local/server Autofill profiles.  The method
  // OnWebDataServiceRequestDone of |consumer| gets called when the request is
  // finished, with the profiles included in the argument |result|.  The
  WebDataServiceBase::Handle GetAutofillProfiles(
      AutofillProfile::Source profile_source,
      WebDataServiceConsumer* consumer);
  WebDataServiceBase::Handle GetServerProfiles(
      WebDataServiceConsumer* consumer);

  // Schedules a task to count the number of unique autofill values contained
  // in the time interval [|begin|, |end|). |begin| and |end| can be null
  // to indicate no time limitation.
  WebDataServiceBase::Handle GetCountOfValuesContainedBetween(
      const base::Time& begin,
      const base::Time& end,
      WebDataServiceConsumer* consumer);

  // Schedules a task to update autocomplete entries in the web database.
  void UpdateAutocompleteEntries(
      const std::vector<AutocompleteEntry>& autocomplete_entries);

  void SetAutofillProfileChangedCallback(
      base::RepeatingCallback<void(const AutofillProfileChange&)> change_cb);

  // Schedules a task to add a local IBAN to the web database.
  void AddLocalIban(const Iban& iban);

  // Initiates the request for local/server IBANs. The method
  // OnWebDataServiceRequestDone of |consumer| gets called when the request is
  // finished, with the IBAN included in the argument |result|. The consumer
  // owns the IBAN.
  WebDataServiceBase::Handle GetLocalIbans(WebDataServiceConsumer* consumer);
  WebDataServiceBase::Handle GetServerIbans(WebDataServiceConsumer* consumer);

  // Schedules a task to update a local IBAN in the web database.
  void UpdateLocalIban(const Iban& iban);

  // Schedules a task to remove an existing local IBAN from the web database.
  // `guid` is the identifier of the IBAN to remove.
  void RemoveLocalIban(const std::string& guid);

  // Schedules a task to add credit card to the web database.
  void AddCreditCard(const CreditCard& credit_card);

  // Schedules a task to update credit card in the web database.
  void UpdateCreditCard(const CreditCard& credit_card);

  // Schedules a task to update a local CVC in the web database.
  void UpdateLocalCvc(const std::string& guid, const std::u16string& cvc);

  // Schedules a task to remove a credit card from the web database.
  // |guid| is identifier of the credit card to remove.
  void RemoveCreditCard(const std::string& guid);

  // Schedules a task to add a full server credit card to the web database.
  void AddFullServerCreditCard(const CreditCard& credit_card);

  // Methods to schedule a task to add, update, remove, clear server cvc in the
  // web database.
  void AddServerCvc(int64_t instrument_id, const std::u16string& cvc);
  void UpdateServerCvc(int64_t instrument_id, const std::u16string& cvc);
  void RemoveServerCvc(int64_t instrument_id);
  void ClearServerCvcs();

  // Initiates the request for local/server credit cards.  The method
  // OnWebDataServiceRequestDone of |consumer| gets called when the request is
  // finished, with the credit cards included in the argument |result|.  The
  // consumer owns the credit cards.
  WebDataServiceBase::Handle GetCreditCards(WebDataServiceConsumer* consumer);
  WebDataServiceBase::Handle GetServerCreditCards(
      WebDataServiceConsumer* consumer);

  // Toggles the record for a server credit card between masked (only last 4
  // digits) and full (all digits).
  void UnmaskServerCreditCard(const CreditCard& card,
                              const std::u16string& full_number);
  void MaskServerCreditCard(const std::string& id);

  // Initiates the request for Payments customer data.  The method
  // OnWebDataServiceRequestDone of |consumer| gets called when the request is
  // finished, with the customer data included in the argument |result|. The
  // consumer owns the data.
  WebDataServiceBase::Handle GetPaymentsCustomerData(
      WebDataServiceConsumer* consumer);

  // Initiates the request for server credit card cloud token data. The method
  // OnWebDataServiceRequestDone of |consumer| gets called when the request is
  // finished, with the cloud token data included in the argument |result|. The
  // consumer owns the data.
  WebDataServiceBase::Handle GetCreditCardCloudTokenData(
      WebDataServiceConsumer* consumer);

  // Initiates the request for autofill offer data. The method
  // OnWebDataServiceRequestDone of |consumer| gets called when the request is
  // finished, with the offer data included in the argument |result|. The
  // consumer owns the data.
  WebDataServiceBase::Handle GetAutofillOffers(
      WebDataServiceConsumer* consumer);

  // Initiates the request for virtual card usage data. The method
  // OnWebDataServiceRequestDone of |consumer| gets called when the request is
  // finished, with the offer data included in the argument |result|. The
  // consumer owns the data.
  WebDataServiceBase::Handle GetVirtualCardUsageData(
      WebDataServiceConsumer* consumer);

  void ClearAllServerData();
  void ClearAllLocalData();

  // Updates the metadata for a server card (masked or not).
  void UpdateServerCardMetadata(const CreditCard& credit_card);

  // Updates the metadata for a server address.
  void UpdateServerAddressMetadata(const AutofillProfile& profile);

  // Removes Autofill records from the database.
  void RemoveAutofillDataModifiedBetween(const base::Time& delete_begin,
                                         const base::Time& delete_end);

  // Removes origin URLs associated with Autofill profiles and credit cards from
  // the database.
  void RemoveOriginURLsModifiedBetween(const base::Time& delete_begin,
                                       const base::Time& delete_end);

  void AddObserver(AutofillWebDataServiceObserverOnDBSequence* observer);
  void RemoveObserver(AutofillWebDataServiceObserverOnDBSequence* observer);

  void AddObserver(AutofillWebDataServiceObserverOnUISequence* observer);
  void RemoveObserver(AutofillWebDataServiceObserverOnUISequence* observer);

  // Returns a SupportsUserData object that may be used to store data accessible
  // from the DB sequence. Should be called only from the DB sequence, and will
  // be destroyed on the DB sequence soon after ShutdownOnUISequence() is
  // called.
  base::SupportsUserData* GetDBUserData();

  // Takes a callback which will be called on the DB sequence with a pointer to
  // an AutofillWebdataBackend. This backend can be used to access or update the
  // WebDatabase directly on the DB sequence.
  void GetAutofillBackend(
      base::OnceCallback<void(AutofillWebDataBackend*)> callback);

  // Returns a task runner that can be used to schedule tasks on the DB
  // sequence.
  base::SequencedTaskRunner* GetDBTaskRunner();

  // Triggers an Autocomplete retention policy run which will cleanup data that
  // hasn't been used since over the retention threshold.
  virtual WebDataServiceBase::Handle RemoveExpiredAutocompleteEntries(
      WebDataServiceConsumer* consumer);

 protected:
  ~AutofillWebDataService() override;

  void NotifyOnAutofillChangedBySyncOnUISequence(syncer::ModelType model_type);

  base::WeakPtr<AutofillWebDataService> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::ObserverList<AutofillWebDataServiceObserverOnUISequence>::Unchecked
      ui_observer_list_;

  // The task runner that this class uses for UI tasks.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // The task runner that this class uses for DB tasks.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  scoped_refptr<AutofillWebDataBackendImpl> autofill_backend_;

  // This factory is used on the UI sequence. All vended weak pointers are
  // invalidated in ShutdownOnUISequence().
  base::WeakPtrFactory<AutofillWebDataService> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_SERVICE_H_
