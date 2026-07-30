// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_H_

#include <optional>
#include <string>

#include "base/observer_list_types.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/core/context_memory_error.h"

namespace autofill {

class EntityType;

// Manages access to personal context data for autofill.
// Instantiated once per profile/context.
class PersonalContextAccessManager : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPrefetchAmbientAutofillContextComplete(bool success) {}
  };

  enum class RequestStatus {
    kNotStarted,
    kPending,
    kSuccess,
    kFailure,
  };

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Callback for `GetUnmaskedSpiiEntity` requests. On success, it returns
  // the unmasked `EntityInstance` corresponding to the requested `id`.
  // Returns `std::nullopt` on failure (e.g., if the entity is not found or
  // the network request fails).
  using GetUnmaskedSpiiEntityCallback =
      base::OnceCallback<void(std::optional<EntityInstance>)>;

  ~PersonalContextAccessManager() override = default;

  // TODO(crbug.com/503303085): Remove "AmbientAutofill" from the name.
  // Fetches ambient autofill context from the personal context service.
  virtual void PrefetchAmbientAutofillContext(
      base::span<const EntityType> requested_types) = 0;

  // TODO(crbug.com/503303085): Remove "AmbientAutofill" from the name.
  virtual RequestStatus GetPrefetchAmbientAutofillStatusByEntityType(
      EntityType type) const = 0;

  // Returns the cached `EntityInstance` with the given `id` if it is
  // currently cached.
  virtual std::optional<EntityInstance> GetCachedEntity(
      const EntityInstance::EntityId& id) const = 0;

  // Retrieves the unmasked SPII `EntityInstance` with the given `id`. If it is
  // in the cache, runs the `callback` immediately with the cached value.
  // Otherwise, triggers a network request and runs the `callback` with the
  // result. If the request fails synchronously, runs the `callback` with
  // `std::nullopt`.
  virtual void GetUnmaskedSpiiEntity(
      const EntityInstance::EntityId& id,
      GetUnmaskedSpiiEntityCallback callback) = 0;

  // Returns all currently cached entities.
  virtual std::vector<EntityInstance> GetCachedEntities() const = 0;

  // Returns true if all entities of the given `type_name` have been cached and
  // the cache has not expired.
  virtual bool IsTypeCached(EntityType type) const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_H_
