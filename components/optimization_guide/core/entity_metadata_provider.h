// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_METADATA_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_METADATA_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "components/optimization_guide/core/entity_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

// Callback to inform the caller that the metadata for an entity ID has been
// retrieved.
using EntityMetadataRetrievedCallback =
    base::OnceCallback<void(const absl::optional<EntityMetadata>&)>;

// Callback to inform the caller that the metadata for a batch of entity IDs has
// been retrieved.
using BatchEntityMetadataRetrievedCallback = base::OnceCallback<void(
    const base::flat_map<std::string, EntityMetadata>&)>;

// A class that provides metadata about entities.
class EntityMetadataProvider {
 public:
  // Retrieves the metadata associated with |entity_id|. Invokes |callback|
  // when done.
  virtual void GetMetadataForEntityId(
      const std::string& entity_id,
      EntityMetadataRetrievedCallback callback) = 0;

  // Retrieves the metadata associated for each entry in |entity_ids|. Invokes
  // |callback| when done.
  virtual void GetMetadataForEntityIds(
      const base::flat_set<std::string>& entity_ids,
      BatchEntityMetadataRetrievedCallback callback) = 0;

 protected:
  EntityMetadataProvider() = default;
  virtual ~EntityMetadataProvider() = default;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_METADATA_PROVIDER_H_