// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl.h"

#include <algorithm>
#include <optional>
#include <set>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/manual_testing_import.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_conversion_util.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/personal_context/proto/features/ambient_autofill.pb.h"
#include "components/prefs/pref_service.h"
#include "net/base/backoff_entry.h"

namespace autofill {

namespace {

// Configuration for exponential backoff on failed prefetch requests.
// Subsequent prefetch requests are blocked until the backoff delay expires
// (starts at 1s, doubles for each consecutive failure, capped at 1 hour).
constexpr net::BackoffEntry::Policy kBackoffPolicy = {
    .num_errors_to_ignore = 0,
    .initial_delay_ms = base::Seconds(1).InMilliseconds(),
    .multiply_factor = 2.0,
    .jitter_factor = 0.0,
    .maximum_backoff_ms = base::Hours(1).InMilliseconds(),
    .entry_lifetime_ms = -1,
    .always_use_initial_delay = false};

bool IsPersonalContextEnabled(
    personal_context::PersonalContextEnablementState state) {
  using enum personal_context::PersonalContextEnablementState;
  switch (state) {
    case kDisabledNotEligible:
    case kDisabledNeedsOptIn:
    case kDisabledViaPersonalIntelligenceInAutofillToggle:
      return false;
    case kEnabled:
    case kEnabledShouldShowNotice:
      return true;
  }
}

bool IsPrefetchContextEnabled(
    personal_context::PersonalContextEnablementService& enablement_service,
    PrefService* pref_service) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAmbientAutofill)) {
    return false;
  }

  if (base::FeatureList::IsEnabled(
          features::debug::kAutofillAmbientAutofillSkipEligibilityChecks)) {
    return true;
  }

  return IsPersonalContextEnabled(enablement_service.GetEnablementState());
}

}  // namespace

PersonalContextAccessManagerImpl::PersonalContextAccessManagerImpl(
    personal_context::PersonalContextService* personal_context_service,
    personal_context::PersonalContextEnablementService*
        personal_context_enablement_service,
    PrefService* pref_service)
    : personal_context_service_(CHECK_DEREF(personal_context_service)),
      personal_context_enablement_service_(
          CHECK_DEREF(personal_context_enablement_service)),
      pref_service_(pref_service) {
  enablement_service_observation_.Observe(personal_context_enablement_service);
  if (pref_service_) {
    pref_registrar_.Init(pref_service_);
    pref_registrar_.Add(
        personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
        base::BindRepeating(&PersonalContextAccessManagerImpl::
                                OnPersonalContextSettingsToggleChanged,
                            base::Unretained(this)));
  }
  MaybeImportEntitiesForTesting(weak_factory_.GetWeakPtr());
}

PersonalContextAccessManagerImpl::~PersonalContextAccessManagerImpl() = default;

void PersonalContextAccessManagerImpl::PrefetchContext(
    base::span<const EntityType> requested_types) {
  if (!IsPrefetchContextEnabled(*personal_context_enablement_service_,
                                pref_service_)) {
    return;
  }

  std::vector<EntityType> types_to_request;
  for (const EntityType& type : requested_types) {
    if (ShouldRequestType(type)) {
      types_to_request.push_back(type);
      SetTypeStatus(type, RequestStatus::kPending);
    }
  }

  if (types_to_request.empty()) {
    NotifyPrefetchStatusObservers({});
    return;
  }

  personal_context::proto::ContextMemoryAmbientAutofillRequest request;
  for (const EntityType& type : types_to_request) {
    request.add_requested_types(
        AutofillEntityTypeToPersonalContextEntityType(type));
  }

  personal_context_service_->FetchContext(
      personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, request,
      /*options=*/{},
      base::BindOnce(
          &PersonalContextAccessManagerImpl::OnPrefetchContextRequestComplete,
          weak_factory_.GetWeakPtr(), std::move(types_to_request)));
}

void PersonalContextAccessManagerImpl::OnPrefetchContextRequestComplete(
    std::vector<EntityType> requested_types,
    personal_context::FetchContextResult result) {
  if (!result.response.has_value()) {
    for (const EntityType& type : requested_types) {
      SetTypeStatus(type, RequestStatus::kFailure);
    }
    NotifyPrefetchStatusObservers({});
    return;
  }

  base::expected<std::vector<ParsedEntity>,
                 personal_context::ContextMemoryError>
      parsed_entities =
          ExtractEntitiesFromResponse(result.response.value().value());

  if (!parsed_entities.has_value()) {
    for (const EntityType& type : requested_types) {
      SetTypeStatus(type, RequestStatus::kFailure);
    }
    NotifyPrefetchStatusObservers({});
    return;
  }

  ProcessPrefetchedEntities(std::move(*parsed_entities), requested_types);
}

base::expected<std::vector<PersonalContextAccessManagerImpl::ParsedEntity>,
               personal_context::ContextMemoryError>
PersonalContextAccessManagerImpl::ExtractEntitiesFromResponse(
    std::string_view serialized_response) {
  personal_context::proto::ContextMemoryAmbientAutofillResponse response;
  if (!response.ParseFromString(serialized_response)) {
    return base::unexpected(
        personal_context::ContextMemoryError::FromExecutionError(
            personal_context::ContextMemoryError::ExecutionError::
                kResponseParseError));
  }

  std::vector<ParsedEntity> entities;
  entities.reserve(response.entities_size());
  for (const personal_context::proto::Entity& entity : response.entities()) {
    if (std::optional<EntityInstance> converted =
            PersonalContextEntityToEntityInstance(entity)) {
      entities.push_back({std::move(*converted), std::move(entity)});
    }
  }
  return entities;
}

void PersonalContextAccessManagerImpl::GetUnmaskedSpiiEntity(
    const EntityInstance::EntityId& id,
    GetUnmaskedSpiiEntityCallback callback) {
  if (auto it = unmasked_spii_cache_.find(id);
      it != unmasked_spii_cache_.end()) {
    std::move(callback).Run(*it);
    return;
  }

  personal_context::proto::Entity* proto_entity =
      base::FindOrNull(prefetched_proto_cache_, id);
  if (!proto_entity) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  personal_context::proto::FetchPiiEntitiesRequest request;
  request.set_feature(
      personal_context::proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL);
  *request.add_masked_entities() = *proto_entity;

  personal_context_service_->FetchPiiEntities(
      request, /*options=*/{},
      base::BindOnce(
          &PersonalContextAccessManagerImpl::OnFetchPiiEntitiesComplete,
          weak_factory_.GetWeakPtr(), id, std::move(callback)));
}

void PersonalContextAccessManagerImpl::OnFetchPiiEntitiesComplete(
    const EntityInstance::EntityId& id,
    GetUnmaskedSpiiEntityCallback callback,
    personal_context::FetchPiiEntitiesResult result) {
  if (!result.response.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  const personal_context::proto::FetchPiiEntitiesResponse& response =
      result.response.value();
  if (response.entities().empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::optional<EntityInstance> unmasked_entity =
      PersonalContextEntityToEntityInstance(response.entities(0),
                                            /*is_masked=*/false);
  if (!unmasked_entity) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  EntityInstance final_entity = unmasked_entity->CopyWithNewEntityId(id);
  CacheUnmaskedSpiiEntity(final_entity);
  std::move(callback).Run(std::move(final_entity));
}

bool PersonalContextAccessManagerImpl::IsTypePrefetched(EntityType type) const {
  const RequestState* request_state = base::FindOrNull(prefetch_state_, type);
  return request_state && request_state->status == RequestStatus::kSuccess;
}

void PersonalContextAccessManagerImpl::AddObserver(
    PersonalContextAccessManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void PersonalContextAccessManagerImpl::RemoveObserver(
    PersonalContextAccessManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

PersonalContextAccessManager::RequestStatus
PersonalContextAccessManagerImpl::GetPrefetchStatusByEntityType(
    EntityType type) const {
  if (const RequestState* state = base::FindOrNull(prefetch_state_, type)) {
    return state->status;
  }
  return RequestStatus::kNotStarted;
}

bool PersonalContextAccessManagerImpl::ServerHasDataAvailable(
    EntityType type) const {
  // TODO(crbug.com/503303085): Implement this properly.
  return false;
}

void PersonalContextAccessManagerImpl::ResetStateForType(EntityType type) {
  // Clear existing proto entities of this type.
  absl::erase_if(prefetched_proto_cache_, [type](const auto& entry) {
    return ToEntityType(entry.second.entity_case()) == type;
  });
  // Clear unmasked SPII of this type.
  base::EraseIf(unmasked_spii_cache_, [type](EntityInstance& entity) {
    return entity.type() == type;
  });
  prefetch_state_.erase(type);
  observers_.Notify(
      &PersonalContextAccessManager::Observer::OnMaskedEntityTypeEvicted, *this,
      type);
}

void PersonalContextAccessManagerImpl::ProcessPrefetchedEntities(
    std::vector<ParsedEntity> parsed_entities,
    base::span<const EntityType> requested_types) {
  // Evict existing entities for the `requested_types`.
  for (EntityType type : requested_types) {
    ResetStateForType(type);
    SetTypeStatus(type, RequestStatus::kSuccess);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PersonalContextAccessManagerImpl::ResetStateForType,
                       weak_factory_.GetWeakPtr(), type),
        kPrefetchedEntitiesCacheTTL);
  }
  // Populate the proto cache and notify observers about the fetched entities.
  std::vector<EntityInstance> entities;
  entities.reserve(parsed_entities.size());
  for (ParsedEntity& entity : parsed_entities) {
    prefetched_proto_cache_.emplace(entity.instance.guid(),
                                    std::move(entity.proto));
    entities.push_back(std::move(entity.instance));
  }
  NotifyPrefetchStatusObservers(entities);
}

void PersonalContextAccessManagerImpl::CacheUnmaskedSpiiEntity(
    EntityInstance entity) {
  EntityInstance::EntityId id = entity.guid();
  auto [it, inserted] = unmasked_spii_cache_.insert(std::move(entity));
  if (!inserted) {
    return;
  }
  // Clear the cache entry after `kUnmaskedSpiiCacheTTL`.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PersonalContextAccessManagerImpl> access_manager,
             const EntityInstance::EntityId& id) {
            if (!access_manager) {
              return;
            }
            // Remove if exists.
            access_manager->unmasked_spii_cache_.erase(id);
          },
          weak_factory_.GetWeakPtr(), id),
      kUnmaskedSpiiCacheTTL);
}

void PersonalContextAccessManagerImpl::WipeCache() {
  // Invalidate weak pointers to cancel any pending fetches.
  weak_factory_.InvalidateWeakPtrs();
  // Copy the keys since `ResetStateForType()` invalidates iterators to
  // `prefetch_state_`.
  std::vector<EntityType> prefetched_types = base::ToVector(
      prefetch_state_, [](const auto& item) { return item.first; });
  for (EntityType type : prefetched_types) {
    ResetStateForType(type);
  }
}

void PersonalContextAccessManagerImpl::OnEnablementStateChanged(
    personal_context::PersonalContextEnablementState new_state) {
  if (!IsPersonalContextEnabled(new_state)) {
    WipeCache();
  }
}

void PersonalContextAccessManagerImpl::
    OnPersonalContextSettingsToggleChanged() {
  if (pref_service_ &&
      !pref_service_->GetBoolean(
          personal_context::prefs::
              kPersonalContextInAutofillSettingsToggleStatus)) {
    WipeCache();
  }
}

void PersonalContextAccessManagerImpl::SetTestingEntities(
    const std::vector<EntityInstance>& test_entities) {
  std::vector<ParsedEntity> parsed_entities;
  std::set<EntityType> types;
  for (const EntityInstance& entity : test_entities) {
    types.insert(entity.type());
    parsed_entities.push_back({
        .instance = entity,
        .proto = personal_context::proto::Entity(),
    });
  }
  ProcessPrefetchedEntities(
      std::move(parsed_entities),
      std::vector<EntityType>(types.begin(), types.end()));
}

bool PersonalContextAccessManagerImpl::ShouldRequestType(
    EntityType type) const {
  const RequestState* request_state = base::FindOrNull(prefetch_state_, type);
  if (!request_state) {
    return true;
  }

  switch (request_state->status) {
    case RequestStatus::kPending:
      return false;
    case RequestStatus::kSuccess:
      if (base::TimeTicks::Now() - request_state->last_update_time >
          kPrefetchedEntitiesCacheTTL) {
        return true;
      }
      return false;
    case RequestStatus::kFailure:
      return ShouldRetryAfterFailure(*request_state);
    case RequestStatus::kNotStarted:
      return true;
  }
}

bool PersonalContextAccessManagerImpl::ShouldRetryAfterFailure(
    const RequestState& state) const {
  return state.backoff_entry && !state.backoff_entry->ShouldRejectRequest();
}

void PersonalContextAccessManagerImpl::SetTypeStatus(EntityType type,
                                                     RequestStatus status) {
  RequestState& state = prefetch_state_[type];
  state.status = status;
  state.last_update_time = base::TimeTicks::Now();

  if (!state.backoff_entry) {
    state.backoff_entry = std::make_unique<net::BackoffEntry>(&kBackoffPolicy);
  }

  switch (status) {
    case RequestStatus::kPending:
      break;
    case RequestStatus::kSuccess:
      state.backoff_entry->Reset();
      break;
    case RequestStatus::kFailure:
      state.backoff_entry->InformOfRequest(/*succeeded=*/false);
      break;
    case RequestStatus::kNotStarted:
      break;
  }
}

void PersonalContextAccessManagerImpl::NotifyPrefetchStatusObservers(
    base::span<const EntityInstance> entities) {
  observers_.Notify(
      &PersonalContextAccessManager::Observer::OnPrefetchContextComplete, *this,
      entities);
}

}  // namespace autofill
