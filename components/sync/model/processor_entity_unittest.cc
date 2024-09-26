// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/processor_entity.h"

#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

const char kKey[] = "key";
const ClientTagHash kHash = ClientTagHash::FromHashed("hash");
const char kId[] = "id";
const char kName[] = "name";
const char kValue1[] = "value1";
const char kValue2[] = "value2";
const char kValue3[] = "value3";

sync_pb::EntitySpecifics GenerateSpecifics(const std::string& name,
                                           const std::string& value) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(name);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

std::unique_ptr<EntityData> GenerateEntityData(const ClientTagHash& hash,
                                               const std::string& name,
                                               const std::string& value) {
  std::unique_ptr<EntityData> entity_data(new EntityData());
  entity_data->client_tag_hash = hash;
  entity_data->specifics = GenerateSpecifics(name, value);
  entity_data->name = name;
  return entity_data;
}

UpdateResponseData GenerateUpdate(const ProcessorEntity& entity,
                                  const ClientTagHash& hash,
                                  const std::string& id,
                                  const std::string& name,
                                  const std::string& value,
                                  const base::Time& mtime,
                                  int64_t version) {
  std::unique_ptr<EntityData> data = GenerateEntityData(hash, name, value);
  data->id = id;
  data->modification_time = mtime;
  UpdateResponseData update;
  update.entity = std::move(*data);
  update.response_version = version;
  return update;
}

UpdateResponseData GenerateTombstone(const ProcessorEntity& entity,
                                     const ClientTagHash& hash,
                                     const std::string& id,
                                     const std::string& name,
                                     const base::Time& mtime,
                                     int64_t version) {
  std::unique_ptr<EntityData> data = std::make_unique<EntityData>();
  data->client_tag_hash = hash;
  data->name = name;
  data->id = id;
  data->modification_time = mtime;
  UpdateResponseData update;
  update.entity = std::move(*data);
  update.response_version = version;
  return update;
}

CommitResponseData GenerateAckData(const CommitRequestData& request,
                                   const std::string id,
                                   int64_t version) {
  CommitResponseData response;
  response.id = id;
  response.client_tag_hash = request.entity->client_tag_hash;
  response.sequence_number = request.sequence_number;
  response.response_version = version;
  response.specifics_hash = request.specifics_hash;
  return response;
}

}  // namespace

// Some simple sanity tests for the ProcessorEntity.
//
// A lot of the more complicated sync logic is implemented in the
// ClientTagBasedModelTypeProcessor that owns the ProcessorEntity.  We
// can't unit test it here.
//
// Instead, we focus on simple tests to make sure that variables are getting
// properly intialized and flags properly set.  Anything more complicated would
// be a redundant and incomplete version of the ClientTagBasedModelTypeProcessor
// tests.
class ProcessorEntityTest : public ::testing::Test {
 public:
  ProcessorEntityTest() : ctime_(base::Time::Now() - base::Seconds(1)) {}

  std::unique_ptr<ProcessorEntity> CreateNew() {
    return ProcessorEntity::CreateNew(kKey, kHash, "", ctime_);
  }

  std::unique_ptr<ProcessorEntity> CreateNewWithEmptyStorageKey() {
    return ProcessorEntity::CreateNew("", kHash, "", ctime_);
  }

  std::unique_ptr<ProcessorEntity> CreateSynced() {
    std::unique_ptr<ProcessorEntity> entity = CreateNew();
    UpdateResponseData update =
        GenerateUpdate(*entity, kHash, kId, kName, kValue1, ctime_, 1);
    entity->RecordAcceptedRemoteUpdate(update, /*trimmed_specifics=*/{});
    DCHECK(!entity->IsUnsynced());
    return entity;
  }

  std::unique_ptr<ProcessorEntity> RestoreFromMetadata(
      sync_pb::EntityMetadata entity_metadata) {
    return ProcessorEntity::CreateFromMetadata(kKey,
                                               std::move(entity_metadata));
  }

  const base::Time ctime_;
};

// Test the state of the default new entity.
TEST_F(ProcessorEntityTest, DefaultEntity) {
  std::unique_ptr<ProcessorEntity> entity = CreateNew();

  EXPECT_EQ(kKey, entity->storage_key());
  EXPECT_EQ(kHash.value(), entity->metadata().client_tag_hash());
  EXPECT_EQ("", entity->metadata().server_id());
  EXPECT_FALSE(entity->metadata().is_deleted());
  EXPECT_EQ(0, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, entity->metadata().server_version());
  EXPECT_EQ(TimeToProtoTime(ctime_), entity->metadata().creation_time());
  EXPECT_EQ(0, entity->metadata().modification_time());
  EXPECT_TRUE(entity->metadata().specifics_hash().empty());
  EXPECT_TRUE(entity->metadata().base_specifics_hash().empty());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->IsVersionAlreadyKnown(1));
  EXPECT_FALSE(entity->HasCommitData());
}

// Test creating and commiting a new local item.
TEST_F(ProcessorEntityTest, NewLocalItem) {
  std::unique_ptr<ProcessorEntity> entity = CreateNew();
  entity->RecordLocalUpdate(GenerateEntityData(kHash, kName, kValue1),
                            /*trimmed_specifics=*/{});

  EXPECT_EQ("", entity->metadata().server_id());
  EXPECT_FALSE(entity->metadata().is_deleted());
  EXPECT_EQ(1, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_EQ(kUncommittedVersion, entity->metadata().server_version());
  EXPECT_NE(0, entity->metadata().modification_time());
  EXPECT_FALSE(entity->metadata().specifics_hash().empty());
  EXPECT_TRUE(entity->metadata().base_specifics_hash().empty());

  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_TRUE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->IsVersionAlreadyKnown(1));
  EXPECT_TRUE(entity->HasCommitData());

  EXPECT_EQ(kValue1, entity->commit_data().specifics.preference().value());

  // Generate a commit request. The metadata should not change.
  const sync_pb::EntityMetadata metadata_v1 = entity->metadata();
  CommitRequestData request;
  entity->InitializeCommitRequestData(&request);
  EXPECT_EQ(metadata_v1.SerializeAsString(),
            entity->metadata().SerializeAsString());

  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->IsVersionAlreadyKnown(1));

  const EntityData& data = *request.entity;
  EXPECT_EQ("", data.id);
  EXPECT_EQ(kHash, data.client_tag_hash);
  EXPECT_EQ(kName, data.name);
  EXPECT_EQ(kValue1, data.specifics.preference().value());
  EXPECT_EQ(TimeToProtoTime(ctime_), TimeToProtoTime(data.creation_time));
  EXPECT_EQ(entity->metadata().modification_time(),
            TimeToProtoTime(data.modification_time));
  EXPECT_FALSE(data.is_deleted());
  EXPECT_EQ(1, request.sequence_number);
  EXPECT_EQ(kUncommittedVersion, request.base_version);
  EXPECT_EQ(entity->metadata().specifics_hash(), request.specifics_hash);

  // Ack the commit.
  entity->ReceiveCommitResponse(GenerateAckData(request, kId, 1), false);

  EXPECT_EQ(kId, entity->metadata().server_id());
  EXPECT_FALSE(entity->metadata().is_deleted());
  EXPECT_EQ(1, entity->metadata().sequence_number());
  EXPECT_EQ(1, entity->metadata().acked_sequence_number());
  EXPECT_EQ(1, entity->metadata().server_version());
  EXPECT_EQ(metadata_v1.creation_time(), entity->metadata().creation_time());
  EXPECT_EQ(metadata_v1.modification_time(),
            entity->metadata().modification_time());
  EXPECT_FALSE(entity->metadata().specifics_hash().empty());
  EXPECT_TRUE(entity->metadata().base_specifics_hash().empty());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_TRUE(entity->IsVersionAlreadyKnown(1));
  EXPECT_FALSE(entity->HasCommitData());
}

// Test handling of invalid server version.
TEST_F(ProcessorEntityTest,
       ShouldIgnoreCommitResponseWithInvalidServerVersion) {
  std::unique_ptr<ProcessorEntity> entity = CreateNew();
  entity->RecordLocalUpdate(GenerateEntityData(kHash, kName, kValue1),
                            /*trimmed_specifics=*/{});

  CommitRequestData request;

  // Ack the commit - set current version to 2.
  entity->InitializeCommitRequestData(&request);
  entity->ReceiveCommitResponse(GenerateAckData(request, kId, 2), false);

  entity->RecordLocalUpdate(GenerateEntityData(kHash, kName, kValue2),
                            /*trimmed_specifics=*/{});
  ASSERT_EQ(2, entity->metadata().server_version());
  ASSERT_EQ(2, entity->metadata().sequence_number());
  ASSERT_EQ(1, entity->metadata().acked_sequence_number());

  // Ack the commit - try server version 1.
  entity->InitializeCommitRequestData(&request);
  entity->ReceiveCommitResponse(GenerateAckData(request, kId, 1), false);
  // no update as the server responds with an older version.
  EXPECT_EQ(2, entity->metadata().server_version());
}

// Test state for a newly synced server item.
TEST_F(ProcessorEntityTest, NewServerItem) {
  std::unique_ptr<ProcessorEntity> entity = CreateNew();

  const base::Time mtime = base::Time::Now();
  UpdateResponseData update =
      GenerateUpdate(*entity, kHash, kId, kName, kValue1, mtime, 10);
  entity->RecordAcceptedRemoteUpdate(update, /*trimmed_specifics=*/{});

  EXPECT_EQ(kId, entity->metadata().server_id());
  EXPECT_FALSE(entity->metadata().is_deleted());
  EXPECT_EQ(0, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_EQ(10, entity->metadata().server_version());
  EXPECT_EQ(TimeToProtoTime(mtime), entity->metadata().modification_time());
  EXPECT_FALSE(entity->metadata().specifics_hash().empty());
  EXPECT_TRUE(entity->metadata().base_specifics_hash().empty());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_TRUE(entity->IsVersionAlreadyKnown(9));
  EXPECT_TRUE(entity->IsVersionAlreadyKnown(10));
  EXPECT_FALSE(entity->IsVersionAlreadyKnown(11));
  EXPECT_FALSE(entity->HasCommitData());
}

// Test creating an entity for new server item with empty storage key, applying
// update and updating storage key.
TEST_F(ProcessorEntityTest, NewServerItem_EmptyStorageKey) {
  std::unique_ptr<ProcessorEntity> entity = CreateNewWithEmptyStorageKey();

  EXPECT_EQ("", entity->storage_key());

  const base::Time mtime = base::Time::Now();
  UpdateResponseData update =
      GenerateUpdate(*entity, kHash, kId, kName, kValue1, mtime, 10);
  entity->RecordAcceptedRemoteUpdate(update, /*trimmed_specifics=*/{});
  entity->SetStorageKey(kKey);
  EXPECT_EQ(kKey, entity->storage_key());
}

// Test state for a tombstone received for a previously unknown item.
TEST_F(ProcessorEntityTest, NewServerTombstone) {
  std::unique_ptr<ProcessorEntity> entity = CreateNew();

  const base::Time mtime = base::Time::Now();
  UpdateResponseData tombstone =
      GenerateTombstone(*entity, kHash, kId, kName, mtime, 1);
  entity->RecordAcceptedRemoteUpdate(tombstone, /*trimmed_specifics=*/{});

  EXPECT_EQ(kId, entity->metadata().server_id());
  EXPECT_TRUE(entity->metadata().is_deleted());
  EXPECT_EQ(0, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_EQ(1, entity->metadata().server_version());
  EXPECT_EQ(TimeToProtoTime(mtime), entity->metadata().modification_time());
  EXPECT_TRUE(entity->metadata().specifics_hash().empty());
  EXPECT_TRUE(entity->metadata().base_specifics_hash().empty());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_TRUE(entity->CanClearMetadata());
  EXPECT_TRUE(entity->IsVersionAlreadyKnown(1));
  EXPECT_FALSE(entity->IsVersionAlreadyKnown(2));
  EXPECT_FALSE(entity->HasCommitData());
}

// Apply a deletion update to a synced item.
TEST_F(ProcessorEntityTest, ServerTombstone) {
  // Start with a non-deleted state with version 1.
  std::unique_ptr<ProcessorEntity> entity = CreateSynced();
  // A deletion update one version later.
  const base::Time mtime = base::Time::Now();
  UpdateResponseData tombstone =
      GenerateTombstone(*entity, kHash, kId, kName, mtime, 2);
  entity->RecordAcceptedRemoteUpdate(tombstone, /*trimmed_specifics=*/{});

  EXPECT_TRUE(entity->metadata().is_deleted());
  EXPECT_EQ(0, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_EQ(2, entity->metadata().server_version());
  EXPECT_EQ(TimeToProtoTime(mtime), entity->metadata().modification_time());
  EXPECT_TRUE(entity->metadata().specifics_hash().empty());
  EXPECT_TRUE(entity->metadata().base_specifics_hash().empty());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_TRUE(entity->CanClearMetadata());
  EXPECT_TRUE(entity->IsVersionAlreadyKnown(2));
  EXPECT_FALSE(entity->IsVersionAlreadyKnown(3));
  EXPECT_FALSE(entity->HasCommitData());
}

// Test a local change of a synced item.
TEST_F(ProcessorEntityTest, LocalChange) {
  std::unique_ptr<ProcessorEntity> entity = CreateSynced();
  const int64_t mtime_v0 = entity->metadata().modification_time();
  const std::string specifics_hash_v0 = entity->metadata().specifics_hash();

  // Make a local change with different specifics.
  entity->RecordLocalUpdate(GenerateEntityData(kHash, kName, kValue2),
                            /*trimmed_specifics=*/{});

  const int64_t mtime_v1 = entity->metadata().modification_time();
  const std::string specifics_hash_v1 = entity->metadata().specifics_hash();

  EXPECT_FALSE(entity->metadata().is_deleted());
  EXPECT_EQ(1, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_EQ(1, entity->metadata().server_version());
  EXPECT_LT(mtime_v0, mtime_v1);
  EXPECT_NE(specifics_hash_v0, specifics_hash_v1);
  EXPECT_EQ(specifics_hash_v0, entity->metadata().base_specifics_hash());

  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_TRUE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_TRUE(entity->HasCommitData());

  // Make a commit.
  CommitRequestData request;
  entity->InitializeCommitRequestData(&request);

  EXPECT_EQ(kId, request.entity->id);
  EXPECT_FALSE(entity->RequiresCommitRequest());

  // Ack the commit.
  entity->ReceiveCommitResponse(GenerateAckData(request, kId, 2), false);

  EXPECT_EQ(1, entity->metadata().sequence_number());
  EXPECT_EQ(1, entity->metadata().acked_sequence_number());
  EXPECT_EQ(2, entity->metadata().server_version());
  EXPECT_EQ(mtime_v1, entity->metadata().modification_time());
  EXPECT_EQ(specifics_hash_v1, entity->metadata().specifics_hash());
  EXPECT_EQ("", entity->metadata().base_specifics_hash());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->HasCommitData());
}

// Test a local deletion of a synced item.
TEST_F(ProcessorEntityTest, LocalDeletion) {
  std::unique_ptr<ProcessorEntity> entity = CreateSynced();
  const int64_t mtime = entity->metadata().modification_time();
  const std::string specifics_hash = entity->metadata().specifics_hash();

  // Make a local delete.
  entity->RecordLocalDeletion();

  EXPECT_TRUE(entity->metadata().is_deleted());
  EXPECT_EQ(1, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_EQ(1, entity->metadata().server_version());
  EXPECT_LT(mtime, entity->metadata().modification_time());
  EXPECT_TRUE(entity->metadata().specifics_hash().empty());
  EXPECT_EQ(specifics_hash, entity->metadata().base_specifics_hash());

  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_TRUE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->HasCommitData());

  // Generate a commit request. The metadata should not change.
  const sync_pb::EntityMetadata metadata_v1 = entity->metadata();
  CommitRequestData request;
  entity->InitializeCommitRequestData(&request);
  EXPECT_EQ(metadata_v1.SerializeAsString(),
            entity->metadata().SerializeAsString());

  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->HasCommitData());

  const EntityData& data = *request.entity;
  EXPECT_EQ(kId, data.id);
  EXPECT_EQ(kHash, data.client_tag_hash);
  EXPECT_EQ("", data.name);
  EXPECT_EQ(TimeToProtoTime(ctime_), TimeToProtoTime(data.creation_time));
  EXPECT_EQ(entity->metadata().modification_time(),
            TimeToProtoTime(data.modification_time));
  EXPECT_TRUE(data.is_deleted());
  EXPECT_EQ(1, request.sequence_number);
  EXPECT_EQ(1, request.base_version);
  EXPECT_EQ(entity->metadata().specifics_hash(), request.specifics_hash);

  // Ack the deletion.
  entity->ReceiveCommitResponse(GenerateAckData(request, kId, 2), false);

  EXPECT_TRUE(entity->metadata().is_deleted());
  EXPECT_EQ(1, entity->metadata().sequence_number());
  EXPECT_EQ(1, entity->metadata().acked_sequence_number());
  EXPECT_EQ(2, entity->metadata().server_version());
  EXPECT_EQ(metadata_v1.modification_time(),
            entity->metadata().modification_time());
  EXPECT_TRUE(entity->metadata().specifics_hash().empty());
  EXPECT_TRUE(entity->metadata().base_specifics_hash().empty());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_TRUE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->HasCommitData());
}

// Test a local deletion followed by an undeletion (creation).
TEST_F(ProcessorEntityTest, LocalUndeletion) {
  std::unique_ptr<ProcessorEntity> entity = CreateSynced();
  const std::string specifics_hash = entity->metadata().specifics_hash();

  entity->RecordLocalDeletion();
  ASSERT_TRUE(entity->metadata().is_deleted());
  ASSERT_TRUE(entity->IsUnsynced());
  ASSERT_EQ(1, entity->metadata().sequence_number());

  // Undelete the entity with different specifics.
  entity->RecordLocalUpdate(GenerateEntityData(kHash, kName, kValue2),
                            /*trimmed_specifics=*/{});

  const std::string specifics_hash_v1 = entity->metadata().specifics_hash();
  ASSERT_NE(specifics_hash_v1, specifics_hash);

  EXPECT_FALSE(entity->metadata().is_deleted());
  EXPECT_EQ(2, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_EQ(1, entity->metadata().server_version());

  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_TRUE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_TRUE(entity->HasCommitData());

  // Make a commit.
  CommitRequestData request;
  entity->InitializeCommitRequestData(&request);

  EXPECT_EQ(kId, request.entity->id);
  EXPECT_FALSE(entity->RequiresCommitRequest());

  // Ack the commit.
  entity->ReceiveCommitResponse(GenerateAckData(request, kId, 2), false);

  EXPECT_EQ(2, entity->metadata().sequence_number());
  EXPECT_EQ(2, entity->metadata().acked_sequence_number());
  EXPECT_EQ(2, entity->metadata().server_version());
  EXPECT_EQ(specifics_hash_v1, entity->metadata().specifics_hash());
  EXPECT_EQ("", entity->metadata().base_specifics_hash());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->HasCommitData());
}

// Test that hashes and sequence numbers are handled correctly for the "commit
// commit, ack ack" case.
TEST_F(ProcessorEntityTest, LocalChangesInterleaved) {
  std::unique_ptr<ProcessorEntity> entity = CreateSynced();
  const std::string specifics_hash_v0 = entity->metadata().specifics_hash();

  // Make the first change.
  entity->RecordLocalUpdate(GenerateEntityData(kHash, kName, kValue2),
                            /*trimmed_specifics=*/{});
  const std::string specifics_hash_v1 = entity->metadata().specifics_hash();

  EXPECT_EQ(1, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_NE(specifics_hash_v0, specifics_hash_v1);
  EXPECT_EQ(specifics_hash_v0, entity->metadata().base_specifics_hash());

  // Request the first commit.
  CommitRequestData request_v1;
  entity->InitializeCommitRequestData(&request_v1);

  // Make the second change.
  entity->RecordLocalUpdate(GenerateEntityData(kHash, kName, kValue3),
                            /*trimmed_specifics=*/{});
  const std::string specifics_hash_v2 = entity->metadata().specifics_hash();

  EXPECT_EQ(2, entity->metadata().sequence_number());
  EXPECT_EQ(0, entity->metadata().acked_sequence_number());
  EXPECT_NE(specifics_hash_v1, specifics_hash_v2);
  EXPECT_EQ(specifics_hash_v0, entity->metadata().base_specifics_hash());

  // Request the second commit.
  CommitRequestData request_v2;
  entity->InitializeCommitRequestData(&request_v2);

  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());

  // Ack the first commit.
  entity->ReceiveCommitResponse(GenerateAckData(request_v1, kId, 2), false);

  EXPECT_EQ(2, entity->metadata().sequence_number());
  EXPECT_EQ(1, entity->metadata().acked_sequence_number());
  EXPECT_EQ(2, entity->metadata().server_version());
  EXPECT_EQ(specifics_hash_v2, entity->metadata().specifics_hash());
  EXPECT_EQ(specifics_hash_v1, entity->metadata().base_specifics_hash());

  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  // Commit data has been moved already to the request.
  EXPECT_FALSE(entity->HasCommitData());

  // Ack the second commit.
  entity->ReceiveCommitResponse(GenerateAckData(request_v2, kId, 3), false);

  EXPECT_EQ(2, entity->metadata().sequence_number());
  EXPECT_EQ(2, entity->metadata().acked_sequence_number());
  EXPECT_EQ(3, entity->metadata().server_version());
  EXPECT_EQ(specifics_hash_v2, entity->metadata().specifics_hash());
  EXPECT_EQ("", entity->metadata().base_specifics_hash());

  EXPECT_FALSE(entity->IsUnsynced());
  EXPECT_FALSE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_FALSE(entity->CanClearMetadata());
  EXPECT_FALSE(entity->HasCommitData());
}

// Tests that updating entity id with commit response while next local change is
// pending correctly updates that change's id and version.
TEST_F(ProcessorEntityTest, NewLocalChangeUpdatedId) {
  std::unique_ptr<ProcessorEntity> entity = CreateNew();
  // Create new local change. Make sure initial id is empty.
  entity->RecordLocalUpdate(GenerateEntityData(kHash, kName, kValue1),
                            /*trimmed_specifics=*/{});

  CommitRequestData request;
  entity->InitializeCommitRequestData(&request);
  EXPECT_TRUE(request.entity->id.empty());

  // Before receiving commit response make local modification to the entity.
  entity->RecordLocalUpdate(GenerateEntityData(kHash, kName, kValue2),
                            /*trimmed_specifics=*/{});
  entity->ReceiveCommitResponse(GenerateAckData(request, kId, 1), false);

  // Receiving commit response with valid id should update
  // ProcessorEntity. Consecutive commit requests should include updated
  // id.
  entity->InitializeCommitRequestData(&request);
  EXPECT_EQ(kId, request.entity->id);
  EXPECT_EQ(1, request.base_version);
}

// Tests that entity restored after restart accepts specifics that don't match
// the ones passed originally to RecordLocalUpdate.
TEST_F(ProcessorEntityTest, RestoredLocalChangeWithUpdatedSpecifics) {
  // Create new entity and preserver its metadata.
  std::unique_ptr<ProcessorEntity> entity = CreateNew();
  entity->RecordLocalUpdate(GenerateEntityData(kHash, kName, kValue1),
                            /*trimmed_specifics=*/{});
  sync_pb::EntityMetadata entity_metadata = entity->metadata();

  // Restore entity from metadata and emulate bridge passing different specifics
  // to SetCommitData.
  entity = RestoreFromMetadata(std::move(entity_metadata));
  std::unique_ptr<EntityData> entity_data =
      GenerateEntityData(kHash, kName, kValue2);
  entity->SetCommitData(std::move(entity_data));

  // No verification is necessary. SetCommitData shouldn't DCHECK.
}

// Tests the scenario where a local creation conflicts with a remote deletion,
// where usually (and in this test) local wins. In this case, the remote update
// should be ignored but the server IDs should be updated.
TEST_F(ProcessorEntityTest, LocalCreationConflictsWithServerTombstone) {
  std::unique_ptr<ProcessorEntity> entity = CreateNew();
  entity->RecordLocalUpdate(GenerateEntityData(kHash, kName, kValue1),
                            /*trimmed_specifics=*/{});

  ASSERT_TRUE(entity->IsUnsynced());
  ASSERT_TRUE(entity->RequiresCommitRequest());
  ASSERT_FALSE(entity->RequiresCommitData());
  ASSERT_TRUE(entity->HasCommitData());
  ASSERT_FALSE(entity->metadata().is_deleted());
  ASSERT_TRUE(entity->metadata().server_id().empty());

  // Before anything gets committed, we receive a remote tombstone, but local
  // would usually win so the remote update is ignored.
  UpdateResponseData tombstone =
      GenerateTombstone(*entity, kHash, kId, kName, base::Time::Now(), 2);
  entity->RecordIgnoredRemoteUpdate(tombstone);

  EXPECT_EQ(kId, entity->metadata().server_id());
  EXPECT_TRUE(entity->IsUnsynced());
  EXPECT_TRUE(entity->RequiresCommitRequest());
  EXPECT_FALSE(entity->RequiresCommitData());
  EXPECT_TRUE(entity->HasCommitData());
  EXPECT_FALSE(entity->metadata().is_deleted());

  // Generate a commit request. The server ID should have been reused from the
  // otherwise ignored update.
  const sync_pb::EntityMetadata metadata_v1 = entity->metadata();
  CommitRequestData request;
  entity->InitializeCommitRequestData(&request);
  EXPECT_EQ(kId, request.entity->id);
}

TEST_F(ProcessorEntityTest, UpdatesSpecificsCacheOnRemoteUpdates) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kCacheBaseEntitySpecificsInMetadata);
  std::unique_ptr<ProcessorEntity> entity = CreateNew();
  const base::Time mtime = base::Time::Now();
  UpdateResponseData update =
      GenerateUpdate(*entity, kHash, kId, kName, kValue1, mtime, 10);
  sync_pb::EntitySpecifics specifics_for_caching =
      GenerateSpecifics(kName, kValue2);
  entity->RecordAcceptedRemoteUpdate(update, specifics_for_caching);
  EXPECT_EQ(
      specifics_for_caching.SerializeAsString(),
      entity->metadata().possibly_trimmed_base_specifics().SerializeAsString());
}

TEST_F(ProcessorEntityTest, UpdatesSpecificsCacheOnLocalUpdates) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kCacheBaseEntitySpecificsInMetadata);

  std::unique_ptr<ProcessorEntity> entity = CreateNew();
  sync_pb::EntitySpecifics specifics_for_caching =
      GenerateSpecifics(kName, kValue2);
  entity->RecordLocalUpdate(GenerateEntityData(kHash, kName, kValue1),
                            specifics_for_caching);
  EXPECT_EQ(
      specifics_for_caching.SerializeAsString(),
      entity->metadata().possibly_trimmed_base_specifics().SerializeAsString());
}

}  // namespace syncer
