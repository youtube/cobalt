// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

#include <map>
#include <set>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/unified_consent/pref_names.h"

namespace unified_consent {

namespace {

class PrefBasedUrlKeyedDataCollectionConsentHelper
    : public UrlKeyedDataCollectionConsentHelper {
 public:
  explicit PrefBasedUrlKeyedDataCollectionConsentHelper(
      PrefService* pref_service);

  PrefBasedUrlKeyedDataCollectionConsentHelper(
      const PrefBasedUrlKeyedDataCollectionConsentHelper&) = delete;
  PrefBasedUrlKeyedDataCollectionConsentHelper& operator=(
      const PrefBasedUrlKeyedDataCollectionConsentHelper&) = delete;

  ~PrefBasedUrlKeyedDataCollectionConsentHelper() override = default;

  // UrlKeyedDataCollectionConsentHelper:
  State GetConsentState() override;

 private:
  void OnPrefChanged();
  raw_ptr<PrefService> pref_service_;  // weak (must outlive this)
  PrefChangeRegistrar pref_change_registrar_;
};

class SyncBasedUrlKeyedDataCollectionConsentHelper
    : public UrlKeyedDataCollectionConsentHelper,
      syncer::SyncServiceObserver {
 public:
  SyncBasedUrlKeyedDataCollectionConsentHelper(
      syncer::SyncService* sync_service,
      std::set<syncer::ModelType> sync_data_types);

  SyncBasedUrlKeyedDataCollectionConsentHelper(
      const SyncBasedUrlKeyedDataCollectionConsentHelper&) = delete;
  SyncBasedUrlKeyedDataCollectionConsentHelper& operator=(
      const SyncBasedUrlKeyedDataCollectionConsentHelper&) = delete;

  ~SyncBasedUrlKeyedDataCollectionConsentHelper() override;

  // UrlKeyedDataCollectionConsentHelper:
  State GetConsentState() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  void UpdateSyncDataTypeStates();

  raw_ptr<syncer::SyncService> sync_service_;
  std::map<syncer::ModelType, syncer::UploadState> sync_data_type_states_;
};

PrefBasedUrlKeyedDataCollectionConsentHelper::
    PrefBasedUrlKeyedDataCollectionConsentHelper(PrefService* pref_service)
    : pref_service_(pref_service) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      base::BindRepeating(
          &PrefBasedUrlKeyedDataCollectionConsentHelper::OnPrefChanged,
          base::Unretained(this)));
}

UrlKeyedDataCollectionConsentHelper::State
PrefBasedUrlKeyedDataCollectionConsentHelper::GetConsentState() {
  // There's no initializing state for pref-based helpers.
  return pref_service_->GetBoolean(
             prefs::kUrlKeyedAnonymizedDataCollectionEnabled)
             ? State::kEnabled
             : State::kDisabled;
}

void PrefBasedUrlKeyedDataCollectionConsentHelper::OnPrefChanged() {
  FireOnStateChanged();
}

SyncBasedUrlKeyedDataCollectionConsentHelper::
    SyncBasedUrlKeyedDataCollectionConsentHelper(
        syncer::SyncService* sync_service,
        std::set<syncer::ModelType> sync_data_types)
    : sync_service_(sync_service) {
  DCHECK(!sync_data_types.empty());

  for (const auto& sync_data_type : sync_data_types) {
    sync_data_type_states_[sync_data_type] = syncer::UploadState::NOT_ACTIVE;
  }
  UpdateSyncDataTypeStates();

  if (sync_service_)
    sync_service_->AddObserver(this);
}

SyncBasedUrlKeyedDataCollectionConsentHelper::
    ~SyncBasedUrlKeyedDataCollectionConsentHelper() {
  if (sync_service_)
    sync_service_->RemoveObserver(this);
}

UrlKeyedDataCollectionConsentHelper::State
SyncBasedUrlKeyedDataCollectionConsentHelper::GetConsentState() {
  // Any sync type that's NOT_ACTIVE makes the whole consent kDisabled.
  for (const auto& sync_data_type_states : sync_data_type_states_) {
    if (sync_data_type_states.second == syncer::UploadState::NOT_ACTIVE) {
      return State::kDisabled;
    }
  }

  // If no sync type is NOT_ACTIVE, any sync type still INITIALIZING makes the
  // whole consent kInitializing.
  for (const auto& sync_data_type_states : sync_data_type_states_) {
    if (sync_data_type_states.second == syncer::UploadState::INITIALIZING) {
      return State::kInitializing;
    }
  }

  DCHECK(base::ranges::all_of(sync_data_type_states_, [](auto& state) {
    return state.second == syncer::UploadState::ACTIVE;
  })) << "Nothing is NOT_ACTIVE or INITIALIZING, so all must be ACTIVE.";
  return State::kEnabled;
}

void SyncBasedUrlKeyedDataCollectionConsentHelper::OnStateChanged(
    syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);
  auto old_state = GetConsentState();
  UpdateSyncDataTypeStates();
  if (old_state != GetConsentState()) {
    FireOnStateChanged();
  }
}

void SyncBasedUrlKeyedDataCollectionConsentHelper::OnSyncShutdown(
    syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);
  sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;
}

void SyncBasedUrlKeyedDataCollectionConsentHelper::UpdateSyncDataTypeStates() {
  for (auto iter = sync_data_type_states_.begin();
       iter != sync_data_type_states_.end(); ++iter) {
    iter->second = syncer::GetUploadToGoogleState(sync_service_, iter->first);
  }
}

}  // namespace

UrlKeyedDataCollectionConsentHelper::UrlKeyedDataCollectionConsentHelper() =
    default;
UrlKeyedDataCollectionConsentHelper::~UrlKeyedDataCollectionConsentHelper() =
    default;

// static
std::unique_ptr<UrlKeyedDataCollectionConsentHelper>
UrlKeyedDataCollectionConsentHelper::NewAnonymizedDataCollectionConsentHelper(
    PrefService* pref_service) {
  return std::make_unique<PrefBasedUrlKeyedDataCollectionConsentHelper>(
      pref_service);
}

// static
std::unique_ptr<UrlKeyedDataCollectionConsentHelper>
UrlKeyedDataCollectionConsentHelper::NewPersonalizedDataCollectionConsentHelper(
    syncer::SyncService* sync_service) {
  return std::make_unique<SyncBasedUrlKeyedDataCollectionConsentHelper>(
      sync_service, std::set<syncer::ModelType>(
                        {syncer::ModelType::HISTORY_DELETE_DIRECTIVES}));
}

// static
std::unique_ptr<UrlKeyedDataCollectionConsentHelper>
UrlKeyedDataCollectionConsentHelper::
    NewPersonalizedBookmarksDataCollectionConsentHelper(
        syncer::SyncService* sync_service) {
  return std::make_unique<SyncBasedUrlKeyedDataCollectionConsentHelper>(
      sync_service,
      std::set<syncer::ModelType>({syncer::ModelType::BOOKMARKS}));
}

bool UrlKeyedDataCollectionConsentHelper::IsEnabled() {
  return GetConsentState() == State::kEnabled;
}

void UrlKeyedDataCollectionConsentHelper::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}
void UrlKeyedDataCollectionConsentHelper::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void UrlKeyedDataCollectionConsentHelper::FireOnStateChanged() {
  for (auto& observer : observer_list_)
    observer.OnUrlKeyedDataCollectionConsentStateChanged(this);
}

}  // namespace unified_consent
