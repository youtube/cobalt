// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/features/common_data.pb.h"
#include "net/base/backoff_entry.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace personal_context {
class PersonalContextService;
}  // namespace personal_context

namespace autofill {

class PersonalContextAccessManagerImpl
    : public PersonalContextAccessManager,
      public personal_context::PersonalContextEnablementService::Observer {
 public:
  // The TTL for prefetched (masked/non-SPII) entities.
  static constexpr base::TimeDelta kPrefetchedEntitiesCacheTTL =
      base::Minutes(30);
  // The TTL for unmasked sensitive PII (SPII) entities.
  static constexpr base::TimeDelta kUnmaskedSpiiCacheTTL = base::Minutes(1);

  PersonalContextAccessManagerImpl(
      personal_context::PersonalContextService* personal_context_service,
      personal_context::PersonalContextEnablementService*
          personal_context_enablement_service);

  PersonalContextAccessManagerImpl(const PersonalContextAccessManagerImpl&) =
      delete;
  PersonalContextAccessManagerImpl& operator=(
      const PersonalContextAccessManagerImpl&) = delete;

  ~PersonalContextAccessManagerImpl() override;

  // PersonalContextAccessManager:
  void PrefetchAmbientAutofillContext(
      base::span<const EntityType> requested_types) override;
  RequestStatus GetPrefetchAmbientAutofillStatusByEntityType(
      EntityType type) const override;
  std::optional<EntityInstance> GetCachedEntity(
      const EntityInstance::EntityId& id) const override;
  void GetUnmaskedSpiiEntity(const EntityInstance::EntityId& id,
                             GetUnmaskedSpiiEntityCallback callback) override;
  std::vector<EntityInstance> GetCachedEntities() const override;
  bool IsTypeCached(EntityType type) const override;
  void AddObserver(PersonalContextAccessManager::Observer* observer) override;
  void RemoveObserver(
      PersonalContextAccessManager::Observer* observer) override;

  // personal_context::PersonalContextEnablementService::Observer:
  void OnEnablementStateChanged(
      personal_context::PersonalContextEnablementState new_state) override;

  // This method should only be used to populate this access manager with
  // testing entities provided via command line.
  void SetTestingEntities(const std::vector<EntityInstance>& test_entities);

 private:
  friend class PersonalContextAccessManagerImplTestApi;

  struct RequestState {
    RequestStatus status = RequestStatus::kNotStarted;
    base::TimeTicks last_update_time;
    std::unique_ptr<net::BackoffEntry> backoff_entry;
  };

  // Clears all caches and invalidates weak pointers.
  void WipeCaches();

  // Resets the cache state for `type` by clearing both the prefetched
  // (masked) entities and the unmasked SPII entities of this type. This ensures
  // that refreshing or invalidating prefetched data also invalidates any
  // corresponding unmasked sensitive data.
  void ResetCacheForType(EntityType type);

  // Handles the asynchronous result of the ambient autofill context fetch.
  void OnPrefetchAmbientAutofillContextComplete(
      std::vector<EntityType> requested_types,
      personal_context::FetchContextResult result);

  // Handles the asynchronous result of the SPII entities fetch.
  void OnFetchPiiEntitiesComplete(
      const EntityInstance::EntityId& id,
      GetUnmaskedSpiiEntityCallback callback,
      personal_context::FetchPiiEntitiesResult result);

  // Caches a batch of prefetched `entities` and their original `protos`.
  // Schedules new invalidations for each `EntityType` after
  // `kPrefetchedEntitiesCacheTTL`. The original entity `protos` from where
  // `entities` were extracted also require caching for subsequent
  // unmasking requests.
  void CachePrefetchedEntities(
      absl::flat_hash_map<EntityType, std::vector<EntityInstance>> entities,
      absl::flat_hash_map<EntityInstance::EntityId,
                          personal_context::proto::Entity> protos);

  // Returns true if a network request should be initiated for `type`.
  // This is true if the type is not cached, its cache TTL has expired, or a
  // previous fetch failed and is now eligible for a retry.
  bool ShouldRequestType(EntityType type) const;

  // Evaluates whether enough time has elapsed since the last failure to
  // attempt fetching the type again, taking backoff delays into account.
  bool ShouldRetryAfterFailure(const RequestState& state) const;

  // Marks the cache state for `type` as `status`. Updates the timestamp
  // to start the cache TTL timer and sets the appropriate failure count.
  void SetTypeStatus(EntityType type, RequestStatus status);

  // Notifies observers of the prefetch status.
  void NotifyPrefetchStatusObservers(bool success);

  // Caches an unmasked SPII `entity`, so it can be refilled without an
  // additional network round trip for `kUnmaskedSpiiCacheTTL`.
  void CacheUnmaskedSpiiEntity(EntityInstance entity);

  const raw_ref<personal_context::PersonalContextService>
      personal_context_service_;
  const raw_ref<personal_context::PersonalContextEnablementService>
      personal_context_enablement_service_;

  // Cache of prefetched entity instances (containing masked/obfuscated values).
  //
  // **Eviction Mechanism**: Managed **per entity type** (not per individual
  // entity). When a type is prefetched, its lifetime is tracked in
  // `cached_state_`. After `kPrefetchedEntitiesCacheTTL` the entire
  // type expires, and all entities belonging to this type are evicted together
  // from this cache.
  //
  // **Interaction with SPII Cache**:
  // - When a type is explicitly reset or updated with new prefetched entities
  //   (via `ResetCacheForType`), both this cache and the `unmasked_spii_cache_`
  //   are cleared for that type to prevent stale unmasked data.
  // - Natural expiration of this cache also forcibly evicts all entries of this
  //   type from `unmasked_spii_cache_`.
  base::flat_set<EntityInstance, EntityInstance::CompareByGuid>
      prefetched_entity_cache_;

  // Map from EntityId to the original proto Entity received during prefetch.
  absl::flat_hash_map<EntityInstance::EntityId, personal_context::proto::Entity>
      prefetched_proto_cache_;

  // Cache of unmasked sensitive PII (SPII) entity instances.
  //
  // **Eviction Mechanism**: Managed **per individual entity** (not per type).
  // When an entity is individually unmasked, it is added here, and a separate
  // task is scheduled to evict just this entity after `kUnmaskedSpiiCacheTTL`.
  //
  // **Interaction with Prefetched Cache**:
  // - This cache is dependent on the freshness of the prefetched data. If the
  //   prefetched cache for a type is reset, updated or naturally expires, all
  //   unmasked entities of that type are immediately evicted from this cache
  //   (via `ResetCacheForType`). This ensures we do not serve unmasked SPII for
  //   entities that have been removed or updated in the masked cache, or when
  //   the prefetch cache has expired.
  base::flat_set<EntityInstance, EntityInstance::CompareByGuid>
      unmasked_spii_cache_;

  // Maps entity types to their current cache request/response state.
  base::flat_map<EntityType, RequestState> cache_state_;

  base::ObserverList<PersonalContextAccessManager::Observer> observers_;

  base::ScopedObservation<
      personal_context::PersonalContextEnablementService,
      personal_context::PersonalContextEnablementService::Observer>
      enablement_service_observation_{this};

  base::WeakPtrFactory<PersonalContextAccessManagerImpl> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_H_
