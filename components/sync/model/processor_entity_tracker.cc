// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/processor_entity_tracker.h"

#include <utility>

#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/model/processor_entity.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state_helper.h"
#include "components/sync/protocol/proto_memory_estimations.h"

namespace syncer {

ProcessorEntityTracker::ProcessorEntityTracker(
    const sync_pb::ModelTypeState& model_type_state,
    std::map<std::string, std::unique_ptr<sync_pb::EntityMetadata>>
        metadata_map)
    : model_type_state_(model_type_state) {
  DCHECK(
      IsInitialSyncAtLeastPartiallyDone(model_type_state.initial_sync_state()));
  for (auto& [storage_key, metadata] : metadata_map) {
    std::unique_ptr<ProcessorEntity> entity =
        ProcessorEntity::CreateFromMetadata(storage_key, std::move(*metadata));
    const ClientTagHash client_tag_hash =
        ClientTagHash::FromHashed(entity->metadata().client_tag_hash());

    DCHECK(storage_key_to_tag_hash_.find(entity->storage_key()) ==
           storage_key_to_tag_hash_.end());
    DCHECK(entities_.find(client_tag_hash) == entities_.end());
    storage_key_to_tag_hash_[entity->storage_key()] = client_tag_hash;
    entities_[client_tag_hash] = std::move(entity);
  }
}

ProcessorEntityTracker::~ProcessorEntityTracker() = default;

bool ProcessorEntityTracker::AllStorageKeysPopulated() const {
  for (const auto& [client_tag_hash, entity] : entities_) {
    if (entity->storage_key().empty())
      return false;
  }
  if (entities_.size() != storage_key_to_tag_hash_.size()) {
    return false;
  }
  return true;
}

void ProcessorEntityTracker::ClearTransientSyncState() {
  for (const auto& [client_tag_hash, entity] : entities_) {
    entity->ClearTransientSyncState();
  }
}

size_t ProcessorEntityTracker::CountNonTombstoneEntries() const {
  size_t count = 0;
  for (const auto& [client_tag_hash, entity] : entities_) {
    if (!entity->metadata().is_deleted()) {
      ++count;
    }
  }
  return count;
}

ProcessorEntity* ProcessorEntityTracker::AddUnsyncedLocal(
    const std::string& storage_key,
    std::unique_ptr<EntityData> data,
    sync_pb::EntitySpecifics trimmed_specifics) {
  DCHECK(data);
  DCHECK(!data->client_tag_hash.value().empty());
  DCHECK(!GetEntityForTagHash(data->client_tag_hash));
  DCHECK(!data->is_deleted());
  DCHECK(!storage_key.empty());

  ProcessorEntity* entity =
      AddInternal(storage_key, *data, kUncommittedVersion);
  entity->RecordLocalUpdate(std::move(data), std::move(trimmed_specifics));
  return entity;
}

ProcessorEntity* ProcessorEntityTracker::AddRemote(
    const std::string& storage_key,
    const UpdateResponseData& update_data,
    sync_pb::EntitySpecifics trimmed_specifics) {
  const EntityData& data = update_data.entity;
  DCHECK(!data.client_tag_hash.value().empty());
  DCHECK(!GetEntityForTagHash(data.client_tag_hash));
  DCHECK(!data.is_deleted());
  DCHECK(storage_key_to_tag_hash_.find(storage_key) ==
         storage_key_to_tag_hash_.end());
  DCHECK(update_data.response_version != kUncommittedVersion);

  ProcessorEntity* entity =
      AddInternal(storage_key, data, update_data.response_version);
  entity->RecordAcceptedRemoteUpdate(update_data, std::move(trimmed_specifics));
  return entity;
}

void ProcessorEntityTracker::RemoveEntityForClientTagHash(
    const ClientTagHash& client_tag_hash) {
  DCHECK(IsInitialSyncAtLeastPartiallyDone(
      model_type_state_.initial_sync_state()));
  DCHECK(!client_tag_hash.value().empty());
  const ProcessorEntity* entity = GetEntityForTagHash(client_tag_hash);
  if (entity == nullptr || entity->storage_key().empty()) {
    entities_.erase(client_tag_hash);
  } else {
    DCHECK(storage_key_to_tag_hash_.find(entity->storage_key()) !=
           storage_key_to_tag_hash_.end());
    RemoveEntityForStorageKey(entity->storage_key());
  }
}

void ProcessorEntityTracker::RemoveEntityForStorageKey(
    const std::string& storage_key) {
  DCHECK(IsInitialSyncAtLeastPartiallyDone(
      model_type_state_.initial_sync_state()));
  // Look-up the client tag hash.
  auto iter = storage_key_to_tag_hash_.find(storage_key);
  if (iter == storage_key_to_tag_hash_.end()) {
    // Missing is as good as untracked as far as the model is concerned.
    return;
  }

  DCHECK_EQ(entities_[iter->second]->storage_key(), storage_key);
  entities_.erase(iter->second);
  storage_key_to_tag_hash_.erase(iter);
}

void ProcessorEntityTracker::ClearStorageKey(const std::string& storage_key) {
  DCHECK(!storage_key.empty());

  ProcessorEntity* entity = GetEntityForStorageKey(storage_key);
  DCHECK(entity);
  DCHECK_EQ(entity->storage_key(), storage_key);
  storage_key_to_tag_hash_.erase(storage_key);
  entity->ClearStorageKey();
}

size_t ProcessorEntityTracker::EstimateMemoryUsage() const {
  size_t memory_usage = 0;
  memory_usage += sync_pb::EstimateMemoryUsage(model_type_state_);
  memory_usage += base::trace_event::EstimateMemoryUsage(entities_);
  memory_usage +=
      base::trace_event::EstimateMemoryUsage(storage_key_to_tag_hash_);
  return memory_usage;
}

ProcessorEntity* ProcessorEntityTracker::GetEntityForTagHash(
    const ClientTagHash& tag_hash) {
  return const_cast<ProcessorEntity*>(
      static_cast<const ProcessorEntityTracker*>(this)->GetEntityForTagHash(
          tag_hash));
}

const ProcessorEntity* ProcessorEntityTracker::GetEntityForTagHash(
    const ClientTagHash& tag_hash) const {
  auto it = entities_.find(tag_hash);
  return it != entities_.end() ? it->second.get() : nullptr;
}

ProcessorEntity* ProcessorEntityTracker::GetEntityForStorageKey(
    const std::string& storage_key) {
  return const_cast<ProcessorEntity*>(
      static_cast<const ProcessorEntityTracker*>(this)->GetEntityForStorageKey(
          storage_key));
}

const ProcessorEntity* ProcessorEntityTracker::GetEntityForStorageKey(
    const std::string& storage_key) const {
  auto iter = storage_key_to_tag_hash_.find(storage_key);
  if (iter == storage_key_to_tag_hash_.end()) {
    return nullptr;
  }
  return GetEntityForTagHash(iter->second);
}

std::vector<const ProcessorEntity*>
ProcessorEntityTracker::GetAllEntitiesIncludingTombstones() const {
  std::vector<const ProcessorEntity*> entities;
  entities.reserve(entities_.size());
  for (const auto& [client_tag_hash, entity] : entities_) {
    entities.push_back(entity.get());
  }
  return entities;
}

std::vector<ProcessorEntity*>
ProcessorEntityTracker::GetEntitiesWithLocalChanges(size_t max_entries) {
  std::vector<ProcessorEntity*> entities;
  for (const auto& [client_tag_hash, entity] : entities_) {
    if (entity->RequiresCommitRequest() && !entity->RequiresCommitData()) {
      entities.push_back(entity.get());
      if (entities.size() >= max_entries)
        break;
    }
  }
  return entities;
}

bool ProcessorEntityTracker::HasLocalChanges() const {
  for (const auto& [client_tag_hash, entity] : entities_) {
    if (entity->RequiresCommitRequest()) {
      return true;
    }
  }
  return false;
}

size_t ProcessorEntityTracker::size() const {
  return entities_.size();
}

std::vector<const ProcessorEntity*>
ProcessorEntityTracker::IncrementSequenceNumberForAllExcept(
    const std::unordered_set<std::string>& already_updated_storage_keys) {
  std::vector<const ProcessorEntity*> affected_entities;
  for (const auto& [client_tag_hash, entity] : entities_) {
    if (entity->storage_key().empty() ||
        (already_updated_storage_keys.find(entity->storage_key()) !=
         already_updated_storage_keys.end())) {
      // Entities with empty storage key were already processed. ProcessUpdate()
      // incremented their sequence numbers and cached commit data. Their
      // metadata will be persisted in UpdateStorageKey().
      continue;
    }
    entity->IncrementSequenceNumber(base::Time::Now());
    affected_entities.push_back(entity.get());
  }
  return affected_entities;
}

void ProcessorEntityTracker::UpdateOrOverrideStorageKey(
    const ClientTagHash& client_tag_hash,
    const std::string& storage_key) {
  ProcessorEntity* entity = GetEntityForTagHash(client_tag_hash);
  DCHECK(entity);
  // If the entity already had a storage key, clear it.
  const std::string previous_storage_key = entity->storage_key();
  DCHECK_NE(previous_storage_key, storage_key);
  if (!previous_storage_key.empty()) {
    ClearStorageKey(previous_storage_key);
  }
  DCHECK(storage_key_to_tag_hash_.find(previous_storage_key) ==
         storage_key_to_tag_hash_.end());
  // Populate the new storage key in the existing entity.
  entity->SetStorageKey(storage_key);
  DCHECK(storage_key_to_tag_hash_.find(storage_key) ==
         storage_key_to_tag_hash_.end());
  storage_key_to_tag_hash_[storage_key] = client_tag_hash;
}

ProcessorEntity* ProcessorEntityTracker::AddInternal(
    const std::string& storage_key,
    const EntityData& data,
    int64_t server_version) {
  DCHECK(!data.client_tag_hash.value().empty());
  DCHECK(!GetEntityForTagHash(data.client_tag_hash));
  DCHECK(storage_key.empty() || storage_key_to_tag_hash_.find(storage_key) ==
                                    storage_key_to_tag_hash_.end());

  std::unique_ptr<ProcessorEntity> entity = ProcessorEntity::CreateNew(
      storage_key, data.client_tag_hash, data.id, data.creation_time);
  ProcessorEntity* entity_ptr = entity.get();
  entities_[data.client_tag_hash] = std::move(entity);
  if (!storage_key.empty()) {
    storage_key_to_tag_hash_[storage_key] = data.client_tag_hash;
  }
  return entity_ptr;
}

}  // namespace syncer
