// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/timezone.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/credit_card_art_image.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/manual_testing_import.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"
#include "components/autofill/core/browser/metrics/payments/wallet_usage_data_metrics.h"
#include "components/autofill/core/browser/metrics/stored_profile_metrics.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/strike_databases/autofill_profile_migration_strike_database.h"
#include "components/autofill/core/browser/strike_databases/autofill_profile_save_strike_database.h"
#include "components/autofill/core/browser/ui/autofill_image_fetcher.h"
#include "components/autofill/core/browser/ui/label_formatter.h"
#include "components/autofill/core/browser/ui/label_formatter_utils.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/version_info/version_info.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace {
// Checks the order of preference of the `original_card` with the
// `duplicate_card` and returns whether to dedupe/erase the `duplicate_card`
// based on the order of preference. We assume that both the cards in params are
// duplicates of each other.
//
// This function returns true in the following situations:
// Case 1: `original_card` = RecordType::kLocalCard
//         `duplicate_card` = RecordType::kMaskedServerCard
//         `should_suggest_server_cards_for_deduped_cards` = false
//
// Case 2: `original_card` = RecordType::kFullServerCard
//         `duplicate_card` = RecordType::kLocalCard
//         `should_suggest_server_cards_for_deduped_cards` = irrelevant
//
// Case 3: `original_card` = RecordType::kMaskedServerCard
//         `duplicate_card` = RecordType::kLocalCard
//         `should_suggest_server_cards_for_deduped_cards` = true
bool ShouldDedupeDuplicateCard(autofill::CreditCard* original_card,
                               autofill::CreditCard* duplicate_card) {
  // FULL_SERVER_CARDs have the highest priority and should never be removed
  // from the suggestion list.
  if (duplicate_card->record_type() ==
      autofill::CreditCard::RecordType::kFullServerCard) {
    return false;
  }
  const bool should_suggest_server_cards_for_deduped_cards =
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillSuggestServerCardInsteadOfLocalCard);

  // Delete duplicated MASKED_SERVER_CARD if the original_card is a LOCAL_CARD
  // and we are NOT suggesting MASKED_SERVER_CARD for duplicates.
  if (duplicate_card->record_type() ==
          autofill::CreditCard::RecordType::kMaskedServerCard &&
      original_card->record_type() ==
          autofill::CreditCard::RecordType::kLocalCard &&
      !should_suggest_server_cards_for_deduped_cards) {
    return true;
  }
  // Delete duplicated LOCAL_CARD if the original_card is a FULL_SERVER_CARD
  // or we are suggesting MASKED_SERVER_CARD for duplicates.
  if (duplicate_card->record_type() ==
          autofill::CreditCard::RecordType::kLocalCard &&
      (original_card->record_type() ==
           autofill::CreditCard::RecordType::kFullServerCard ||
       should_suggest_server_cards_for_deduped_cards)) {
    return true;
  }
  return false;
}
}  // namespace

namespace autofill {

using autofill_metrics::LogMandatoryReauthOfferOptInDecision;
using autofill_metrics::MandatoryReauthOfferOptInDecision;

namespace {

using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::GetStreetAddressLinesAsSingleLine;
using ::i18n::addressinput::STREET_ADDRESS;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MigrateUserOptedInWalletSyncType {
  kNotMigrated = 0,
  kMigratedFromCanonicalEmail = 1,
  kMigratedFromNonCanonicalEmail = 2,
  kNotMigratedUnexpectedPrimaryAccountIdWithEmail = 3,
  kMaxValue = kNotMigratedUnexpectedPrimaryAccountIdWithEmail,
};

template <typename T>
const T& Deref(T* x) {
  return *x;
}

template <typename T>
const T& Deref(const std::unique_ptr<T>& x) {
  return *x;
}

template <typename T>
const T& Deref(const T& x) {
  return x;
}

template <typename T>
typename base::span<T>::iterator FindElementByGUID(base::span<T> container,
                                                   std::string_view guid) {
  return base::ranges::find(container, guid, [](const auto& element) {
    return Deref(element).guid();
  });
}

template <typename T>
typename std::vector<T>::const_iterator FindElementByGUID(
    const std::vector<T>& container,
    std::string_view guid) {
  return base::ranges::find(container, guid, [](const auto& element) {
    return Deref(element).guid();
  });
}

template <typename C>
bool FindByGUID(const C& container, std::string_view guid) {
  return FindElementByGUID(container, guid) != container.end();
}

template <typename C, typename T>
bool FindByContents(const C& container, const T& needle) {
  return base::ranges::any_of(container, [&needle](const auto& element) {
    return element->Compare(needle) == 0;
  });
}

// Receives the loaded profiles from the web data service and stores them in
// |*dest|. The pending handle is the address of the pending handle
// corresponding to this request type. This function is used to save both server
// and local profiles and credit cards.
template <typename ValueType>
void ReceiveLoadedDbValues(WebDataServiceBase::Handle h,
                           WDTypedResult* result,
                           WebDataServiceBase::Handle* pending_handle,
                           std::vector<std::unique_ptr<ValueType>>* dest) {
  DCHECK_EQ(*pending_handle, h);
  *pending_handle = 0;

  *dest = std::move(
      static_cast<WDResult<std::vector<std::unique_ptr<ValueType>>>*>(result)
          ->GetValue());
}

// A helper function for finding the maximum value in a string->int map.
static bool CompareVotes(const std::pair<std::string, int>& a,
                         const std::pair<std::string, int>& b) {
  return a.second < b.second;
}

// Orders all `profiles` by the specified `order` rule.
void OrderProfiles(std::vector<AutofillProfile*>& profiles,
                   PersonalDataManager::ProfileOrder order) {
  switch (order) {
    case PersonalDataManager::ProfileOrder::kNone:
      break;
    case PersonalDataManager::ProfileOrder::kHighestFrecencyDesc:
      // TODO(crbug.com/1411114): Remove code duplication for sorting profiles.
      base::ranges::sort(profiles, [comparison_time = AutofillClock::Now()](
                                       AutofillProfile* a, AutofillProfile* b) {
        return a->HasGreaterRankingThan(b, comparison_time);
      });
      break;
    case PersonalDataManager::ProfileOrder::kMostRecentlyModifiedDesc:
      base::ranges::sort(profiles, [](AutofillProfile* a, AutofillProfile* b) {
        return a->modification_date() > b->modification_date();
      });
      break;
    case PersonalDataManager::ProfileOrder::kMostRecentlyUsedFirstDesc:
      base::ranges::sort(profiles, [](AutofillProfile* a, AutofillProfile* b) {
        return a->use_date() > b->use_date();
      });
      break;
  }
}

}  // namespace

// Helper class to abstract the switching between account and profile storage
// for server cards away from the rest of PersonalDataManager.
class PersonalDatabaseHelper
    : public AutofillWebDataServiceObserverOnUISequence {
 public:
  explicit PersonalDatabaseHelper(PersonalDataManager* personal_data_manager)
      : personal_data_manager_(personal_data_manager) {}

  PersonalDatabaseHelper(const PersonalDatabaseHelper&) = delete;
  PersonalDatabaseHelper& operator=(const PersonalDatabaseHelper&) = delete;

  void Init(scoped_refptr<AutofillWebDataService> profile_database,
            scoped_refptr<AutofillWebDataService> account_database) {
    profile_database_ = profile_database;
    account_database_ = account_database;

    if (!profile_database_) {
      // In some tests, there are no dbs.
      return;
    }

    // Start observing the profile database. Don't observe the account database
    // until we know that we should use it.
    profile_database_->AddObserver(personal_data_manager_);

    // If we don't have an account_database , we always use the profile database
    // for server data.
    if (!account_database_) {
      server_database_ = profile_database_;
    } else {
      // Wait for the call to SetUseAccountStorageForServerData to decide
      // which database to use for server data.
      server_database_ = nullptr;
    }
  }

  ~PersonalDatabaseHelper() override {
    if (profile_database_) {
      profile_database_->RemoveObserver(personal_data_manager_);
    }

    // If we have a different server database, also remove its observer.
    if (server_database_ && server_database_ != profile_database_) {
      server_database_->RemoveObserver(personal_data_manager_);
    }
  }

  // Returns the database that should be used for storing local data.
  scoped_refptr<AutofillWebDataService> GetLocalDatabase() {
    return profile_database_;
  }

  // Returns the database that should be used for storing server data.
  scoped_refptr<AutofillWebDataService> GetServerDatabase() {
    return server_database_;
  }

  // Whether we're currently using the ephemeral account storage for saving
  // server data.
  bool IsUsingAccountStorageForServerData() {
    return server_database_ != profile_database_;
  }

  // Set whether this should use the passed in account storage for server
  // addresses. If false, this will use the profile_storage.
  // It's an error to call this if no account storage was passed in at
  // construction time.
  void SetUseAccountStorageForServerData(
      bool use_account_storage_for_server_cards) {
    if (!profile_database_) {
      // In some tests, there are no dbs.
      return;
    }
    scoped_refptr<AutofillWebDataService> new_server_database =
        use_account_storage_for_server_cards ? account_database_
                                             : profile_database_;
    DCHECK(new_server_database != nullptr)
        << "SetUseAccountStorageForServerData("
        << use_account_storage_for_server_cards << "): storage not available.";

    if (new_server_database == server_database_) {
      // Nothing to do :)
      return;
    }

    if (server_database_ != nullptr) {
      if (server_database_ != profile_database_) {
        // Remove the previous observer if we had any.
        server_database_->RemoveObserver(personal_data_manager_);
      }
      personal_data_manager_->CancelPendingServerQueries();
    }
    server_database_ = new_server_database;
    // We don't need to add an observer if server_database_ is equal to
    // profile_database_, because we're already observing that.
    if (server_database_ != profile_database_) {
      server_database_->AddObserver(personal_data_manager_);
    }
    // Notify the manager that the database changed.
    personal_data_manager_->Refresh();
  }

 private:
  scoped_refptr<AutofillWebDataService> profile_database_;
  scoped_refptr<AutofillWebDataService> account_database_;

  // The database that should be used for server data. This will always be equal
  // to either profile_database_, or account_database_.
  scoped_refptr<AutofillWebDataService> server_database_;

  raw_ptr<PersonalDataManager> personal_data_manager_;
};

PersonalDataManager::PersonalDataManager(
    const std::string& app_locale,
    const std::string& variations_country_code)
    : app_locale_(app_locale),
      variations_country_code_(variations_country_code) {
  database_helper_ = std::make_unique<PersonalDatabaseHelper>(this);
}

PersonalDataManager::PersonalDataManager(const std::string& app_locale)
    : PersonalDataManager(app_locale, std::string()) {}

void PersonalDataManager::Init(
    scoped_refptr<AutofillWebDataService> profile_database,
    scoped_refptr<AutofillWebDataService> account_database,
    PrefService* pref_service,
    PrefService* local_state,
    signin::IdentityManager* identity_manager,
    history::HistoryService* history_service,
    syncer::SyncService* sync_service,
    StrikeDatabaseBase* strike_database,
    AutofillImageFetcherBase* image_fetcher) {
  database_helper_->Init(profile_database, account_database);

  SetPrefService(pref_service);

  // Listen for the preference changes.
  pref_registrar_.Init(pref_service);

  alternative_state_name_map_updater_ =
      std::make_unique<AlternativeStateNameMapUpdater>(local_state, this);
  AddObserver(alternative_state_name_map_updater_.get());

  // Listen for URL deletions from browsing history.
  history_service_ = history_service;
  if (history_service_)
    history_service_observation_.Observe(history_service_.get());

  // Listen for account cookie deletion by the user.
  identity_manager_ = identity_manager;
  if (identity_manager_)
    identity_manager_->AddObserver(this);

  SetSyncService(sync_service);

  image_fetcher_ = image_fetcher;

  AutofillMetrics::LogIsAutofillEnabledAtStartup(IsAutofillEnabled());
  AutofillMetrics::LogIsAutofillProfileEnabledAtStartup(
      IsAutofillProfileEnabled());
  AutofillMetrics::LogIsAutofillCreditCardEnabledAtStartup(
      IsAutofillPaymentMethodsEnabled());

  if (strike_database) {
    profile_migration_strike_database_ =
        std::make_unique<AutofillProfileMigrationStrikeDatabase>(
            strike_database);
    profile_save_strike_database_ =
        std::make_unique<AutofillProfileSaveStrikeDatabase>(strike_database);
    profile_update_strike_database_ =
        std::make_unique<AutofillProfileUpdateStrikeDatabase>(strike_database);
  }

  // WebDataService may not be available in tests.
  if (!database_helper_->GetLocalDatabase()) {
    return;
  }

  database_helper_->GetLocalDatabase()->SetAutofillProfileChangedCallback(
      base::BindRepeating(&PersonalDataManager::OnAutofillProfileChanged,
                          weak_factory_.GetWeakPtr()));

  Refresh();

  personal_data_manager_cleaner_ = std::make_unique<PersonalDataManagerCleaner>(
      this, alternative_state_name_map_updater_.get(), pref_service);

  // Potentially import profiles for testing. `Init()` is called whenever the
  // corresponding Chrome profile is created. This is either during start-up or
  // when the Chrome profile is changed.
  MaybeImportDataForManualTesting(weak_factory_.GetWeakPtr());
}

PersonalDataManager::~PersonalDataManager() {
  CancelPendingLocalQuery(&pending_synced_local_profiles_query_);
  CancelPendingLocalQuery(&pending_creditcards_query_);
  CancelPendingServerQueries();

  if (alternative_state_name_map_updater_)
    RemoveObserver(alternative_state_name_map_updater_.get());
}

void PersonalDataManager::Shutdown() {
  if (sync_service_)
    sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;

  if (history_service_)
    history_service_observation_.Reset();
  history_service_ = nullptr;

  if (identity_manager_)
    identity_manager_->RemoveObserver(this);
  identity_manager_ = nullptr;
}

void PersonalDataManager::OnURLsDeleted(
    history::HistoryService* /* history_service */,
    const history::DeletionInfo& deletion_info) {
  for (PersonalDataManagerObserver& observer : observers_) {
    observer.OnBrowsingHistoryCleared(deletion_info);
  }

  if (!deletion_info.is_from_expiration() && deletion_info.IsAllHistory()) {
    AutofillDownloadManager::ClearUploadHistory(pref_service_);
  }

  if (profile_save_strike_database_) {
    if (deletion_info.IsAllHistory()) {
      // If the whole history is deleted, clear all strikes.
      profile_save_strike_database_->ClearAllStrikes();
    } else {
      std::set<std::string> deleted_hosts;
      for (const auto& url_row : deletion_info.deleted_rows()) {
        deleted_hosts.insert(url_row.url().host());
      }
      if (deletion_info.time_range().IsValid() &&
          !deletion_info.time_range().IsAllTime()) {
        profile_save_strike_database_->ClearStrikesByOriginAndTimeInternal(
            deleted_hosts, deletion_info.time_range().begin(),
            deletion_info.time_range().end());
      } else {
        profile_save_strike_database_->ClearStrikesByOrigin(deleted_hosts);
      }
    }
  }
}

void PersonalDataManager::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK(pending_synced_local_profiles_query_ ||
         pending_account_profiles_query_ ||
         pending_creditcard_billing_addresses_query_ ||
         pending_creditcards_query_ || pending_server_creditcards_query_ ||
         pending_server_creditcard_cloud_token_data_query_ ||
         pending_local_ibans_query_ || pending_server_ibans_query_ ||
         pending_customer_data_query_ || pending_offer_data_query_ ||
         pending_virtual_card_usage_data_query_);

  if (!result) {
    // Error from the web database.
    if (h == pending_synced_local_profiles_query_) {
      pending_synced_local_profiles_query_ = 0;
    } else if (h == pending_account_profiles_query_) {
      pending_account_profiles_query_ = 0;
    } else if (h == pending_creditcard_billing_addresses_query_) {
      pending_creditcard_billing_addresses_query_ = 0;
    } else if (h == pending_creditcards_query_) {
      pending_creditcards_query_ = 0;
    } else if (h == pending_server_creditcards_query_) {
      pending_server_creditcards_query_ = 0;
    } else if (h == pending_server_creditcard_cloud_token_data_query_) {
      pending_server_creditcard_cloud_token_data_query_ = 0;
    } else if (h == pending_local_ibans_query_) {
      pending_local_ibans_query_ = 0;
    } else if (h == pending_server_ibans_query_) {
      pending_server_ibans_query_ = 0;
    } else if (h == pending_customer_data_query_) {
      pending_customer_data_query_ = 0;
    } else if (h == pending_offer_data_query_) {
      pending_offer_data_query_ = 0;
    } else if (h == pending_virtual_card_usage_data_query_) {
      pending_virtual_card_usage_data_query_ = 0;
    }
  } else {
    switch (result->GetType()) {
      case AUTOFILL_PROFILES_RESULT:
        if (h == pending_synced_local_profiles_query_) {
          ReceiveLoadedDbValues(h, result.get(),
                                &pending_synced_local_profiles_query_,
                                &synced_local_profiles_);
        } else if (h == pending_account_profiles_query_) {
          ReceiveLoadedDbValues(h, result.get(),
                                &pending_account_profiles_query_,
                                &account_profiles_);
        } else {
          DCHECK_EQ(h, pending_creditcard_billing_addresses_query_)
              << "received profiles from invalid request.";
          ReceiveLoadedDbValues(h, result.get(),
                                &pending_creditcard_billing_addresses_query_,
                                &credit_card_billing_addresses_);
        }
        break;
      case AUTOFILL_CREDITCARDS_RESULT:
        if (h == pending_creditcards_query_) {
          ReceiveLoadedDbValues(h, result.get(), &pending_creditcards_query_,
                                &local_credit_cards_);
        } else {
          DCHECK_EQ(h, pending_server_creditcards_query_)
              << "received creditcards from invalid request.";
          ReceiveLoadedDbValues(h, result.get(),
                                &pending_server_creditcards_query_,
                                &server_credit_cards_);
          OnServerCreditCardsRefreshed();
        }
        break;
      case AUTOFILL_CLOUDTOKEN_RESULT:
        DCHECK_EQ(h, pending_server_creditcard_cloud_token_data_query_)
            << "received credit card cloud token data from invalid request.";
        ReceiveLoadedDbValues(
            h, result.get(), &pending_server_creditcard_cloud_token_data_query_,
            &server_credit_card_cloud_token_data_);
        break;
      case AUTOFILL_IBANS_RESULT:
        if (h == pending_local_ibans_query_) {
          ReceiveLoadedDbValues(h, result.get(), &pending_local_ibans_query_,
                                &local_ibans_);
        } else {
          DCHECK_EQ(h, pending_server_ibans_query_)
              << "received ibans from invalid request.";
          ReceiveLoadedDbValues(h, result.get(), &pending_server_ibans_query_,
                                &server_ibans_);
        }
        break;
      case AUTOFILL_CUSTOMERDATA_RESULT:
        DCHECK_EQ(h, pending_customer_data_query_)
            << "received customer data from invalid request.";
        pending_customer_data_query_ = 0;

        payments_customer_data_ =
            static_cast<WDResult<std::unique_ptr<PaymentsCustomerData>>*>(
                result.get())
                ->GetValue();
        break;
      case AUTOFILL_OFFER_DATA:
        DCHECK_EQ(h, pending_offer_data_query_)
            << "received autofill offer data from invalid request.";
        ReceiveLoadedDbValues(h, result.get(), &pending_offer_data_query_,
                              &autofill_offer_data_);
        break;
      case AUTOFILL_VIRTUAL_CARD_USAGE_DATA:
        DCHECK_EQ(h, pending_virtual_card_usage_data_query_)
            << "received autofill virtual card usage data from invalid "
               "request.";
        ReceiveLoadedDbValues(h, result.get(),
                              &pending_virtual_card_usage_data_query_,
                              &autofill_virtual_card_usage_data_);
        break;
      default:
        NOTREACHED();
    }
  }

  if (HasPendingQueries())
    return;

  if (!database_helper_->GetServerDatabase()) {
    DLOG(WARNING) << "There are no pending queries but the server database "
                     "wasn't set yet, so some data might be missing. Maybe "
                     "SetSyncService() wasn't called yet.";
    return;
  }

  // All personal data is loaded, notify observers. |is_data_loaded_| is false
  // if this is the initial load.
  if (!is_data_loaded_) {
    is_data_loaded_ = true;
    personal_data_manager_cleaner_->CleanupDataAndNotifyPersonalDataObservers();
  } else {
    NotifyPersonalDataObserver();
  }
}

void PersonalDataManager::OnAutofillChangedBySync(
    syncer::ModelType model_type) {
  Refresh();

  // Note, it's possible that the cleanups are run on the stale data since
  // `Refresh` is an async operation. But, since the cleanups happen over the
  // local data, it should be fine.
  personal_data_manager_cleaner_->ApplyAddressAndCardFixesAndCleanups(
      model_type);
}

void PersonalDataManager::OnStateChanged(syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);

  for (PersonalDataManagerObserver& observer : observers_) {
    observer.OnPersonalDataSyncStateChanged();
  }

  // Use the ephemeral account storage when the user didn't enable the sync
  // feature explicitly. `sync_service` is nullptr-checked because this
  // method can also be used (apart from the Sync service observer's calls) in
  // SetSyncService() where setting a nullptr is possible.
  // TODO(crbug.com/1462552): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  database_helper_->SetUseAccountStorageForServerData(
      sync_service && !sync_service->IsSyncFeatureEnabled());
}

void PersonalDataManager::OnSyncPaymentsIntegrationEnabledChanged(
    syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);

  if (!sync_service_->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kPayments)) {
    // Re-mask all server cards when the user turns off wallet card
    // integration.
    ResetFullServerCards();
    NotifyPersonalDataObserver();
  }
}

void PersonalDataManager::OnSyncShutdown(syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);
  sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;
}

CoreAccountInfo PersonalDataManager::GetAccountInfoForPaymentsServer() const {
  // Return the account of the active signed-in user irrespective of whether
  // they enabled sync or not.
  return identity_manager_->GetPrimaryAccountInfo(
      signin::ConsentLevel::kSignin);
}

bool PersonalDataManager::IsSyncFeatureEnabledForPaymentsServerMetrics() const {
  // TODO(crbug.com/1462552): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  return sync_service_ && sync_service_->IsSyncFeatureEnabled();
}

void PersonalDataManager::OnAccountsCookieDeletedByUserAction() {
  // Clear all the Sync Transport feature opt-ins.
  prefs::ClearSyncTransportOptIns(pref_service_);
}

absl::optional<CoreAccountInfo> PersonalDataManager::GetPrimaryAccountInfo()
    const {
  if (identity_manager_ &&
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return identity_manager_->GetPrimaryAccountInfo(
        signin::ConsentLevel::kSignin);
  }

  return absl::nullopt;
}

bool PersonalDataManager::IsPaymentsDownloadActive() const {
  if (!sync_service_ || !identity_manager_ ||
      sync_service_->GetAccountInfo().IsEmpty() ||
      sync_service_->GetTransportState() ==
          syncer::SyncService::TransportState::PAUSED) {
    return false;
  }
  // TODO(crbug.com/1462552): Simplify (merge with
  // IsPaymentsWalletSyncTransportEnabled()) once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  return sync_service_->IsSyncFeatureEnabled() ||
         sync_service_->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA);
}

bool PersonalDataManager::IsPaymentsWalletSyncTransportEnabled() const {
  if (!sync_service_ || !identity_manager_ ||
      sync_service_->GetAccountInfo().IsEmpty() ||
      sync_service_->GetTransportState() ==
          syncer::SyncService::TransportState::PAUSED) {
    return false;
  }
  // TODO(crbug.com/1462552): Simplify (merge with IsPaymentsDownloadActive())
  // once ConsentLevel::kSync and SyncService::IsSyncFeatureEnabled() are
  // deleted from the codebase.
  return !sync_service_->IsSyncFeatureEnabled() &&
         sync_service_->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA);
}

AutofillMetrics::PaymentsSigninState
PersonalDataManager::GetPaymentsSigninStateForMetrics() const {
  using PaymentsSigninState = AutofillMetrics::PaymentsSigninState;

  // Check if the user is signed out.
  if (!sync_service_ || !identity_manager_ ||
      sync_service_->GetAccountInfo().IsEmpty()) {
    return PaymentsSigninState::kSignedOut;
  }

  if (sync_service_->GetTransportState() ==
      syncer::SyncService::TransportState::PAUSED) {
    return PaymentsSigninState::kSyncPaused;
  }

  // Check if the user has turned on sync.
  // TODO(crbug.com/1462552): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  if (sync_service_->IsSyncFeatureEnabled()) {
    return PaymentsSigninState::kSignedInAndSyncFeatureEnabled;
  }

  // Check if Wallet data types are supported.
  if (sync_service_->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA)) {
    return PaymentsSigninState::kSignedInAndWalletSyncTransportEnabled;
  }

  return PaymentsSigninState::kSignedIn;
}

void PersonalDataManager::AddObserver(PersonalDataManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void PersonalDataManager::RemoveObserver(
    PersonalDataManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PersonalDataManager::MarkObserversInsufficientFormDataForImport() {
  for (PersonalDataManagerObserver& observer : observers_)
    observer.OnInsufficientFormData();
}

void PersonalDataManager::RecordUseOf(
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card) {
  if (absl::holds_alternative<const CreditCard*>(profile_or_credit_card)) {
    CreditCard* credit_card = GetCreditCardByGUID(
        absl::get<const CreditCard*>(profile_or_credit_card)->guid());
    if (!credit_card) {
      return;
    }

    credit_card->RecordAndLogUse();

    if (credit_card->record_type() == CreditCard::RecordType::kLocalCard) {
      // Fail silently if there's no local database, because we need to
      // support this for tests.
      if (database_helper_->GetLocalDatabase()) {
        database_helper_->GetLocalDatabase()->UpdateCreditCard(*credit_card);
      }
    } else {
      DCHECK(database_helper_->GetServerDatabase())
          << "Recording use of server card without server storage.";
      database_helper_->GetServerDatabase()->UpdateServerCardMetadata(
          *credit_card);
    }

    Refresh();
  } else {
    // TODO(crbug.com/941498): Server profiles are not recorded therefore
    // GetProfileByGUID returns null for them.
    AutofillProfile* profile = GetProfileByGUID(
        absl::get<const AutofillProfile*>(profile_or_credit_card)->guid());
    if (!profile) {
      return;
    }

    AutofillProfile updated_profile = *profile;
    updated_profile.RecordAndLogUse();

    switch (updated_profile.record_type()) {
      case AutofillProfile::LOCAL_PROFILE:
        UpdateProfile(updated_profile);
        break;
      case AutofillProfile::SERVER_PROFILE:
        DCHECK(database_helper_->GetServerDatabase())
            << "Recording use of server address without server storage.";
        database_helper_->GetServerDatabase()->UpdateServerAddressMetadata(
            updated_profile);
        Refresh();
        break;
    }
  }
}

void PersonalDataManager::AddProfile(const AutofillProfile& profile) {
  if (!IsAutofillProfileEnabled() || !database_helper_->GetLocalDatabase()) {
    return;
  }
  if (profile.IsEmpty(app_locale_)) {
    // TODO(crbug.com/1007974): This call is only used to notify tests to stop
    // waiting. Since no profile is added, this case shouldn't trigger
    // `OnPersonalDataChanged()`.
    NotifyPersonalDataObserver();
    return;
  }
  ongoing_profile_changes_[profile.guid()].emplace_back(
      AutofillProfileChange(AutofillProfileChange::ADD, profile.guid(),
                            profile),
      /*is_ongoing=*/false);
  HandleNextProfileChange(profile.guid());
}

void PersonalDataManager::UpdateProfile(const AutofillProfile& profile) {
  if (!database_helper_->GetLocalDatabase())
    return;

  // If the profile is empty, remove it unconditionally.
  if (profile.IsEmpty(app_locale_)) {
    RemoveByGUID(profile.guid());
    return;
  }

  // The profile is a duplicate of an existing profile if it has a distinct GUID
  // but the same content.
  // Duplicates can exist across profile sources.
  const std::vector<std::unique_ptr<AutofillProfile>>& profiles =
      GetProfileStorage(profile.source());
  auto duplicate_profile_iter =
      base::ranges::find_if(profiles, [&profile](const auto& other_profile) {
        return profile.guid() != other_profile->guid() &&
               other_profile->Compare(profile) == 0;
      });

  // Remove the profile if it is a duplicate of another already existing
  // profile.
  if (duplicate_profile_iter != profiles.end()) {
    // Keep the more recently used version of the profile.
    if (profile.use_date() > duplicate_profile_iter->get()->use_date()) {
      UpdateProfileInDB(profile);
      RemoveByGUID(duplicate_profile_iter->get()->guid());
    } else {
      RemoveByGUID(profile.guid());
    }
    return;
  }

  UpdateProfileInDB(profile);
}

AutofillProfile* PersonalDataManager::GetProfileByGUID(
    const std::string& guid) const {
  // GUIDs are unique among profile sources.
  std::vector<AutofillProfile*> profiles = GetProfiles();
  auto iter = FindElementByGUID(profiles, guid);
  return iter != profiles.end() ? *iter : nullptr;
}

bool PersonalDataManager::IsEligibleForAddressAccountStorage() const {
  // The CONTACT_INFO data type is only running for eligible users. See
  // ContactInfoModelTypeController. Some additional countries are excluded
  // based on their GeoIP.
  return sync_service_ &&
         sync_service_->GetActiveDataTypes().Has(syncer::CONTACT_INFO) &&
         base::FeatureList::IsEnabled(
             features::kAutofillAccountProfileStorage) &&
         (features::kAutofillAccountProfileStorageFromUnsupportedIPs.Get() ||
          IsCountryEligibleForAccountStorage(variations_country_code_));
}

bool PersonalDataManager::IsCountryEligibleForAccountStorage(
    base::StringPiece country_code) const {
  constexpr char const* kUnsupportedCountries[] = {"CU", "IR", "KP", "SD",
                                                   "SY"};
  return !base::Contains(kUnsupportedCountries, country_code);
}

void PersonalDataManager::MigrateProfileToAccount(
    const AutofillProfile& profile) {
  CHECK_EQ(profile.source(), AutofillProfile::Source::kLocalOrSyncable);
  AutofillProfile account_profile = profile.ConvertToAccountProfile();
  DCHECK_NE(profile.guid(), account_profile.guid());
  // Update the database (and this way indirectly Sync).
  RemoveByGUID(profile.guid());
  AddProfile(account_profile);
}

std::string PersonalDataManager::AddIban(Iban iban) {
  CHECK_EQ(iban.record_type(), Iban::kUnknown);
  // IBAN shares the same pref with payment methods enablement toggle.
  if (!IsAutofillPaymentMethodsEnabled()) {
    return std::string();
  }

  // Sets the `kAutofillHasSeenIban` pref to true indicating that the user has
  // added an IBAN via Chrome payment settings page or accepted the save-IBAN
  // prompt, which indicates that the user is familiar with IBANs as a concept.
  // We set the pref so that even if the user travels to a country where IBAN
  // functionality is not typically used, they will still be able to save new
  // IBANs from the settings page using this pref.
  SetAutofillHasSeenIban();

  if (!database_helper_->GetLocalDatabase()) {
    return std::string();
  }

  // Set the GUID as this IBAN will be saved locally.
  iban.set_identifier(
      Iban::Guid(base::Uuid::GenerateRandomV4().AsLowercaseString()));
  iban.set_record_type(Iban::kLocalIban);
  // Search through `local_ibans_` to ensure no IBAN that already saved has the
  // same value and nickname as `iban`, because we do not want to add two IBANs
  // with the exact same data.
  if (base::ranges::any_of(
          local_ibans_, [&iban](const std::unique_ptr<Iban>& iban_from_list) {
            return iban.value().compare(iban_from_list->value()) == 0 &&
                   iban.nickname().compare(iban_from_list->nickname());
          })) {
    return std::string();
  }

  // Add the new IBAN to the web database.
  database_helper_->GetLocalDatabase()->AddLocalIban(iban);

  // Refresh our local cache and send notifications to observers.
  Refresh();
  return iban.guid();
}

std::string PersonalDataManager::UpdateIban(const Iban& iban) {
  if (!database_helper_->GetLocalDatabase())
    return std::string();

  // Make the update.
  database_helper_->GetLocalDatabase()->UpdateLocalIban(iban);

  // Refresh our local cache and send notifications to observers.
  Refresh();
  return iban.guid();
}

void PersonalDataManager::AddCreditCard(const CreditCard& credit_card) {
  if (!IsAutofillPaymentMethodsEnabled()) {
    return;
  }

  if (credit_card.IsEmpty(app_locale_))
    return;

  if (FindByGUID(local_credit_cards_, credit_card.guid()))
    return;

  if (!database_helper_->GetLocalDatabase())
    return;

  // Don't add a duplicate.
  if (FindByContents(local_credit_cards_, credit_card))
    return;

  // Add the new credit card to the web database.
  database_helper_->GetLocalDatabase()->AddCreditCard(credit_card);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::DeleteLocalCreditCards(
    const std::vector<CreditCard>& cards) {
  DCHECK(database_helper_);
  DCHECK(database_helper_->GetLocalDatabase())
      << "Use of local card without local storage.";

  for (const auto& card : cards)
    database_helper_->GetLocalDatabase()->RemoveCreditCard(card.guid());

  // Refresh the database, so latest state is reflected in all consumers.
  if (!cards.empty())
    Refresh();
}

void PersonalDataManager::DeleteAllLocalCreditCards() {
  std::vector<CreditCard*> credit_cards = GetLocalCreditCards();

  std::vector<CreditCard> cards_to_delete;
  cards_to_delete.reserve(credit_cards.size());
  for (const CreditCard* card : credit_cards) {
    cards_to_delete.push_back(*card);
  }

  DeleteLocalCreditCards(cards_to_delete);
}

void PersonalDataManager::UpdateCreditCard(const CreditCard& credit_card) {
  DCHECK_EQ(CreditCard::RecordType::kLocalCard, credit_card.record_type());
  CreditCard* existing_credit_card = GetCreditCardByGUID(credit_card.guid());
  if (!existing_credit_card)
    return;

  // Don't overwrite the origin for a credit card that is already stored.
  if (existing_credit_card->Compare(credit_card) == 0)
    return;

  if (credit_card.IsEmpty(app_locale_)) {
    RemoveByGUID(credit_card.guid());
    return;
  }

  // Update the cached version.
  *existing_credit_card = credit_card;

  if (!database_helper_->GetLocalDatabase())
    return;

  // Make the update.
  database_helper_->GetLocalDatabase()->UpdateCreditCard(credit_card);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::UpdateLocalCvc(const std::string& guid,
                                         const std::u16string& cvc) {
  if (!database_helper_->GetLocalDatabase()) {
    return;
  }

  CreditCard* existing_credit_card = GetCreditCardByGUID(guid);
  if (!existing_credit_card) {
    return;
  }

  database_helper_->GetLocalDatabase()->UpdateLocalCvc(guid, cvc);
  Refresh();
}

void PersonalDataManager::AddFullServerCreditCard(
    const CreditCard& credit_card) {
  DCHECK_EQ(CreditCard::RecordType::kFullServerCard, credit_card.record_type());
  DCHECK(!credit_card.IsEmpty(app_locale_));
  DCHECK(!credit_card.server_id().empty());
  DCHECK(database_helper_->GetServerDatabase())
      << "Adding server card without server storage.";

  // Don't add a duplicate.
  if (FindByGUID(server_credit_cards_, credit_card.guid()) ||
      FindByContents(server_credit_cards_, credit_card))
    return;

  // Add the new credit card to the web database.
  database_helper_->GetServerDatabase()->AddFullServerCreditCard(credit_card);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::UpdateServerCreditCard(
    const CreditCard& credit_card) {
  DCHECK_NE(CreditCard::RecordType::kLocalCard, credit_card.record_type());

  if (!database_helper_->GetServerDatabase()) {
    return;
  }

  // Look up by server id, not GUID.
  const CreditCard* existing_credit_card = nullptr;
  for (const auto& server_card : server_credit_cards_) {
    if (credit_card.server_id() == server_card->server_id()) {
      existing_credit_card = server_card.get();
      break;
    }
  }
  if (!existing_credit_card)
    return;

  DCHECK_NE(existing_credit_card->record_type(), credit_card.record_type());
  DCHECK_EQ(existing_credit_card->Label(), credit_card.Label());
  if (existing_credit_card->record_type() ==
      CreditCard::RecordType::kMaskedServerCard) {
    database_helper_->GetServerDatabase()->UnmaskServerCreditCard(
        credit_card, credit_card.number());
  } else {
    database_helper_->GetServerDatabase()->MaskServerCreditCard(
        credit_card.server_id());
  }

  Refresh();
}

void PersonalDataManager::UpdateServerCardsMetadata(
    const std::vector<CreditCard>& credit_cards) {
  DCHECK(database_helper_->GetServerDatabase())
      << "Updating server card metadata without server storage.";

  for (const auto& credit_card : credit_cards) {
    DCHECK_NE(CreditCard::RecordType::kLocalCard, credit_card.record_type());
    database_helper_->GetServerDatabase()->UpdateServerCardMetadata(
        credit_card);
  }

  Refresh();
}

void PersonalDataManager::AddServerCvc(int64_t instrument_id,
                                       const std::u16string& cvc) {
  // We don't check the validity of the instrument_id.
  // When a user saves a new card along with the CVC, we first save the card and
  // wait for the instrument id passed back from the UploadResponse. Then this
  // function is triggered to save server cvc. At this time, a new card should
  // theoretically be synced down via Chrome Sync but it can be delayed. As a
  // result, this Chrome client does not have the instrument id yet in the card
  // table but it should invoke the AddServerCvc.
  CHECK(!cvc.empty());
  CHECK(database_helper_->GetServerDatabase())
      << "Adding Server cvc without server storage.";

  // Add the new server cvc to the web database.
  database_helper_->GetServerDatabase()->AddServerCvc(instrument_id, cvc);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::UpdateServerCvc(int64_t instrument_id,
                                          const std::u16string& cvc) {
  CHECK(GetCreditCardByInstrumentId(instrument_id));
  CHECK(!cvc.empty());
  CHECK(database_helper_->GetServerDatabase())
      << "Updating Server cvc without server storage.";

  // Update the new server cvc to the web database.
  database_helper_->GetServerDatabase()->UpdateServerCvc(instrument_id, cvc);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::RemoveServerCvc(int64_t instrument_id) {
  // We don't check the validity of the instrument_id.
  // This is only called in cvc sync bridge's ApplyIncrementalSyncChanges()
  // call. If the card sync finishes before cvc sync, the card is gone before
  // removing cvc.
  CHECK(database_helper_->GetServerDatabase())
      << "Removing Server cvc without server storage.";

  // Remove the server cvc in the web database.
  database_helper_->GetServerDatabase()->RemoveServerCvc(instrument_id);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::ClearServerCvcs() {
  CHECK(database_helper_->GetServerDatabase())
      << "Removing Server cvc without server storage.";

  // Clear the server cvc in the web database.
  database_helper_->GetServerDatabase()->ClearServerCvcs();

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::ResetFullServerCard(const std::string& guid) {
  for (const auto& card : server_credit_cards_) {
    if (card->guid() == guid) {
      DCHECK_EQ(card->record_type(), CreditCard::RecordType::kFullServerCard);
      CreditCard card_copy = *card;
      card_copy.set_record_type(CreditCard::RecordType::kMaskedServerCard);
      card_copy.SetNumber(card->LastFourDigits());
      UpdateServerCreditCard(card_copy);
      break;
    }
  }
}

void PersonalDataManager::ResetFullServerCards() {
  for (const auto& card : server_credit_cards_) {
    if (card->record_type() == CreditCard::RecordType::kFullServerCard) {
      CreditCard card_copy = *card;
      card_copy.set_record_type(CreditCard::RecordType::kMaskedServerCard);
      card_copy.SetNumber(card->LastFourDigits());
      UpdateServerCreditCard(card_copy);
    }
  }
}

void PersonalDataManager::ClearAllServerDataForTesting() {
  // This could theoretically be called before we get the data back from the
  // database on startup, and it could get called when the wallet pref is
  // off (meaning this class won't even query for the server data) so don't
  // check the server_credit_cards_/profiles_ before posting to the DB.

  // TODO(crbug.com/864519): Move this null check logic to the database helper.
  // The server database can be null for a limited amount of time before the
  // sync service gets initialized. Not clearing it does not matter in that case
  // since it will not have been created yet (nothing to clear).
  if (database_helper_->GetServerDatabase())
    database_helper_->GetServerDatabase()->ClearAllServerData();

  // The above call will eventually clear our server data by notifying us
  // that the data changed and then this class will re-fetch. Preemptively
  // clear so that tests can synchronously verify that this data was cleared.
  server_credit_cards_.clear();
  server_ibans_.clear();
  credit_card_billing_addresses_.clear();
  payments_customer_data_.reset();
  server_credit_card_cloud_token_data_.clear();
  autofill_offer_data_.clear();
  credit_card_art_images_.clear();
}

void PersonalDataManager::ClearAllLocalData() {
  database_helper_->GetLocalDatabase()->ClearAllLocalData();
  local_credit_cards_.clear();
  local_ibans_.clear();
  synced_local_profiles_.clear();
}

void PersonalDataManager::AddServerCreditCardForTest(
    std::unique_ptr<CreditCard> credit_card) {
  server_credit_cards_.push_back(std::move(credit_card));
}

bool PersonalDataManager::IsUsingAccountStorageForServerDataForTest() const {
  return database_helper_->IsUsingAccountStorageForServerData();
}

void PersonalDataManager::AddOfferDataForTest(
    std::unique_ptr<AutofillOfferData> offer_data) {
  autofill_offer_data_.push_back(std::move(offer_data));
}

void PersonalDataManager::SetSyncServiceForTest(
    syncer::SyncService* sync_service) {
  // Before the sync service pointer gets changed, remove the observer.
  if (sync_service_) {
    sync_service_->RemoveObserver(this);
    sync_service_ = nullptr;
  }
  SetSyncService(sync_service);
}

void PersonalDataManager::RemoveByGUID(const std::string& guid) {
  if (!database_helper_->GetLocalDatabase())
    return;

  if (FindByGUID(local_credit_cards_, guid)) {
    database_helper_->GetLocalDatabase()->RemoveCreditCard(guid);
    // Refresh our local cache and send notifications to observers.
    Refresh();
  } else if (FindByGUID(local_ibans_, guid)) {
    database_helper_->GetLocalDatabase()->RemoveLocalIban(guid);
    // Refresh our local cache and send notifications to observers.
    Refresh();
  } else {
    RemoveProfileFromDB(guid);
  }
}

Iban* PersonalDataManager::GetIbanByGUID(const std::string& guid) {
  const std::vector<Iban*>& ibans = GetLocalIbans();
  auto iter = FindElementByGUID(ibans, guid);
  return iter != ibans.end() ? *iter : nullptr;
}

CreditCard* PersonalDataManager::GetCreditCardByGUID(const std::string& guid) {
  const std::vector<CreditCard*>& credit_cards = GetCreditCards();
  auto iter = FindElementByGUID(credit_cards, guid);
  return iter != credit_cards.end() ? *iter : nullptr;
}

CreditCard* PersonalDataManager::GetCreditCardByNumber(
    const std::string& number) {
  CreditCard numbered_card;
  numbered_card.SetNumber(base::ASCIIToUTF16(number));
  for (CreditCard* credit_card : GetCreditCards()) {
    DCHECK(credit_card);
    if (credit_card->MatchingCardDetails(numbered_card)) {
      return credit_card;
    }
  }
  return nullptr;
}

CreditCard* PersonalDataManager::GetCreditCardByInstrumentId(
    int64_t instrument_id) {
  const std::vector<CreditCard*> credit_cards = GetCreditCards();
  for (CreditCard* credit_card : credit_cards) {
    if (credit_card->instrument_id() == instrument_id)
      return credit_card;
  }
  return nullptr;
}

CreditCard* PersonalDataManager::GetCreditCardByServerId(
    const std::string& server_id) {
  const std::vector<CreditCard*> server_credit_cards = GetServerCreditCards();
  for (CreditCard* credit_card : server_credit_cards) {
    if (credit_card->server_id() == server_id)
      return credit_card;
  }
  return nullptr;
}

void PersonalDataManager::GetNonEmptyTypes(
    ServerFieldTypeSet* non_empty_types) const {
  for (AutofillProfile* profile : GetProfiles())
    profile->GetNonEmptyTypes(app_locale_, non_empty_types);
  for (CreditCard* card : GetCreditCards())
    card->GetNonEmptyTypes(app_locale_, non_empty_types);
}

bool PersonalDataManager::IsDataLoaded() const {
  return is_data_loaded_;
}

std::vector<AutofillProfile*> PersonalDataManager::GetProfiles(
    ProfileOrder order) const {
  std::vector<AutofillProfile*> a = GetProfilesFromSource(
      AutofillProfile::Source::kLocalOrSyncable, ProfileOrder::kNone);
  std::vector<AutofillProfile*> b = GetProfilesFromSource(
      AutofillProfile::Source::kAccount, ProfileOrder::kNone);
  a.reserve(a.size() + b.size());
  base::ranges::move(b, std::back_inserter(a));
  OrderProfiles(a, order);
  return a;
}

std::vector<AutofillProfile*> PersonalDataManager::GetProfilesFromSource(
    AutofillProfile::Source profile_source,
    ProfileOrder order) const {
  const std::vector<std::unique_ptr<AutofillProfile>>& profiles =
      GetProfileStorage(profile_source);
  std::vector<AutofillProfile*> result;
  result.reserve(profiles.size());
  for (const auto& profile : profiles)
    result.push_back(profile.get());
  OrderProfiles(result, order);
  return result;
}

std::vector<AutofillProfile*> PersonalDataManager::GetServerProfiles() const {
  std::vector<AutofillProfile*> result;
  if (!IsAutofillProfileEnabled())
    return result;
  result.reserve(credit_card_billing_addresses_.size());
  for (const auto& profile : credit_card_billing_addresses_)
    result.push_back(profile.get());
  return result;
}

std::vector<CreditCard*> PersonalDataManager::GetLocalCreditCards() const {
  std::vector<CreditCard*> result;
  result.reserve(local_credit_cards_.size());
  for (const auto& card : local_credit_cards_)
    result.push_back(card.get());
  return result;
}

std::vector<CreditCard*> PersonalDataManager::GetServerCreditCards() const {
  std::vector<CreditCard*> result;
  if (!IsAutofillWalletImportEnabled())
    return result;

  result.reserve(server_credit_cards_.size());
  for (const auto& card : server_credit_cards_) {
    result.push_back(card.get());
  }
  return result;
}

std::vector<CreditCard*> PersonalDataManager::GetCreditCards() const {
  std::vector<CreditCard*> result;
  result.reserve(local_credit_cards_.size() + server_credit_cards_.size());
  for (const auto& card : local_credit_cards_)
    result.push_back(card.get());
  if (IsAutofillWalletImportEnabled()) {
    for (const auto& card : server_credit_cards_) {
      result.push_back(card.get());
    }
  }
  return result;
}

std::vector<Iban*> PersonalDataManager::GetLocalIbans() const {
  std::vector<Iban*> result;
  result.reserve(local_ibans_.size());
  for (const auto& iban : local_ibans_) {
    result.push_back(iban.get());
  }
  return result;
}

std::vector<const Iban*> PersonalDataManager::GetServerIbans() const {
  std::vector<const Iban*> result;
  if (!IsAutofillWalletImportEnabled()) {
    return result;
  }

  result.reserve(server_ibans_.size());
  for (const std::unique_ptr<Iban>& iban : server_ibans_) {
    result.push_back(iban.get());
  }
  return result;
}

std::vector<const Iban*> PersonalDataManager::GetIbans() const {
  std::vector<const Iban*> result;
  result.reserve(local_ibans_.size() + server_ibans_.size());
  if (IsAutofillWalletImportEnabled()) {
    for (const std::unique_ptr<Iban>& iban : server_ibans_) {
      result.push_back(iban.get());
    }
  }

  for (const std::unique_ptr<Iban>& iban : local_ibans_) {
    result.push_back(iban.get());
  }
  return result;
}

std::vector<const Iban*> PersonalDataManager::GetIbansToSuggest() const {
  std::vector<const Iban*> ibans_to_suggest = GetIbans();
  // Remove any IBAN from the returned list if it's a local IBAN and its
  // prefix, suffix, and length matches any existing server IBAN.
  base::EraseIf(ibans_to_suggest, [this](const Iban* iban) {
    return iban->record_type() == Iban::kLocalIban &&
           base::ranges::any_of(
               server_ibans_, [&](const std::unique_ptr<Iban>& server_iban) {
                 return server_iban->MatchesPrefixSuffixAndLength(*iban);
               });
  });

  return ibans_to_suggest;
}

PaymentsCustomerData* PersonalDataManager::GetPaymentsCustomerData() const {
  return payments_customer_data_ ? payments_customer_data_.get() : nullptr;
}

std::vector<CreditCardCloudTokenData*>
PersonalDataManager::GetCreditCardCloudTokenData() const {
  std::vector<CreditCardCloudTokenData*> result;
  if (!IsAutofillWalletImportEnabled())
    return result;

  result.reserve(server_credit_card_cloud_token_data_.size());
  for (const auto& data : server_credit_card_cloud_token_data_)
    result.push_back(data.get());
  return result;
}

std::vector<AutofillOfferData*> PersonalDataManager::GetAutofillOffers() const {
  if (!IsAutofillWalletImportEnabled() || !IsAutofillPaymentMethodsEnabled()) {
    return {};
  }

  std::vector<AutofillOfferData*> result;
  result.reserve(autofill_offer_data_.size());
  for (const auto& data : autofill_offer_data_)
    result.push_back(data.get());
  return result;
}

std::vector<const AutofillOfferData*>
PersonalDataManager::GetActiveAutofillPromoCodeOffersForOrigin(
    GURL origin) const {
  if (!IsAutofillWalletImportEnabled() || !IsAutofillPaymentMethodsEnabled()) {
    return {};
  }

  std::vector<const AutofillOfferData*> promo_code_offers_for_origin;
  base::ranges::for_each(
      autofill_offer_data_,
      [&](const std::unique_ptr<AutofillOfferData>& autofill_offer_data) {
        if (autofill_offer_data.get()->IsPromoCodeOffer() &&
            autofill_offer_data.get()->IsActiveAndEligibleForOrigin(origin)) {
          promo_code_offers_for_origin.push_back(autofill_offer_data.get());
        }
      });
  return promo_code_offers_for_origin;
}

GURL PersonalDataManager::GetCardArtURL(const CreditCard& credit_card) const {
  if (credit_card.record_type() == CreditCard::RecordType::kMaskedServerCard) {
    return credit_card.card_art_url();
  }

  if (credit_card.record_type() == CreditCard::RecordType::kLocalCard) {
    const CreditCard* server_duplicate_card =
        GetServerCardForLocalCard(&credit_card);
    if (server_duplicate_card) {
      return server_duplicate_card->card_art_url();
    }
  }

  return GURL();
}

gfx::Image* PersonalDataManager::GetCreditCardArtImageForUrl(
    const GURL& card_art_url) const {
  if (!card_art_url.is_valid()) {
    return nullptr;
  }

  gfx::Image* cached_image = GetCachedCardArtImageForUrl(card_art_url);
  if (cached_image)
    return cached_image;

  FetchImagesForURLs(base::make_span(&card_art_url, 1u));
  return nullptr;
}

gfx::Image* PersonalDataManager::GetCachedCardArtImageForUrl(
    const GURL& card_art_url) const {
  if (!IsAutofillWalletImportEnabled())
    return nullptr;

  if (!card_art_url.is_valid())
    return nullptr;

  auto images_iterator = credit_card_art_images_.find(card_art_url);

  // If the cache contains the image, return it.
  if (images_iterator != credit_card_art_images_.end()) {
    gfx::Image* image = images_iterator->second.get();
    if (!image->IsEmpty())
      return image;
  }

  // The cache does not contain the image, return nullptr.
  return nullptr;
}

std::vector<VirtualCardUsageData*>
PersonalDataManager::GetVirtualCardUsageData() const {
  if (!IsAutofillWalletImportEnabled() || !IsAutofillPaymentMethodsEnabled()) {
    return {};
  }

  std::vector<VirtualCardUsageData*> result;
  result.reserve(autofill_virtual_card_usage_data_.size());
  for (const auto& data : autofill_virtual_card_usage_data_) {
    result.push_back(data.get());
  }
  return result;
}

void PersonalDataManager::Refresh() {
  LoadProfiles();
  LoadCreditCards();
  LoadCreditCardCloudTokenData();
  LoadIbans();
  LoadPaymentsCustomerData();
  LoadAutofillOffers();
  LoadVirtualCardUsageData();
}

std::vector<AutofillProfile*> PersonalDataManager::GetProfilesToSuggest()
    const {
  return IsAutofillProfileEnabled()
             ? GetProfiles(ProfileOrder::kHighestFrecencyDesc)
             : std::vector<AutofillProfile*>{};
}

std::vector<AutofillProfile*> PersonalDataManager::GetProfilesForSettings()
    const {
  return GetProfiles(ProfileOrder::kMostRecentlyModifiedDesc);
}

const std::vector<CreditCard*> PersonalDataManager::GetCreditCardsToSuggest()
    const {
  if (!IsAutofillPaymentMethodsEnabled()) {
    return std::vector<CreditCard*>{};
  }

  std::vector<CreditCard*> credit_cards;
  if (ShouldSuggestServerCards()) {
    credit_cards = GetCreditCards();
  } else {
    credit_cards = GetLocalCreditCards();
  }

  std::list<CreditCard*> cards_to_dedupe(credit_cards.begin(),
                                         credit_cards.end());

  DedupeCreditCardToSuggest(&cards_to_dedupe);

  std::vector<CreditCard*> cards_to_suggest(
      std::make_move_iterator(std::begin(cards_to_dedupe)),
      std::make_move_iterator(std::end(cards_to_dedupe)));

  // Rank the cards by ranking score (see AutofillDataModel for details). All
  // expired cards should be suggested last, also by ranking score.
  base::Time comparison_time = AutofillClock::Now();
  if (cards_to_suggest.size() > 1) {
    std::sort(cards_to_suggest.begin(), cards_to_suggest.end(),
              [comparison_time](const CreditCard* a, const CreditCard* b) {
                const bool a_is_expired = a->IsExpired(comparison_time);
                if (a_is_expired != b->IsExpired(comparison_time)) {
                  return !a_is_expired;
                }

                return a->HasGreaterRankingThan(b, comparison_time);
              });
  }

  return cards_to_suggest;
}

bool PersonalDataManager::IsAutofillEnabled() const {
  return IsAutofillProfileEnabled() || IsAutofillPaymentMethodsEnabled();
}

bool PersonalDataManager::IsAutofillProfileEnabled() const {
  return prefs::IsAutofillProfileEnabled(pref_service_);
}

bool PersonalDataManager::IsAutofillPaymentMethodsEnabled() const {
  return prefs::IsAutofillPaymentMethodsEnabled(pref_service_);
}

bool PersonalDataManager::IsAutofillHasSeenIbanPrefEnabled() const {
  return prefs::HasSeenIban(pref_service_);
}

void PersonalDataManager::SetAutofillHasSeenIban() {
  prefs::SetAutofillHasSeenIban(pref_service_);
}

bool PersonalDataManager::IsAutofillWalletImportEnabled() const {
  if (is_syncing_for_test_) {
    return true;
  }

  if (!sync_service_) {
    // Without `sync_service_`, namely in off-the-record profiles, wallet import
    // is effectively disabled.
    return false;
  }

  return sync_service_->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPayments);
}

bool PersonalDataManager::ShouldSuggestServerCards() const {
  if (!IsAutofillWalletImportEnabled())
    return false;

  if (is_syncing_for_test_)
    return true;

  CHECK(sync_service_);

  // Check if the user is in sync transport mode for wallet data.
  // TODO(crbug.com/1462552): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  if (!sync_service_->IsSyncFeatureEnabled()) {
    // For SyncTransport, only show server cards if the user has opted in to
    // seeing them in the dropdown.
    if (!prefs::IsUserOptedInWalletSyncTransport(
            pref_service_, sync_service_->GetAccountInfo().account_id)) {
      return false;
    }
  }

  // Server cards should be suggested if the sync service is active.
  return sync_service_->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA);
}

std::string PersonalDataManager::CountryCodeForCurrentTimezone() const {
  return base::CountryCodeForCurrentTimezone();
}

void PersonalDataManager::SetPrefService(PrefService* pref_service) {
  profile_enabled_pref_ = std::make_unique<BooleanPrefMember>();
  credit_card_enabled_pref_ = std::make_unique<BooleanPrefMember>();
  pref_service_ = pref_service;
  // |pref_service_| can be nullptr in tests. Using base::Unretained(this) is
  // safe because observer instances are destroyed once |this| is destroyed.
  if (pref_service_) {
    credit_card_enabled_pref_->Init(
        prefs::kAutofillCreditCardEnabled, pref_service_,
        base::BindRepeating(&PersonalDataManager::EnableAutofillPrefChanged,
                            base::Unretained(this)));
    profile_enabled_pref_->Init(
        prefs::kAutofillProfileEnabled, pref_service_,
        base::BindRepeating(&PersonalDataManager::EnableAutofillPrefChanged,
                            base::Unretained(this)));
  }
}

void PersonalDataManager::FetchImagesForURLs(
    base::span<const GURL> updated_urls) const {
  if (!image_fetcher_)
    return;

  image_fetcher_->FetchImagesForURLs(
      updated_urls, base::BindOnce(&PersonalDataManager::OnCardArtImagesFetched,
                                   weak_factory_.GetMutableWeakPtr()));
}

const std::vector<std::unique_ptr<AutofillProfile>>&
PersonalDataManager::GetProfileStorage(AutofillProfile::Source source) const {
  switch (source) {
    case AutofillProfile::Source::kLocalOrSyncable:
      return synced_local_profiles_;
    case AutofillProfile::Source::kAccount:
      return account_profiles_;
  }
  NOTREACHED();
}

const std::string& PersonalDataManager::GetDefaultCountryCodeForNewAddress()
    const {
  if (default_country_code_.empty())
    default_country_code_ = MostCommonCountryCodeFromProfiles();

  // Failing that, use the country code determined for experiment groups.
  if (default_country_code_.empty())
    default_country_code_ = GetCountryCodeForExperimentGroup();

  return default_country_code_;
}

const std::string& PersonalDataManager::GetCountryCodeForExperimentGroup()
    const {
  // Set to |variations_country_code_| if it exists.
  if (experiment_country_code_.empty())
    experiment_country_code_ = variations_country_code_;

  // Failing that, guess based on system timezone.
  if (experiment_country_code_.empty())
    experiment_country_code_ = CountryCodeForCurrentTimezone();

  // Failing that, guess based on locale. This returns "US" if there is no good
  // guess.
  if (experiment_country_code_.empty()) {
    experiment_country_code_ =
        AutofillCountry::CountryCodeForLocale(app_locale());
  }

  return experiment_country_code_;
}

// The priority ranking for deduping a duplicate card is:
// 1. RecordType::kFullServerCard
// 2. RecordType::kLocalCard
// 3. RecordType::kMaskedServerCard
// Note: 2 & 3 are swapped if experiment
// kAutofillSuggestServerCardInsteadOfLocalCard is enabled.
// static
void PersonalDataManager::DedupeCreditCardToSuggest(
    std::list<CreditCard*>* cards_to_suggest) {
  for (auto outer_it = cards_to_suggest->begin();
       outer_it != cards_to_suggest->end(); ++outer_it) {
    for (auto inner_it = cards_to_suggest->begin();
         inner_it != cards_to_suggest->end();) {
      auto inner_it_copy = inner_it++;
      if (outer_it == inner_it_copy) {
        continue;
      }
      // Check if the cards are local or server duplicate of each other. If yes,
      // then check if we can dedupe/erase the duplicate card.
      if ((*inner_it_copy)->IsLocalOrServerDuplicateOf(**outer_it) &&
          ShouldDedupeDuplicateCard(*outer_it, *inner_it_copy)) {
        cards_to_suggest->erase(inner_it_copy);
      }
    }
  }
}

bool PersonalDataManager::IsCardPresentAsBothLocalAndServerCards(
    const CreditCard& credit_card) {
  for (CreditCard* card_from_list : GetCreditCards()) {
    if (credit_card.IsLocalOrServerDuplicateOf(*card_from_list)) {
      return true;
    }
  }
  return false;
}

const CreditCard* PersonalDataManager::GetServerCardForLocalCard(
    const CreditCard* local_card) const {
  DCHECK(local_card);
  if (local_card->record_type() != CreditCard::RecordType::kLocalCard) {
    return nullptr;
  }

  std::vector<CreditCard*> server_cards = GetServerCreditCards();
  auto it =
      base::ranges::find_if(server_cards, [&](const CreditCard* server_card) {
        return local_card->IsLocalOrServerDuplicateOf(*server_card);
      });

  if (it != server_cards.end()) {
    return *it;
  }

  return nullptr;
}

bool PersonalDataManager::IsProfileMigrationBlocked(
    const std::string& guid) const {
  AutofillProfile* profile = GetProfileByGUID(guid);
  DCHECK(profile == nullptr ||
         profile->source() == AutofillProfile::Source::kLocalOrSyncable);
  if (!GetProfileMigrationStrikeDatabase()) {
    return false;
  }
  return GetProfileMigrationStrikeDatabase()->ShouldBlockFeature(guid);
}

void PersonalDataManager::AddStrikeToBlockProfileMigration(
    const std::string& guid) {
  if (!GetProfileMigrationStrikeDatabase()) {
    return;
  }
  GetProfileMigrationStrikeDatabase()->AddStrike(guid);
}

void PersonalDataManager::AddMaxStrikesToBlockProfileMigration(
    const std::string& guid) {
  if (AutofillProfileMigrationStrikeDatabase* db =
          GetProfileMigrationStrikeDatabase()) {
    db->AddStrikes(db->GetMaxStrikesLimit() - db->GetStrikes(guid), guid);
  }
}

void PersonalDataManager::RemoveStrikesToBlockProfileMigration(
    const std::string& guid) {
  if (!GetProfileMigrationStrikeDatabase()) {
    return;
  }
  GetProfileMigrationStrikeDatabase()->ClearStrikes(guid);
}

bool PersonalDataManager::IsNewProfileImportBlockedForDomain(
    const GURL& url) const {
  if (!GetProfileSaveStrikeDatabase() || !url.is_valid() || !url.has_host()) {
    return false;
  }

  return GetProfileSaveStrikeDatabase()->ShouldBlockFeature(url.host());
}

void PersonalDataManager::AddStrikeToBlockNewProfileImportForDomain(
    const GURL& url) {
  if (!GetProfileSaveStrikeDatabase() || !url.is_valid() || !url.has_host()) {
    return;
  }
  GetProfileSaveStrikeDatabase()->AddStrike(url.host());
}

void PersonalDataManager::RemoveStrikesToBlockNewProfileImportForDomain(
    const GURL& url) {
  if (!GetProfileSaveStrikeDatabase() || !url.is_valid() || !url.has_host()) {
    return;
  }
  GetProfileSaveStrikeDatabase()->ClearStrikes(url.host());
}

bool PersonalDataManager::IsProfileUpdateBlocked(
    const std::string& guid) const {
  if (!GetProfileUpdateStrikeDatabase()) {
    return false;
  }
  return GetProfileUpdateStrikeDatabase()->ShouldBlockFeature(guid);
}

void PersonalDataManager::AddStrikeToBlockProfileUpdate(
    const std::string& guid) {
  if (!GetProfileUpdateStrikeDatabase())
    return;
  GetProfileUpdateStrikeDatabase()->AddStrike(guid);
}

void PersonalDataManager::RemoveStrikesToBlockProfileUpdate(
    const std::string& guid) {
  if (!GetProfileUpdateStrikeDatabase()) {
    return;
  }
  GetProfileUpdateStrikeDatabase()->ClearStrikes(guid);
}

bool PersonalDataManager::IsSyncFeatureEnabledForAutofill() const {
  // TODO(crbug.com/1462552): Remove this method in favor of
  // `IsUserSelectableTypeEnabled` once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  return sync_service_ != nullptr && sync_service_->IsSyncFeatureEnabled() &&
         IsUserSelectableTypeEnabled(syncer::UserSelectableType::kAutofill);
}

bool PersonalDataManager::IsUserSelectableTypeEnabled(
    syncer::UserSelectableType type) const {
  return sync_service_ != nullptr &&
         sync_service_->GetUserSettings()->GetSelectedTypes().Has(type);
}

void PersonalDataManager::SetPaymentMethodsMandatoryReauthEnabled(
    bool enabled) {
  prefs::SetPaymentMethodsMandatoryReauthEnabled(pref_service_, enabled);
}

bool PersonalDataManager::IsPaymentMethodsMandatoryReauthEnabled() {
  return prefs::IsPaymentMethodsMandatoryReauthEnabled(pref_service_);
}

bool PersonalDataManager::ShouldShowPaymentMethodsMandatoryReauthPromo() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnablePaymentsMandatoryReauth)) {
    return false;
  }

  // There is no need to show the promo if the feature is already enabled.
  if (prefs::IsPaymentMethodsMandatoryReauthEnabled(pref_service_)) {
#if BUILDFLAG(IS_ANDROID)
    // The mandatory reauth feature is always enabled on automotive, there
    // is/was no opt-in. As such, there is no need to log anything here on
    // automotive.
    if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
      LogMandatoryReauthOfferOptInDecision(
          MandatoryReauthOfferOptInDecision::kAlreadyOptedIn);
    }
#else
    LogMandatoryReauthOfferOptInDecision(
        MandatoryReauthOfferOptInDecision::kAlreadyOptedIn);
#endif  // BUILDFLAG(IS_ANDROID)
    return false;
  }

  // If the user has explicitly opted out of this feature previously, then we
  // should not show the opt-in promo.
  if (prefs::IsPaymentMethodsMandatoryReauthSetExplicitly(pref_service_)) {
    LogMandatoryReauthOfferOptInDecision(
        MandatoryReauthOfferOptInDecision::kAlreadyOptedOut);
    return false;
  }

  // We should only show the opt-in promo if we have not reached the maximum
  // number of shows for the promo.
  bool allowed_by_strike_database =
      prefs::IsPaymentMethodsMandatoryReauthPromoShownCounterBelowMaxCap(
          pref_service_);
  if (!allowed_by_strike_database) {
    LogMandatoryReauthOfferOptInDecision(
        MandatoryReauthOfferOptInDecision::kBlockedByStrikeDatabase);
  }
  return allowed_by_strike_database;
#else
  return false;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
}

void PersonalDataManager::
    IncrementPaymentMethodsMandatoryReauthPromoShownCounter() {
  prefs::IncrementPaymentMethodsMandatoryReauthPromoShownCounter(pref_service_);
}

bool PersonalDataManager::IsPaymentCvcStorageEnabled() {
  return base::FeatureList::IsEnabled(
             features::kAutofillEnableCvcStorageAndFilling) &&
         prefs::IsPaymentCvcStorageEnabled(pref_service_);
}

AutofillImageFetcherBase* PersonalDataManager::GetImageFetcher() const {
  return image_fetcher_;
}

AutofillProfileMigrationStrikeDatabase*
PersonalDataManager::GetProfileMigrationStrikeDatabase() {
  return const_cast<AutofillProfileMigrationStrikeDatabase*>(
      std::as_const(*this).GetProfileMigrationStrikeDatabase());
}

const AutofillProfileMigrationStrikeDatabase*
PersonalDataManager::GetProfileMigrationStrikeDatabase() const {
  return profile_migration_strike_database_.get();
}

AutofillProfileSaveStrikeDatabase*
PersonalDataManager::GetProfileSaveStrikeDatabase() {
  return const_cast<AutofillProfileSaveStrikeDatabase*>(
      std::as_const(*this).GetProfileSaveStrikeDatabase());
}

const AutofillProfileSaveStrikeDatabase*
PersonalDataManager::GetProfileSaveStrikeDatabase() const {
  return profile_save_strike_database_.get();
}

AutofillProfileUpdateStrikeDatabase*
PersonalDataManager::GetProfileUpdateStrikeDatabase() {
  return const_cast<AutofillProfileUpdateStrikeDatabase*>(
      std::as_const(*this).GetProfileUpdateStrikeDatabase());
}

const AutofillProfileUpdateStrikeDatabase*
PersonalDataManager::GetProfileUpdateStrikeDatabase() const {
  return profile_update_strike_database_.get();
}

void PersonalDataManager::SetCreditCards(
    std::vector<CreditCard>* credit_cards) {
  // Remove empty credit cards from input.
  base::EraseIf(*credit_cards, [this](const CreditCard& credit_card) {
    return credit_card.IsEmpty(app_locale_);
  });

  if (!database_helper_->GetLocalDatabase())
    return;

  // Any credit cards that are not in the new credit card list should be
  // removed.
  for (const auto& card : local_credit_cards_) {
    if (!FindByGUID(*credit_cards, card->guid()))
      database_helper_->GetLocalDatabase()->RemoveCreditCard(card->guid());
  }

  // Update the web database with the existing credit cards.
  for (const CreditCard& card : *credit_cards) {
    if (FindByGUID(local_credit_cards_, card.guid()))
      database_helper_->GetLocalDatabase()->UpdateCreditCard(card);
  }

  // Add the new credit cards to the web database.  Don't add a duplicate.
  for (const CreditCard& card : *credit_cards) {
    if (!FindByGUID(local_credit_cards_, card.guid()) &&
        !FindByContents(local_credit_cards_, card))
      database_helper_->GetLocalDatabase()->AddCreditCard(card);
  }

  // Copy in the new credit cards.
  local_credit_cards_.clear();
  for (const CreditCard& card : *credit_cards)
    local_credit_cards_.push_back(std::make_unique<CreditCard>(card));

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::LoadProfiles() {
  if (!database_helper_->GetLocalDatabase()) {
    NOTREACHED();
    return;
  }

  CancelPendingLocalQuery(&pending_synced_local_profiles_query_);
  CancelPendingLocalQuery(&pending_account_profiles_query_);
  CancelPendingServerQuery(&pending_creditcard_billing_addresses_query_);

  pending_synced_local_profiles_query_ =
      database_helper_->GetLocalDatabase()->GetAutofillProfiles(
          AutofillProfile::Source::kLocalOrSyncable, this);
  pending_account_profiles_query_ =
      database_helper_->GetLocalDatabase()->GetAutofillProfiles(
          AutofillProfile::Source::kAccount, this);
  if (database_helper_->GetServerDatabase()) {
    pending_creditcard_billing_addresses_query_ =
        database_helper_->GetServerDatabase()->GetServerProfiles(this);
  }
}

void PersonalDataManager::LoadCreditCards() {
  if (!database_helper_->GetLocalDatabase()) {
    NOTREACHED();
    return;
  }

  CancelPendingLocalQuery(&pending_creditcards_query_);
  CancelPendingServerQuery(&pending_server_creditcards_query_);

  pending_creditcards_query_ =
      database_helper_->GetLocalDatabase()->GetCreditCards(this);
  if (database_helper_->GetServerDatabase()) {
    pending_server_creditcards_query_ =
        database_helper_->GetServerDatabase()->GetServerCreditCards(this);
  }
}

void PersonalDataManager::LoadCreditCardCloudTokenData() {
  if (!database_helper_->GetServerDatabase())
    return;

  CancelPendingServerQuery(&pending_server_creditcard_cloud_token_data_query_);

  pending_server_creditcard_cloud_token_data_query_ =
      database_helper_->GetServerDatabase()->GetCreditCardCloudTokenData(this);
}

void PersonalDataManager::LoadIbans() {
  if (!database_helper_->GetLocalDatabase()) {
    NOTREACHED();
    return;
  }

  CancelPendingLocalQuery(&pending_local_ibans_query_);
  CancelPendingServerQuery(&pending_server_ibans_query_);

  pending_local_ibans_query_ =
      database_helper_->GetLocalDatabase()->GetLocalIbans(this);
  if (database_helper_->GetServerDatabase()) {
    pending_server_ibans_query_ =
        database_helper_->GetServerDatabase()->GetServerIbans(this);
  }
}

void PersonalDataManager::LoadAutofillOffers() {
  if (!database_helper_->GetServerDatabase())
    return;

  CancelPendingServerQuery(&pending_offer_data_query_);

  pending_offer_data_query_ =
      database_helper_->GetServerDatabase()->GetAutofillOffers(this);
}

void PersonalDataManager::LoadVirtualCardUsageData() {
  if (!database_helper_->GetServerDatabase()) {
    return;
  }

  CancelPendingServerQuery(&pending_virtual_card_usage_data_query_);

  pending_virtual_card_usage_data_query_ =
      database_helper_->GetServerDatabase()->GetVirtualCardUsageData(this);
}

void PersonalDataManager::CancelPendingLocalQuery(
    WebDataServiceBase::Handle* handle) {
  if (*handle) {
    if (!database_helper_->GetLocalDatabase()) {
      NOTREACHED();
      return;
    }
    database_helper_->GetLocalDatabase()->CancelRequest(*handle);
  }
  *handle = 0;
}

void PersonalDataManager::CancelPendingServerQuery(
    WebDataServiceBase::Handle* handle) {
  if (*handle) {
    if (!database_helper_->GetServerDatabase()) {
      NOTREACHED();
      return;
    }
    database_helper_->GetServerDatabase()->CancelRequest(*handle);
  }
  *handle = 0;
}

void PersonalDataManager::CancelPendingServerQueries() {
  CancelPendingServerQuery(&pending_creditcard_billing_addresses_query_);
  CancelPendingServerQuery(&pending_server_creditcards_query_);
  CancelPendingServerQuery(&pending_customer_data_query_);
  CancelPendingServerQuery(&pending_server_creditcard_cloud_token_data_query_);
  CancelPendingServerQuery(&pending_server_ibans_query_);
  CancelPendingServerQuery(&pending_offer_data_query_);
  CancelPendingServerQuery(&pending_virtual_card_usage_data_query_);
}

void PersonalDataManager::LoadPaymentsCustomerData() {
  if (!database_helper_->GetServerDatabase())
    return;

  CancelPendingServerQuery(&pending_customer_data_query_);

  pending_customer_data_query_ =
      database_helper_->GetServerDatabase()->GetPaymentsCustomerData(this);
}

std::string PersonalDataManager::OnAcceptedLocalCreditCardSave(
    const CreditCard& imported_card) {
  DCHECK(!imported_card.number().empty());
  return SaveImportedCreditCard(imported_card);
}

std::string PersonalDataManager::OnAcceptedLocalIbanSave(Iban imported_iban) {
  DCHECK(!imported_iban.value().empty());
  // If an existing IBAN is found, call `UpdateIban()`, otherwise, `AddIban()`.
  // `local_ibans_` will be in sync with the local web database as of
  // `Refresh()` which will be called by both `UpdateIban()` and `AddIban()`.
  for (auto& iban : local_ibans_) {
    if (iban->value().compare(imported_iban.value()) == 0) {
      // Set the GUID of the IBAN to the one that matches it in
      // `local_ibans_` so that UpdateIban() will be able to update the
      // specific IBAN.
      imported_iban.set_identifier(Iban::Guid(iban->guid()));
      return UpdateIban(imported_iban);
    }
  }
  return AddIban(std::move(imported_iban));
}

void PersonalDataManager::SetSyncService(syncer::SyncService* sync_service) {
  CHECK(!sync_service_);

  sync_service_ = sync_service;
  if (sync_service_) {
    sync_service_->AddObserver(this);
  }

  // Re-mask all server cards if the upload state is not active.
  const bool is_upload_not_active =
      syncer::GetUploadToGoogleState(sync_service_,
                                     syncer::ModelType::AUTOFILL_WALLET_DATA) ==
      syncer::UploadState::NOT_ACTIVE;
  if (is_upload_not_active) {
    ResetFullServerCards();
  }

  OnStateChanged(sync_service_);
}

std::string PersonalDataManager::SaveImportedCreditCard(
    const CreditCard& imported_card) {
  // Set to true if |imported_card| is merged into the credit card list.
  bool merged = false;

  std::string guid = imported_card.guid();
  std::vector<CreditCard> credit_cards;
  for (auto& card : local_credit_cards_) {
    // If |imported_card| has not yet been merged, check whether it should be
    // with the current |card|.
    if (!merged && card->UpdateFromImportedCard(imported_card, app_locale_)) {
      guid = card->guid();
      merged = true;
    }

    credit_cards.push_back(*card);
  }

  if (!merged)
    credit_cards.push_back(imported_card);

  SetCreditCards(&credit_cards);

  // After a card is saved locally, notifies the observers.
  OnCreditCardSaved(/*is_local_card=*/true);

  return guid;
}

void PersonalDataManager::LogStoredDataMetrics() const {
  if (has_logged_stored_data_metrics_) {
    return;
  }
  // Only log this info once per Chrome user profile load.
  has_logged_stored_data_metrics_ = true;

  const std::vector<AutofillProfile*> profiles = GetProfiles();
  autofill_metrics::LogStoredProfileMetrics(profiles);
  if (base::FeatureList::IsEnabled(features::kAutofillAccountProfileStorage)) {
    autofill_metrics::LogLocalProfileSupersetMetrics(std::move(profiles),
                                                     app_locale_);
  }

  AutofillMetrics::LogStoredCreditCardMetrics(
      local_credit_cards_, server_credit_cards_,
      GetServerCardWithArtImageCount(), kDisusedDataModelTimeDelta);
  autofill_metrics::LogStoredIbanMetrics(local_ibans_,
                                         kDisusedDataModelTimeDelta);
  autofill_metrics::LogStoredOfferMetrics(autofill_offer_data_);
  autofill_metrics::LogStoredVirtualCardUsageCount(
      autofill_virtual_card_usage_data_.size());
}

std::string PersonalDataManager::MostCommonCountryCodeFromProfiles() const {
  if (!IsAutofillEnabled())
    return std::string();

  // Count up country codes from existing profiles.
  std::map<std::string, int> votes;
  const std::vector<AutofillProfile*>& profiles = GetProfiles();
  const std::vector<std::string>& country_codes =
      CountryDataMap::GetInstance()->country_codes();
  for (auto* profile : profiles) {
    std::string country_code = base::ToUpperASCII(
        base::UTF16ToASCII(profile->GetRawInfo(ADDRESS_HOME_COUNTRY)));
    if (base::Contains(country_codes, country_code)) {
      votes[country_code]++;
    }
  }

  // Take the most common country code.
  if (!votes.empty()) {
    auto iter = std::max_element(votes.begin(), votes.end(), CompareVotes);
    return iter->first;
  }

  return std::string();
}

void PersonalDataManager::EnableAutofillPrefChanged() {
  default_country_code_.clear();

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

bool PersonalDataManager::IsKnownCard(const CreditCard& credit_card) const {
  const auto stripped_pan = CreditCard::StripSeparators(credit_card.number());
  for (const auto& card : local_credit_cards_) {
    if (stripped_pan == CreditCard::StripSeparators(card->number()))
      return true;
  }

  const auto masked_info = credit_card.NetworkAndLastFourDigits();
  for (const auto& card : server_credit_cards_) {
    switch (card->record_type()) {
      case CreditCard::RecordType::kFullServerCard:
        if (stripped_pan == CreditCard::StripSeparators(card->number()))
          return true;
        break;
      case CreditCard::RecordType::kMaskedServerCard:
        if (masked_info == card->NetworkAndLastFourDigits())
          return true;
        break;
      default:
        NOTREACHED();
    }
  }

  return false;
}

bool PersonalDataManager::IsServerCard(const CreditCard* credit_card) const {
  // Check whether the current card itself is a server card.
  if (credit_card->record_type() != CreditCard::RecordType::kLocalCard) {
    return true;
  }

  std::vector<CreditCard*> server_credit_cards = GetServerCreditCards();
  // Check whether the current card is already uploaded.
  for (const CreditCard* server_card : server_credit_cards) {
    if (credit_card->MatchingCardDetails(*server_card)) {
      return true;
    }
  }
  return false;
}

bool PersonalDataManager::ShouldShowCardsFromAccountOption() const {
// The feature is only for Linux, Windows, Mac, and Fuchsia.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
  // This option should only be shown for users that have not enabled the Sync
  // Feature and that have server credit cards available.
  // TODO(crbug.com/1462552): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  if (!sync_service_ || sync_service_->IsSyncFeatureEnabled() ||
      GetServerCreditCards().empty()) {
    return false;
  }

  bool is_opted_in = prefs::IsUserOptedInWalletSyncTransport(
      pref_service_, sync_service_->GetAccountInfo().account_id);

  // The option should only be shown if the user has not already opted-in.
  return !is_opted_in;
#else
  return false;
#endif  // #if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) ||
        // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
}

void PersonalDataManager::OnUserAcceptedCardsFromAccountOption() {
  DCHECK(IsPaymentsWalletSyncTransportEnabled());
  prefs::SetUserOptedInWalletSyncTransport(
      pref_service_, sync_service_->GetAccountInfo().account_id,
      /*opted_in=*/true);
}

void PersonalDataManager::OnAutofillProfileChanged(
    const AutofillProfileChange& change) {
  const std::string& guid = change.key();
  const AutofillProfile& profile = change.data_model();
  DCHECK(guid == profile.guid());
  // Happens only in tests.
  if (!ProfileChangesAreOngoing(guid)) {
    DVLOG(1) << "Received an unexpected response from database.";
    return;
  }

  std::vector<std::unique_ptr<AutofillProfile>>& profiles =
      GetProfileStorage(profile.source());
  const AutofillProfile* existing_profile = GetProfileByGUID(guid);
  switch (change.type()) {
    case AutofillProfileChange::ADD:
      if (!existing_profile && !FindByContents(profiles, profile)) {
        profiles.push_back(std::make_unique<AutofillProfile>(profile));
      }
      break;
    case AutofillProfileChange::UPDATE:
      if (existing_profile &&
          !existing_profile->EqualsForUpdatePurposes(profile)) {
        profiles.erase(FindElementByGUID(profiles, guid));
        profiles.push_back(std::make_unique<AutofillProfile>(profile));
      }
      break;
    case AutofillProfileChange::REMOVE:
      if (existing_profile) {
        profiles.erase(FindElementByGUID(profiles, guid));
      }
      break;
  }

  OnProfileChangeDone(guid);
}

void PersonalDataManager::OnCardArtImagesFetched(
    const std::vector<std::unique_ptr<CreditCardArtImage>>& art_images) {
  for (auto& art_image : art_images) {
    if (!art_image->card_art_image.IsEmpty()) {
      credit_card_art_images_[art_image->card_art_url] =
          std::make_unique<gfx::Image>(art_image->card_art_image);
    }
  }
}

void PersonalDataManager::LogServerCardLinkClicked() const {
  AutofillMetrics::LogServerCardLinkClicked(GetPaymentsSigninStateForMetrics());
}

void PersonalDataManager::OnUserAcceptedUpstreamOffer() {
  // If the user is in sync transport mode for Wallet, record an opt-in.
  if (IsPaymentsWalletSyncTransportEnabled()) {
    prefs::SetUserOptedInWalletSyncTransport(
        pref_service_, sync_service_->GetAccountInfo().account_id,
        /*opted_in=*/true);
  }
}

void PersonalDataManager::NotifyPersonalDataObserver() {
  // The PDM's state is inconsistent with the database in two cases:
  // - When profile modifications are still pending: ProfileChangesAreOngoing().
  // - When reads are still pending.
  bool pending_changes = ProfileChangesAreOngoing() || HasPendingQueries();
  for (PersonalDataManagerObserver& observer : observers_) {
    observer.OnPersonalDataChanged();
  }
  if (!pending_changes) {
    // Call `OnPersonalDataFinishedProfileTasks()` in a separate loop as
    // the observers might have removed themselves in OnPersonalDataChanged
    for (PersonalDataManagerObserver& observer : observers_) {
      observer.OnPersonalDataFinishedProfileTasks();
    }
  }
}

void PersonalDataManager::OnCreditCardSaved(bool is_local_card) {}

void PersonalDataManager::UpdateProfileInDB(const AutofillProfile& profile) {
  if (!ProfileChangesAreOngoing(profile.guid())) {
    const AutofillProfile* existing_profile = GetProfileByGUID(profile.guid());
    if (!existing_profile ||
        existing_profile->EqualsForUpdatePurposes(profile)) {
      NotifyPersonalDataObserver();
      return;
    }
  }

  ongoing_profile_changes_[profile.guid()].emplace_back(
      AutofillProfileChange(AutofillProfileChange::UPDATE, profile.guid(),
                            profile),
      /*is_ongoing=*/false);
  HandleNextProfileChange(profile.guid());
}

void PersonalDataManager::RemoveProfileFromDB(const std::string& guid) {
  // Find the profile to remove.
  // TODO(crbug.com/1420547): This shouldn't be necessary. Providing a `guid`
  // to the `AutofillProfileChange()` should suffice for removals.
  const AutofillProfile* profile =
      ProfileChangesAreOngoing(guid)
          ? &ongoing_profile_changes_[guid].back().first.data_model()
          : GetProfileByGUID(guid);
  if (!profile) {
    NotifyPersonalDataObserver();
    return;
  }

  ongoing_profile_changes_[guid].emplace_back(
      AutofillProfileChange(AutofillProfileChange::REMOVE, guid, *profile),
      /*is_ongoing=*/false);
  HandleNextProfileChange(guid);
}

void PersonalDataManager::HandleNextProfileChange(const std::string& guid) {
  if (!ProfileChangesAreOngoing(guid))
    return;

  auto& [change, is_ongoing] = ongoing_profile_changes_[guid].front();
  if (is_ongoing) {
    return;
  }

  const AutofillProfile* existing_profile = GetProfileByGUID(guid);
  const AutofillProfile& profile = change.data_model();
  DCHECK(guid == profile.guid());
  scoped_refptr<AutofillWebDataService> webdata_service =
      database_helper_->GetLocalDatabase();

  switch (change.type()) {
    case AutofillProfileChange::REMOVE: {
      if (!existing_profile) {
        OnProfileChangeDone(guid);
        return;
      }
      webdata_service->RemoveAutofillProfile(guid, existing_profile->source());
      break;
    }
    case AutofillProfileChange::ADD: {
      if (existing_profile ||
          FindByContents(GetProfileStorage(profile.source()), profile)) {
        OnProfileChangeDone(guid);
        return;
      }
      webdata_service->AddAutofillProfile(profile);
      break;
    }
    case AutofillProfileChange::UPDATE: {
      if (!existing_profile ||
          existing_profile->EqualsForUpdatePurposes(profile)) {
        OnProfileChangeDone(guid);
        return;
      }
      // At this point, the `existing_profile` is consistent with
      // AutofillTable's state. Reset observations for all types that change due
      // to this update.
      AutofillProfile updated_profile = profile;
      updated_profile.token_quality().ResetObservationsForDifferingTokens(
          *existing_profile);
      // Unless only metadata has changed, which operator== ignores, update the
      // modification date. This happens e.g. when increasing the use count.
      if (*existing_profile != updated_profile) {
        updated_profile.set_modification_date(AutofillClock::Now());
      }
      webdata_service->UpdateAutofillProfile(updated_profile);
      break;
    }
  }
  is_ongoing = true;
}

bool PersonalDataManager::ProfileChangesAreOngoing(const std::string& guid) {
  return ongoing_profile_changes_.find(guid) !=
             ongoing_profile_changes_.end() &&
         !ongoing_profile_changes_[guid].empty();
}

bool PersonalDataManager::ProfileChangesAreOngoing() {
  for (const auto& [guid, change] : ongoing_profile_changes_) {
    if (ProfileChangesAreOngoing(guid)) {
      return true;
    }
  }
  return false;
}

void PersonalDataManager::OnProfileChangeDone(const std::string& guid) {
  ongoing_profile_changes_[guid].pop_front();
  NotifyPersonalDataObserver();
  HandleNextProfileChange(guid);
}

bool PersonalDataManager::HasPendingQueries() {
  return pending_synced_local_profiles_query_ != 0 ||
         pending_account_profiles_query_ != 0 ||
         pending_creditcards_query_ != 0 ||
         pending_creditcard_billing_addresses_query_ != 0 ||
         pending_server_creditcards_query_ != 0 ||
         pending_server_creditcard_cloud_token_data_query_ != 0 ||
         pending_customer_data_query_ != 0 || pending_offer_data_query_ != 0 ||
         pending_virtual_card_usage_data_query_ != 0;
}

scoped_refptr<AutofillWebDataService> PersonalDataManager::GetLocalDatabase() {
  DCHECK(database_helper_);
  return database_helper_->GetLocalDatabase();
}

void PersonalDataManager::OnServerCreditCardsRefreshed() {
  ProcessCardArtUrlChanges();
}

void PersonalDataManager::ProcessCardArtUrlChanges() {
  std::vector<GURL> updated_urls;
  for (auto& card : server_credit_cards_) {
    if (!card->card_art_url().is_valid())
      continue;

    // Try to find the old entry with the same url.
    auto it = credit_card_art_images_.find(card->card_art_url());
    // No existing entry found.
    if (it == credit_card_art_images_.end())
      updated_urls.emplace_back(card->card_art_url());
  }
  if (!updated_urls.empty())
    FetchImagesForURLs(updated_urls);
}

size_t PersonalDataManager::GetServerCardWithArtImageCount() const {
  return base::ranges::count_if(
      server_credit_cards_.begin(), server_credit_cards_.end(),
      [](const auto& card) { return card->card_art_url().is_valid(); });
}

}  // namespace autofill
