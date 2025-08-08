// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/database.h"

#include <math.h>

#include <algorithm>
#include <cstddef>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/base_tracing.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/browser/indexed_db/indexed_db_return_value.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/instance/bucket_context_handle.h"
#include "content/browser/indexed_db/instance/callback_helpers.h"
#include "content/browser/indexed_db/instance/connection.h"
#include "content/browser/indexed_db/instance/cursor.h"
#include "content/browser/indexed_db/instance/database_callbacks.h"
#include "content/browser/indexed_db/instance/factory_client.h"
#include "content/browser/indexed_db/instance/index_writer.h"
#include "content/browser/indexed_db/instance/lock_request_data.h"
#include "content/browser/indexed_db/instance/pending_connection.h"
#include "content/browser/indexed_db/instance/transaction.h"
#include "ipc/ipc_channel.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "url/origin.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexKeys;
using blink::IndexedDBIndexMetadata;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;
using blink::IndexedDBKeyRange;
using blink::IndexedDBObjectStoreMetadata;

namespace content::indexed_db {
namespace {

// Returns an `IDBReturnValuePtr` created from the cursor's current position.
blink::mojom::IDBReturnValuePtr ExtractReturnValueFromCursorValue(
    BucketContext& bucket_context,
    const IndexedDBObjectStoreMetadata& object_store_metadata,
    BackingStore::Cursor& cursor) {
  IndexedDBReturnValue idb_return_value;
  idb_return_value.swap(*cursor.value());

  const bool is_generated_key =
      (!idb_return_value.empty() && object_store_metadata.auto_increment &&
       !object_store_metadata.key_path.IsNull());
  if (is_generated_key) {
    idb_return_value.primary_key = cursor.primary_key();
    idb_return_value.key_path = object_store_metadata.key_path;
  }

  blink::mojom::IDBReturnValuePtr mojo_return_value =
      IndexedDBReturnValue::ConvertReturnValue(&idb_return_value);
  bucket_context.CreateAllExternalObjects(
      idb_return_value.external_objects,
      &mojo_return_value->value->external_objects);
  return mojo_return_value;
}

blink::mojom::IDBErrorPtr CreateIDBErrorPtr(blink::mojom::IDBException code,
                                            const std::string& message,
                                            Transaction* transaction) {
  transaction->IncrementNumErrorsSent();
  return blink::mojom::IDBError::New(code, base::UTF8ToUTF16(message));
}

std::unique_ptr<IndexedDBKey> GenerateKey(BackingStore* backing_store,
                                          Transaction* transaction,
                                          int64_t database_id,
                                          int64_t object_store_id) {
  // Maximum integer uniquely representable as ECMAScript number.
  const int64_t max_generator_value = 9007199254740992LL;
  int64_t current_number;
  Status s = backing_store->GetKeyGeneratorCurrentNumber(
      transaction->BackingStoreTransaction(), database_id, object_store_id,
      &current_number);
  if (!s.ok()) {
    LOG(ERROR) << "Failed to GetKeyGeneratorCurrentNumber";
    return std::make_unique<IndexedDBKey>();
  }
  if (current_number < 0 || current_number > max_generator_value) {
    return std::make_unique<IndexedDBKey>();
  }

  return std::make_unique<IndexedDBKey>(current_number,
                                        blink::mojom::IDBKeyType::Number);
}

// Called at the end of a "put" operation. The key is a number that was either
// generated by the generator which now needs to be incremented (so
// `check_current` is false) or was user-supplied so we only conditionally use
// (and `check_current` is true).
Status UpdateKeyGenerator(BackingStore* backing_store,
                          Transaction* transaction,
                          int64_t database_id,
                          int64_t object_store_id,
                          const IndexedDBKey& key,
                          bool check_current) {
  DCHECK_EQ(blink::mojom::IDBKeyType::Number, key.type());
  // Maximum integer uniquely representable as ECMAScript number.
  const double max_generator_value = 9007199254740992.0;
  int64_t value = base::saturated_cast<int64_t>(
      floor(std::min(key.number(), max_generator_value)));
  return backing_store->MaybeUpdateKeyGeneratorCurrentNumber(
      transaction->BackingStoreTransaction(), database_id, object_store_id,
      value + 1, check_current);
}

}  // namespace

Database::PutOperationParams::PutOperationParams() = default;
Database::PutOperationParams::~PutOperationParams() = default;

Database::OpenCursorOperationParams::OpenCursorOperationParams() = default;
Database::OpenCursorOperationParams::~OpenCursorOperationParams() = default;

Database::Database(const std::u16string& name,
                   BucketContext& bucket_context,
                   const Identifier& unique_identifier)
    : metadata_(name,
                kInvalidId,
                IndexedDBDatabaseMetadata::NO_VERSION,
                kInvalidId),
      identifier_(unique_identifier),
      bucket_context_(bucket_context),
      connection_coordinator_(this, bucket_context) {}

Database::~Database() = default;

BackingStore* Database::backing_store() {
  return bucket_context_->backing_store();
}

PartitionedLockManager& Database::lock_manager() {
  return bucket_context_->lock_manager();
}

void Database::RequireBlockingTransactionClientsToBeActive(
    Transaction* current_transaction,
    std::vector<PartitionedLockManager::PartitionedLockRequest>&
        lock_requests) {
  std::vector<PartitionedLockId> blocked_lock_ids =
      lock_manager().GetUnacquirableLocks(lock_requests);

  if (blocked_lock_ids.empty()) {
    return;
  }

  for (Connection* connection : connections_) {
    if (connection->client_token() ==
        current_transaction->connection()->client_token()) {
      continue;
    }

    // If any of the connection's transactions is holding one of the blocked
    // lock IDs, require that client to be active.
    if (std::any_of(
            connection->transactions().begin(),
            connection->transactions().end(),
            [&](const std::pair<const int64_t, std::unique_ptr<Transaction>>&
                    existing_transaction) {
              return !base::STLSetIntersection<std::vector<PartitionedLockId>>(
                          blocked_lock_ids,
                          existing_transaction.second->lock_ids())
                          .empty();
            })) {
      connection->DisallowInactiveClient(
          storage::mojom::DisallowInactiveClientReason::
              kTransactionIsAcquiringLocks,
          base::DoNothing());
    }
  }
}

void Database::RegisterAndScheduleTransaction(Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::RegisterAndScheduleTransaction",
               "txn.id", transaction->id());
  // Locks for version change transactions are covered by `ConnectionRequest`.
  DCHECK_NE(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);
  std::vector<PartitionedLockManager::PartitionedLockRequest> lock_requests =
      transaction->BuildLockRequests();

  RequireBlockingTransactionClientsToBeActive(transaction, lock_requests);

  lock_manager().AcquireLocks(
      std::move(lock_requests), *transaction->mutable_locks_receiver(),
      base::BindOnce(&Transaction::Start, transaction->AsWeakPtr()),
      base::BindRepeating(&Connection::HasHigherPriorityThan,
                          transaction->mutable_locks_receiver()));
}

std::tuple<Database::RunTasksResult, Status> Database::RunTasks() {
  // First execute any pending tasks in the connection coordinator.
  ConnectionCoordinator::ExecuteTaskResult task_state;
  Status status;
  do {
    std::tie(task_state, status) =
        connection_coordinator_.ExecuteTask(!connections_.empty());
  } while (task_state == ConnectionCoordinator::ExecuteTaskResult::kMoreTasks);

  if (task_state == ConnectionCoordinator::ExecuteTaskResult::kError) {
    return {RunTasksResult::kError, status};
  }

  bool transactions_removed = true;

  // Finally, execute transactions that have tasks & remove those that are
  // complete.
  while (transactions_removed) {
    transactions_removed = false;
    Transaction* finished_upgrade_transaction = nullptr;
    bool upgrade_transaction_commmitted = false;
    for (Connection* connection : connections_) {
      std::vector<int64_t> txns_to_remove;
      for (const auto& id_txn_pair : connection->transactions()) {
        Transaction* txn = id_txn_pair.second.get();
        // Determine if the transaction's task queue should be processed.
        switch (txn->state()) {
          case Transaction::FINISHED:
            if (txn->mode() ==
                blink::mojom::IDBTransactionMode::VersionChange) {
              finished_upgrade_transaction = txn;
              upgrade_transaction_commmitted = !txn->aborted();
            }
            txns_to_remove.push_back(id_txn_pair.first);
            continue;
          case Transaction::CREATED:
            continue;
          case Transaction::STARTED:
          case Transaction::COMMITTING:
            break;
        }

        // Process the queue for transactions that are STARTED or COMMITTING.
        // Add transactions that can be removed to a queue.
        Transaction::RunTasksResult task_result;
        Status transaction_status;
        std::tie(task_result, transaction_status) = txn->RunTasks();
        switch (task_result) {
          case Transaction::RunTasksResult::kError:
            return {RunTasksResult::kError, transaction_status};
          case Transaction::RunTasksResult::kCommitted:
          case Transaction::RunTasksResult::kAborted:
            if (txn->mode() ==
                blink::mojom::IDBTransactionMode::VersionChange) {
              DCHECK(!finished_upgrade_transaction);
              finished_upgrade_transaction = txn;
              upgrade_transaction_commmitted = !txn->aborted();
            }
            txns_to_remove.push_back(txn->id());
            break;
          case Transaction::RunTasksResult::kNotFinished:
            continue;
        }
      }
      // Do the removals.
      for (int64_t id : txns_to_remove) {
        connection->RemoveTransaction(id);
        transactions_removed = true;
      }
      if (finished_upgrade_transaction) {
        connection_coordinator_.OnUpgradeTransactionFinished(
            upgrade_transaction_commmitted);
      }
    }
  }
  if (CanBeDestroyed()) {
    return {RunTasksResult::kCanBeDestroyed, Status::OK()};
  }
  return {RunTasksResult::kDone, Status::OK()};
}

Status Database::ForceCloseAndRunTasks() {
  Status status;
  DCHECK(!force_closing_);
  force_closing_ = true;
  for (Connection* connection : connections_) {
    connection->CloseAndReportForceClose();
  }
  connections_.clear();
  Status abort_status = connection_coordinator_.PruneTasksForForceClose();
  if (!abort_status.ok()) [[unlikely]] {
    return abort_status;
  }
  connection_coordinator_.OnNoConnections();

  // Execute any pending tasks in the connection coordinator.
  ConnectionCoordinator::ExecuteTaskResult task_state;
  do {
    std::tie(task_state, status) = connection_coordinator_.ExecuteTask(false);
    DCHECK(task_state !=
           ConnectionCoordinator::ExecuteTaskResult::kPendingAsyncWork)
        << "There are no more connections, so all tasks should be able to "
           "complete synchronously.";
  } while (task_state != ConnectionCoordinator::ExecuteTaskResult::kDone &&
           task_state != ConnectionCoordinator::ExecuteTaskResult::kError);
  DCHECK(connections_.empty());
  force_closing_ = false;
  bucket_context_->QueueRunTasks();
  return status;
}

void Database::ScheduleOpenConnection(
    std::unique_ptr<PendingConnection> connection) {
  connection_coordinator_.ScheduleOpenConnection(std::move(connection));
}

void Database::ScheduleDeleteDatabase(
    std::unique_ptr<FactoryClient> factory_client,
    base::OnceClosure on_deletion_complete) {
  connection_coordinator_.ScheduleDeleteDatabase(
      std::move(factory_client), std::move(on_deletion_complete));
}

void Database::AddObjectStoreToMetadata(
    IndexedDBObjectStoreMetadata object_store,
    int64_t new_max_object_store_id) {
  DCHECK(metadata_.object_stores.find(object_store.id) ==
         metadata_.object_stores.end());
  if (new_max_object_store_id != IndexedDBObjectStoreMetadata::kInvalidId) {
    DCHECK_LT(metadata_.max_object_store_id, new_max_object_store_id);
    metadata_.max_object_store_id = new_max_object_store_id;
  }
  metadata_.object_stores[object_store.id] = std::move(object_store);
}

IndexedDBObjectStoreMetadata Database::RemoveObjectStoreFromMetadata(
    int64_t object_store_id) {
  auto it = metadata_.object_stores.find(object_store_id);
  CHECK(it != metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata metadata = std::move(it->second);
  metadata_.object_stores.erase(it);
  return metadata;
}

void Database::AddIndexToMetadata(int64_t object_store_id,
                                  IndexedDBIndexMetadata index,
                                  int64_t new_max_index_id) {
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata& object_store =
      metadata_.object_stores[object_store_id];

  DCHECK(object_store.indexes.find(index.id) == object_store.indexes.end());
  object_store.indexes[index.id] = std::move(index);
  if (new_max_index_id != IndexedDBIndexMetadata::kInvalidId) {
    DCHECK_LT(object_store.max_index_id, new_max_index_id);
    object_store.max_index_id = new_max_index_id;
  }
}

IndexedDBIndexMetadata Database::RemoveIndexFromMetadata(
    int64_t object_store_id,
    int64_t index_id) {
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata& object_store =
      metadata_.object_stores[object_store_id];

  auto it = object_store.indexes.find(index_id);
  CHECK(it != object_store.indexes.end());
  IndexedDBIndexMetadata metadata = std::move(it->second);
  object_store.indexes.erase(it);
  return metadata;
}

Status Database::CreateObjectStoreOperation(int64_t object_store_id,
                                            const std::u16string& name,
                                            const IndexedDBKeyPath& key_path,
                                            bool auto_increment,
                                            Transaction* transaction) {
  DCHECK(transaction);
  TRACE_EVENT1("IndexedDB", "Database::CreateObjectStoreOperation", "txn.id",
               transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (base::Contains(metadata_.object_stores, object_store_id)) {
    return Status::InvalidArgument("Invalid object_store_id");
  }

  IndexedDBObjectStoreMetadata object_store_metadata;
  Status s = backing_store()->CreateObjectStore(
      transaction->BackingStoreTransaction(), id(), object_store_id, name,
      key_path, auto_increment, &object_store_metadata);

  if (!s.ok()) {
    return s;
  }

  AddObjectStoreToMetadata(std::move(object_store_metadata), object_store_id);

  transaction->ScheduleAbortTask(
      base::BindOnce(&Database::CreateObjectStoreAbortOperation, AsWeakPtr(),
                     object_store_id));
  return Status::OK();
}

void Database::CreateObjectStoreAbortOperation(int64_t object_store_id) {
  TRACE_EVENT0("IndexedDB", "Database::CreateObjectStoreAbortOperation");
  RemoveObjectStoreFromMetadata(object_store_id);
}

Status Database::DeleteObjectStoreOperation(int64_t object_store_id,
                                            Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::DeleteObjectStoreOperation", "txn.id",
               transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (!IsObjectStoreIdInMetadata(object_store_id)) {
    return Status::InvalidArgument("Invalid object_store_id.");
  }

  IndexedDBObjectStoreMetadata object_store_metadata =
      RemoveObjectStoreFromMetadata(object_store_id);

  // First remove metadata.
  Status s = backing_store()->DeleteObjectStore(
      transaction->BackingStoreTransaction(), id(), object_store_metadata);

  if (!s.ok()) {
    AddObjectStoreToMetadata(std::move(object_store_metadata),
                             IndexedDBObjectStoreMetadata::kInvalidId);
    return s;
  }

  // Then remove object store contents.
  s = backing_store()->ClearObjectStore(transaction->BackingStoreTransaction(),
                                        id(), object_store_id);

  if (!s.ok()) {
    AddObjectStoreToMetadata(std::move(object_store_metadata),
                             IndexedDBObjectStoreMetadata::kInvalidId);
    return s;
  }
  transaction->ScheduleAbortTask(
      base::BindOnce(&Database::DeleteObjectStoreAbortOperation, AsWeakPtr(),
                     std::move(object_store_metadata)));
  return s;
}

void Database::DeleteObjectStoreAbortOperation(
    IndexedDBObjectStoreMetadata object_store_metadata) {
  TRACE_EVENT0("IndexedDB", "Database::DeleteObjectStoreAbortOperation");
  AddObjectStoreToMetadata(std::move(object_store_metadata),
                           IndexedDBObjectStoreMetadata::kInvalidId);
}

Status Database::RenameObjectStoreOperation(int64_t object_store_id,
                                            const std::u16string& new_name,
                                            Transaction* transaction) {
  DCHECK(transaction);
  TRACE_EVENT1("IndexedDB", "Database::RenameObjectStore", "txn.id",
               transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (!IsObjectStoreIdInMetadata(object_store_id)) {
    return Status::InvalidArgument("Invalid object_store_id.");
  }

  // Store renaming is done synchronously, as it may be followed by
  // index creation (also sync) since preemptive OpenCursor/SetIndexKeys
  // may follow.
  IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];

  std::u16string old_name;

  Status s = backing_store()->RenameObjectStore(
      transaction->BackingStoreTransaction(), id(), new_name, &old_name,
      &object_store_metadata);

  if (!s.ok()) {
    return s;
  }
  DCHECK_EQ(object_store_metadata.name, new_name);

  transaction->ScheduleAbortTask(
      base::BindOnce(&Database::RenameObjectStoreAbortOperation, AsWeakPtr(),
                     object_store_id, std::move(old_name)));
  return Status::OK();
}

void Database::RenameObjectStoreAbortOperation(int64_t object_store_id,
                                               std::u16string old_name) {
  TRACE_EVENT0("IndexedDB", "Database::RenameObjectStoreAbortOperation");

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  metadata_.object_stores[object_store_id].name = std::move(old_name);
}

Status Database::VersionChangeOperation(int64_t version,
                                        Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::VersionChangeOperation", "txn.id",
               transaction->id());
  int64_t old_version = metadata_.version;
  DCHECK_GT(version, old_version);

  Status s = backing_store()->SetDatabaseVersion(
      transaction->BackingStoreTransaction(), id(), version, &metadata_);
  if (!s.ok()) {
    return s;
  }

  transaction->ScheduleAbortTask(base::BindOnce(
      &Database::VersionChangeAbortOperation, AsWeakPtr(), old_version));

  connection_coordinator_.BindVersionChangeTransactionReceiver();
  connection_coordinator_.OnUpgradeTransactionStarted(old_version);
  return Status::OK();
}

void Database::VersionChangeAbortOperation(int64_t previous_version) {
  TRACE_EVENT0("IndexedDB", "Database::VersionChangeAbortOperation");
  metadata_.version = previous_version;
}

Status Database::CreateIndexOperation(int64_t object_store_id,
                                      int64_t index_id,
                                      const std::u16string& name,
                                      const IndexedDBKeyPath& key_path,
                                      bool unique,
                                      bool multi_entry,
                                      Transaction* transaction) {
  DCHECK(transaction);
  TRACE_EVENT1("IndexedDB", "Database::CreateIndexOperation", "txn.id",
               transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (!IsObjectStoreIdInMetadataAndIndexNotInMetadata(object_store_id,
                                                      index_id)) {
    return Status::InvalidArgument("Invalid object_store_id and/or index_id.");
  }

  IndexedDBIndexMetadata index_metadata;
  Status s = backing_store()->CreateIndex(
      transaction->BackingStoreTransaction(), id(), object_store_id, index_id,
      name, key_path, unique, multi_entry, &index_metadata);

  if (!s.ok()) {
    return s;
  }

  AddIndexToMetadata(object_store_id, std::move(index_metadata), index_id);
  transaction->ScheduleAbortTask(
      base::BindOnce(&Database::CreateIndexAbortOperation, AsWeakPtr(),
                     object_store_id, index_id));
  return s;
}

void Database::CreateIndexAbortOperation(int64_t object_store_id,
                                         int64_t index_id) {
  TRACE_EVENT0("IndexedDB", "Database::CreateIndexAbortOperation");
  RemoveIndexFromMetadata(object_store_id, index_id);
}

Status Database::DeleteIndexOperation(int64_t object_store_id,
                                      int64_t index_id,
                                      Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::DeleteIndexOperation", "txn.id",
               transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (!IsObjectStoreIdAndIndexIdInMetadata(object_store_id, index_id)) {
    return Status::InvalidArgument("Invalid object_store_id and/or index_id.");
  }

  IndexedDBIndexMetadata index_metadata =
      RemoveIndexFromMetadata(object_store_id, index_id);

  Status s =
      backing_store()->DeleteIndex(transaction->BackingStoreTransaction(), id(),
                                   object_store_id, index_metadata);

  if (!s.ok()) {
    return s;
  }

  s = backing_store()->ClearIndex(transaction->BackingStoreTransaction(), id(),
                                  object_store_id, index_id);
  if (!s.ok()) {
    AddIndexToMetadata(object_store_id, std::move(index_metadata),
                       IndexedDBIndexMetadata::kInvalidId);
    return s;
  }

  transaction->ScheduleAbortTask(
      base::BindOnce(&Database::DeleteIndexAbortOperation, AsWeakPtr(),
                     object_store_id, std::move(index_metadata)));
  return s;
}

void Database::DeleteIndexAbortOperation(
    int64_t object_store_id,
    IndexedDBIndexMetadata index_metadata) {
  TRACE_EVENT0("IndexedDB", "Database::DeleteIndexAbortOperation");
  AddIndexToMetadata(object_store_id, std::move(index_metadata),
                     IndexedDBIndexMetadata::kInvalidId);
}

Status Database::RenameIndexOperation(int64_t object_store_id,
                                      int64_t index_id,
                                      const std::u16string& new_name,
                                      Transaction* transaction) {
  DCHECK(transaction);
  TRACE_EVENT1("IndexedDB", "Database::RenameIndex", "txn.id",
               transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (!IsObjectStoreIdAndIndexIdInMetadata(object_store_id, index_id)) {
    return Status::InvalidArgument("Invalid object_store_id and/or index_id.");
  }

  IndexedDBIndexMetadata& index_metadata =
      metadata_.object_stores[object_store_id].indexes[index_id];

  std::u16string old_name;
  Status s = backing_store()->RenameIndex(
      transaction->BackingStoreTransaction(), id(), object_store_id, new_name,
      &old_name, &index_metadata);
  if (!s.ok()) {
    return s;
  }

  DCHECK_EQ(index_metadata.name, new_name);
  transaction->ScheduleAbortTask(
      base::BindOnce(&Database::RenameIndexAbortOperation, AsWeakPtr(),
                     object_store_id, index_id, std::move(old_name)));
  return Status::OK();
}

void Database::RenameIndexAbortOperation(int64_t object_store_id,
                                         int64_t index_id,
                                         std::u16string old_name) {
  TRACE_EVENT0("IndexedDB", "Database::RenameIndexAbortOperation");

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata& object_store =
      metadata_.object_stores[object_store_id];

  DCHECK(object_store.indexes.find(index_id) != object_store.indexes.end());
  object_store.indexes[index_id].name = std::move(old_name);
}

Status Database::GetOperation(int64_t object_store_id,
                              int64_t index_id,
                              std::unique_ptr<IndexedDBKeyRange> key_range,
                              CursorType cursor_type,
                              blink::mojom::IDBDatabase::GetCallback callback,
                              Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::GetOperation", "txn.id",
               transaction->id());

  if (!IsObjectStoreIdAndMaybeIndexIdInMetadata(object_store_id, index_id)) {
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        CreateIDBErrorPtr(blink::mojom::IDBException::kUnknownError,
                          "Bad request", transaction)));
    return Status::InvalidArgument("Invalid object_store_id and/or index_id.");
  }

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];

  const IndexedDBKey* key;

  Status s = Status::OK();
  std::unique_ptr<BackingStore::Cursor> backing_store_cursor;
  if (key_range->IsOnlyKey()) {
    key = &key_range->lower();
  } else {
    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      // ObjectStore Retrieval Operation
      if (cursor_type == CursorType::kKeyOnly) {
        backing_store_cursor = backing_store()->OpenObjectStoreKeyCursor(
            transaction->BackingStoreTransaction(), id(), object_store_id,
            *key_range, blink::mojom::IDBCursorDirection::Next, &s);
      } else {
        backing_store_cursor = backing_store()->OpenObjectStoreCursor(
            transaction->BackingStoreTransaction(), id(), object_store_id,
            *key_range, blink::mojom::IDBCursorDirection::Next, &s);
      }
    } else if (cursor_type == CursorType::kKeyOnly) {
      // Index Value Retrieval Operation
      backing_store_cursor = backing_store()->OpenIndexKeyCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          index_id, *key_range, blink::mojom::IDBCursorDirection::Next, &s);
    } else {
      // Index Referenced Value Retrieval Operation
      backing_store_cursor = backing_store()->OpenIndexCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          index_id, *key_range, blink::mojom::IDBCursorDirection::Next, &s);
    }

    if (!s.ok()) {
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewErrorResult(CreateIDBErrorPtr(
              blink::mojom::IDBException::kUnknownError,
              "Corruption detected, unable to continue", transaction)));
      return s;
    }

    if (!backing_store_cursor) {
      // This means we've run out of data.
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewEmpty(true));
      return s;
    }

    key = &backing_store_cursor->key();
  }

  if (index_id == IndexedDBIndexMetadata::kInvalidId) {
    // Object Store Retrieval Operation
    IndexedDBReturnValue value;
    s = backing_store()->GetRecord(transaction->BackingStoreTransaction(), id(),
                                   object_store_id, *key, &value);
    if (!s.ok()) {
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewErrorResult(
              CreateIDBErrorPtr(blink::mojom::IDBException::kUnknownError,
                                "Unknown error", transaction)));
      return s;
    }

    if (value.empty()) {
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewEmpty(true));
      return s;
    }

    if (cursor_type == CursorType::kKeyOnly) {
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewKey(std::move(*key)));
      return s;
    }

    if (object_store_metadata.auto_increment &&
        !object_store_metadata.key_path.IsNull()) {
      value.primary_key = *key;
      value.key_path = object_store_metadata.key_path;
    }

    blink::mojom::IDBReturnValuePtr mojo_value =
        IndexedDBReturnValue::ConvertReturnValue(&value);
    bucket_context_->CreateAllExternalObjects(
        value.external_objects, &mojo_value->value->external_objects);
    std::move(callback).Run(
        blink::mojom::IDBDatabaseGetResult::NewValue(std::move(mojo_value)));
    return s;
  }

  // From here we are dealing only with indexes.
  std::unique_ptr<IndexedDBKey> primary_key;
  s = backing_store()->GetPrimaryKeyViaIndex(
      transaction->BackingStoreTransaction(), id(), object_store_id, index_id,
      *key, &primary_key);
  if (!s.ok()) {
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        CreateIDBErrorPtr(blink::mojom::IDBException::kUnknownError,
                          "Unknown error", transaction)));
    return s;
  }

  if (!primary_key) {
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewEmpty(true));
    return s;
  }
  if (cursor_type == CursorType::kKeyOnly) {
    // Index Value Retrieval Operation
    std::move(callback).Run(
        blink::mojom::IDBDatabaseGetResult::NewKey(std::move(*primary_key)));
    return s;
  }

  // Index Referenced Value Retrieval Operation
  IndexedDBReturnValue value;
  s = backing_store()->GetRecord(transaction->BackingStoreTransaction(), id(),
                                 object_store_id, *primary_key, &value);
  if (!s.ok()) {
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        CreateIDBErrorPtr(blink::mojom::IDBException::kUnknownError,
                          "Unknown error", transaction)));
    return s;
  }

  if (value.empty()) {
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewEmpty(true));
    return s;
  }
  if (object_store_metadata.auto_increment &&
      !object_store_metadata.key_path.IsNull()) {
    value.primary_key = *primary_key;
    value.key_path = object_store_metadata.key_path;
  }

  blink::mojom::IDBReturnValuePtr mojo_value =
      IndexedDBReturnValue::ConvertReturnValue(&value);
  bucket_context_->CreateAllExternalObjects(
      value.external_objects, &mojo_value->value->external_objects);
  std::move(callback).Run(
      blink::mojom::IDBDatabaseGetResult::NewValue(std::move(mojo_value)));
  return s;
}

Transaction::Operation Database::CreateGetAllOperation(
    int64_t object_store_id,
    int64_t index_id,
    std::unique_ptr<blink::IndexedDBKeyRange> key_range,
    blink::mojom::IDBGetAllResultType result_type,
    int64_t max_count,
    blink::mojom::IDBCursorDirection direction,
    blink::mojom::IDBDatabase::GetAllCallback callback,
    Transaction* transaction) {
  return BindWeakOperation(&Database::GetAllOperation, AsWeakPtr(),
                           object_store_id, index_id, std::move(key_range),
                           result_type, max_count, direction,
                           std::make_unique<GetAllResultSinkWrapper>(
                               transaction->AsWeakPtr(), std::move(callback)));
}

static_assert(sizeof(size_t) >= sizeof(int32_t),
              "Size of size_t is less than size of int32");
static_assert(blink::mojom::kIDBMaxMessageOverhead <= INT32_MAX,
              "kIDBMaxMessageOverhead is more than INT32_MAX");

Database::GetAllResultSinkWrapper::GetAllResultSinkWrapper(
    base::WeakPtr<Transaction> transaction,
    blink::mojom::IDBDatabase::GetAllCallback callback)
    : transaction_(transaction), callback_(std::move(callback)) {}

Database::GetAllResultSinkWrapper::~GetAllResultSinkWrapper() {
  if (!callback_) {
    return;
  }

  if (transaction_) {
    transaction_->IncrementNumErrorsSent();
    // If we're reaching this line because the Connection has been
    // disconnected from its remote, then `result_sink_` won't have been
    // successfully associated, and invoking any methods on it will CHECK.
    // See crbug.com/346955148.
    // TODO(crbug.com/347047640): remove this workaround when 347047640 is
    // fixed.
    if (!transaction_->connection()->is_shutting_down()) {
      DatabaseError error(blink::mojom::IDBException::kIgnorableAbortError,
                          "Backend aborted error");
      Get()->OnError(
          blink::mojom::IDBError::New(error.code(), error.message()));
    }
  } else {
    // Make sure `callback_` is invoked because the Mojo client is waiting for a
    // response.
    Get();
  }
}

mojo::AssociatedRemote<blink::mojom::IDBDatabaseGetAllResultSink>&
Database::GetAllResultSinkWrapper::Get() {
  if (!result_sink_) {
    mojo::PendingAssociatedReceiver<blink::mojom::IDBDatabaseGetAllResultSink>
        pending_receiver;
    if (use_dedicated_receiver_for_testing_) {
      pending_receiver = result_sink_.BindNewEndpointAndPassDedicatedReceiver();
    } else {
      pending_receiver = result_sink_.BindNewEndpointAndPassReceiver();
    }
    std::move(callback_).Run(std::move(pending_receiver));
  }
  return result_sink_;
}

Status Database::GetAllOperation(
    int64_t object_store_id,
    int64_t index_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    blink::mojom::IDBGetAllResultType result_type,
    int64_t max_count,
    blink::mojom::IDBCursorDirection direction,
    std::unique_ptr<GetAllResultSinkWrapper> result_sink,
    Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::GetAllOperation", "txn.id",
               transaction->id());

  if (!IsObjectStoreIdAndMaybeIndexIdInMetadata(object_store_id, index_id)) {
    result_sink->Get()->OnError(CreateIDBErrorPtr(
        blink::mojom::IDBException::kUnknownError, "Bad request", transaction));
    return Status::InvalidArgument("Invalid object_store_id.");
  }

  DCHECK_GT(max_count, 0);

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];

  Status s = Status::OK();
  std::unique_ptr<BackingStore::Cursor> cursor;

  if (result_type == blink::mojom::IDBGetAllResultType::Keys) {
    // Retrieving keys
    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      // Object Store: Key Retrieval Operation
      cursor = backing_store()->OpenObjectStoreKeyCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          *key_range, direction, &s);
    } else {
      // Index Value: (Primary Key) Retrieval Operation
      cursor = backing_store()->OpenIndexKeyCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          index_id, *key_range, direction, &s);
    }
  } else {
    // Retrieving values
    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      // Object Store: Value Retrieval Operation
      cursor = backing_store()->OpenObjectStoreCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          *key_range, direction, &s);
    } else {
      // Object Store: Referenced Value Retrieval Operation
      cursor = backing_store()->OpenIndexCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          index_id, *key_range, direction, &s);
    }
  }

  if (!s.ok()) {
    DLOG(ERROR) << "Unable to open cursor operation: " << s.ToString();
    result_sink->Get()->OnError(CreateIDBErrorPtr(
        blink::mojom::IDBException::kUnknownError,
        "Corruption detected, unable to continue", transaction));
    return s;
  }

  std::vector<blink::mojom::IDBRecordPtr> found_records;

  auto send_records = [&](bool done) {
    result_sink->Get()->ReceiveResults(std::move(found_records), done);
    found_records.clear();
  };

  // No records found.
  if (!cursor) {
    send_records(/*done=*/true);
    return s;
  }

  bool did_first_seek = false;

  // Max idbvalue size before blob wrapping is 64k, so make an assumption
  // that max key/value size is 128kb tops, to fit under 128mb mojo limit.
  // This value is just a heuristic and is an attempt to make sure that
  // GetAll fits under the message limit size.
  static_assert(
      blink::mojom::kIDBMaxMessageSize >
          blink::mojom::kIDBGetAllChunkSize * blink::mojom::kIDBWrapThreshold,
      "Chunk heuristic too large");

  const size_t max_values_before_sending = blink::mojom::kIDBGetAllChunkSize;
  int64_t num_found_items = 0;
  while (num_found_items++ < max_count) {
    bool cursor_valid;
    if (did_first_seek) {
      cursor_valid = cursor->Continue(&s);
    } else {
      // Cursor creation performs the first seek, returning a nullptr cursor
      // when invalid.
      cursor_valid = true;
      did_first_seek = true;
    }
    if (!s.ok()) {
      result_sink->Get()->OnError(
          CreateIDBErrorPtr(blink::mojom::IDBException::kUnknownError,
                            "Seek failure, unable to continue", transaction));
      return s;
    }

    if (!cursor_valid) {
      break;
    }

    blink::mojom::IDBRecordPtr return_record;

    if (result_type == blink::mojom::IDBGetAllResultType::Keys) {
      return_record = blink::mojom::IDBRecord::New(cursor->primary_key(),
                                                   /*value=*/nullptr,
                                                   /*index_key=*/std::nullopt);
    } else if (result_type == blink::mojom::IDBGetAllResultType::Values) {
      blink::mojom::IDBReturnValuePtr return_value =
          ExtractReturnValueFromCursorValue(bucket_context_.get(),
                                            object_store_metadata, *cursor);
      return_record = blink::mojom::IDBRecord::New(
          /*primary_key=*/std::nullopt, std::move(return_value),
          /*index_key=*/std::nullopt);
    } else if (result_type == blink::mojom::IDBGetAllResultType::Records) {
      // Construct the record, which includes the primary key, value and index
      // key.
      blink::mojom::IDBReturnValuePtr return_value =
          ExtractReturnValueFromCursorValue(bucket_context_.get(),
                                            object_store_metadata, *cursor);
      std::optional<IndexedDBKey> index_key;
      if (index_id != IndexedDBIndexMetadata::kInvalidId) {
        // The index key only exists for `IDBIndex::getAllRecords()`.
        index_key = cursor->key();
      }
      return_record = blink::mojom::IDBRecord::New(
          cursor->primary_key(), std::move(return_value), std::move(index_key));
    } else {
      NOTREACHED();
    }

    found_records.push_back(std::move(return_record));

    // Periodically stream records if we have too many.
    if (found_records.size() >= max_values_before_sending) {
      send_records(/*done=*/false);
    }
  }
  send_records(/*done=*/true);
  return s;
}

Status Database::PutOperation(std::unique_ptr<PutOperationParams> params,
                              Transaction* transaction) {
  TRACE_EVENT2("IndexedDB", "Database::PutOperation", "txn.id",
               transaction->id(), "size", params->value.SizeEstimate());
  DCHECK_NE(transaction->mode(), blink::mojom::IDBTransactionMode::ReadOnly);
  bool key_was_generated = false;
  Status s = Status::OK();
  transaction->in_flight_memory() -= params->value.SizeEstimate();
  DCHECK(transaction->in_flight_memory().IsValid());

  if (!IsObjectStoreIdInMetadata(params->object_store_id)) {
    std::move(params->callback)
        .Run(blink::mojom::IDBTransactionPutResult::NewErrorResult(
            CreateIDBErrorPtr(blink::mojom::IDBException::kUnknownError,
                              "Bad request", transaction)));
    return Status::InvalidArgument("Invalid object_store_id.");
  }

  DCHECK(metadata_.object_stores.find(params->object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store =
      metadata_.object_stores[params->object_store_id];
  DCHECK(object_store.auto_increment || params->key->IsValid());

  std::unique_ptr<IndexedDBKey> key;
  if (params->put_mode != blink::mojom::IDBPutMode::CursorUpdate &&
      object_store.auto_increment && !params->key->IsValid()) {
    std::unique_ptr<IndexedDBKey> auto_inc_key = GenerateKey(
        backing_store(), transaction, id(), params->object_store_id);
    key_was_generated = true;
    if (!auto_inc_key->IsValid()) {
      std::move(params->callback)
          .Run(blink::mojom::IDBTransactionPutResult::NewErrorResult(
              CreateIDBErrorPtr(blink::mojom::IDBException::kConstraintError,
                                "Maximum key generator value reached.",
                                transaction)));
      return s;
    }
    key = std::move(auto_inc_key);
  } else {
    key = std::move(params->key);
  }

  if (!key->IsValid()) {
    return Status::InvalidArgument("Invalid key");
  }

  BackingStore::RecordIdentifier record_identifier;
  if (params->put_mode == blink::mojom::IDBPutMode::AddOnly) {
    bool found = false;
    Status found_status = backing_store()->KeyExistsInObjectStore(
        transaction->BackingStoreTransaction(), id(), params->object_store_id,
        *key, &record_identifier, &found);
    if (!found_status.ok()) {
      return found_status;
    }
    if (found) {
      std::move(params->callback)
          .Run(blink::mojom::IDBTransactionPutResult::NewErrorResult(
              CreateIDBErrorPtr(blink::mojom::IDBException::kConstraintError,
                                "Key already exists in the object store.",
                                transaction)));
      return found_status;
    }
  }

  std::vector<std::unique_ptr<IndexWriter>> index_writers;
  std::string error_message;
  bool obeys_constraints = false;
  bool backing_store_success = MakeIndexWriters(
      transaction, backing_store(), id(), object_store, *key, key_was_generated,
      params->index_keys, &index_writers, &error_message, &obeys_constraints);
  if (!backing_store_success) {
    std::move(params->callback)
        .Run(blink::mojom::IDBTransactionPutResult::NewErrorResult(
            CreateIDBErrorPtr(
                blink::mojom::IDBException::kUnknownError,
                "Internal error: backing store error updating index keys.",
                transaction)));
    return s;
  }
  if (!obeys_constraints) {
    std::move(params->callback)
        .Run(blink::mojom::IDBTransactionPutResult::NewErrorResult(
            CreateIDBErrorPtr(blink::mojom::IDBException::kConstraintError,
                              error_message, transaction)));
    return s;
  }

  // Before this point, don't do any mutation. After this point, rollback the
  // transaction in case of error.
  s = backing_store()->PutRecord(transaction->BackingStoreTransaction(), id(),
                                 params->object_store_id, *key, &params->value,
                                 &record_identifier);
  if (!s.ok()) {
    return s;
  }

  {
    TRACE_EVENT1("IndexedDB", "Database::PutOperation.UpdateIndexes", "txn.id",
                 transaction->id());
    for (const auto& writer : index_writers) {
      writer->WriteIndexKeys(record_identifier, backing_store(),
                             transaction->BackingStoreTransaction(), id(),
                             params->object_store_id);
    }
  }

  if (object_store.auto_increment &&
      params->put_mode != blink::mojom::IDBPutMode::CursorUpdate &&
      key->type() == blink::mojom::IDBKeyType::Number) {
    TRACE_EVENT1("IndexedDB", "Database::PutOperation.AutoIncrement", "txn.id",
                 transaction->id());
    s = UpdateKeyGenerator(backing_store(), transaction, id(),
                           params->object_store_id, *key, !key_was_generated);
    if (!s.ok()) {
      return s;
    }
  }
  {
    TRACE_EVENT1("IndexedDB", "Database::PutOperation.Callbacks", "txn.id",
                 transaction->id());
    std::move(params->callback)
        .Run(blink::mojom::IDBTransactionPutResult::NewKey(*key));
  }
  bucket_context_->delegate().on_content_changed.Run(
      metadata_.name, metadata_.object_stores[params->object_store_id].name);
  return s;
}

Status Database::SetIndexKeysOperation(
    int64_t object_store_id,
    std::unique_ptr<IndexedDBKey> primary_key,
    const std::vector<IndexedDBIndexKeys>& index_keys,
    Transaction* transaction) {
  DCHECK(transaction);
  TRACE_EVENT1("IndexedDB", "Database::SetIndexKeysOperation", "txn.id",
               transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  BackingStore::RecordIdentifier record_identifier;
  bool found = false;
  Status s = backing_store()->KeyExistsInObjectStore(
      transaction->BackingStoreTransaction(), metadata_.id, object_store_id,
      *primary_key, &record_identifier, &found);
  if (!s.ok()) {
    return s;
  }
  if (!found) {
    return transaction->Abort(
        DatabaseError(blink::mojom::IDBException::kUnknownError,
                      "Internal error setting index keys for object store."));
  }

  std::vector<std::unique_ptr<IndexWriter>> index_writers;
  std::string error_message;
  bool obeys_constraints = false;
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];
  bool backing_store_success = MakeIndexWriters(
      transaction, backing_store(), id(), object_store_metadata, *primary_key,
      false, index_keys, &index_writers, &error_message, &obeys_constraints);
  if (!backing_store_success) {
    return transaction->Abort(DatabaseError(
        blink::mojom::IDBException::kUnknownError,
        "Internal error: backing store error updating index keys."));
  }
  if (!obeys_constraints) {
    return transaction->Abort(DatabaseError(
        blink::mojom::IDBException::kConstraintError, error_message));
  }

  for (const auto& writer : index_writers) {
    s = writer->WriteIndexKeys(record_identifier, backing_store(),
                               transaction->BackingStoreTransaction(), id(),
                               object_store_id);
    if (!s.ok()) {
      return s;
    }
  }
  return Status::OK();
}

Status Database::SetIndexesReadyOperation(size_t index_count,
                                          Transaction* transaction) {
  // TODO(dmurph): This method should be refactored out for something more
  // reliable.
  for (size_t i = 0; i < index_count; ++i) {
    transaction->DidCompletePreemptiveEvent();
  }
  return Status::OK();
}

Status Database::OpenCursorOperation(
    std::unique_ptr<OpenCursorOperationParams> params,
    const storage::BucketLocator& bucket_locator,
    Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::OpenCursorOperation", "txn.id",
               transaction->id());

  if (!IsObjectStoreIdAndMaybeIndexIdInMetadata(params->object_store_id,
                                                params->index_id)) {
    return Status::InvalidArgument("Invalid object_store_id and/or index_id.");
  }

  // The frontend has begun indexing, so this pauses the transaction
  // until the indexing is complete. This can't happen any earlier
  // because we don't want to switch to early mode in case multiple
  // indexes are being created in a row, with Put()'s in between.
  if (params->task_type == blink::mojom::IDBTaskType::Preemptive) {
    transaction->AddPreemptiveEvent();
  }

  Status s;
  std::unique_ptr<BackingStore::Cursor> backing_store_cursor;
  if (params->index_id == IndexedDBIndexMetadata::kInvalidId) {
    if (params->cursor_type == CursorType::kKeyOnly) {
      DCHECK_EQ(params->task_type, blink::mojom::IDBTaskType::Normal);
      backing_store_cursor = backing_store()->OpenObjectStoreKeyCursor(
          transaction->BackingStoreTransaction(), id(), params->object_store_id,
          *params->key_range, params->direction, &s);
    } else {
      backing_store_cursor = backing_store()->OpenObjectStoreCursor(
          transaction->BackingStoreTransaction(), id(), params->object_store_id,
          *params->key_range, params->direction, &s);
    }
  } else {
    DCHECK_EQ(params->task_type, blink::mojom::IDBTaskType::Normal);
    if (params->cursor_type == CursorType::kKeyOnly) {
      backing_store_cursor = backing_store()->OpenIndexKeyCursor(
          transaction->BackingStoreTransaction(), id(), params->object_store_id,
          params->index_id, *params->key_range, params->direction, &s);
    } else {
      backing_store_cursor = backing_store()->OpenIndexCursor(
          transaction->BackingStoreTransaction(), id(), params->object_store_id,
          params->index_id, *params->key_range, params->direction, &s);
    }
  }

  if (!s.ok()) {
    DLOG(ERROR) << "Unable to open cursor operation: " << s.ToString();
    return s;
  }

  if (!backing_store_cursor) {
    // Occurs when we've reached the end of cursor's data.
    std::move(params->callback)
        .Run(blink::mojom::IDBDatabaseOpenCursorResult::NewEmpty(true));
    return s;
  }

  mojo::PendingAssociatedRemote<blink::mojom::IDBCursor> pending_remote;
  Cursor* cursor = Cursor::CreateAndBind(
      std::move(backing_store_cursor), params->cursor_type, params->task_type,
      transaction->AsWeakPtr(), pending_remote);
  transaction->RegisterOpenCursor(cursor);

  blink::mojom::IDBValuePtr mojo_value;
  std::vector<IndexedDBExternalObject> external_objects;
  if (cursor->Value()) {
    mojo_value = IndexedDBValue::ConvertAndEraseValue(cursor->Value());
    external_objects.swap(cursor->Value()->external_objects);
  }

  if (mojo_value) {
    bucket_context_->CreateAllExternalObjects(external_objects,
                                              &mojo_value->external_objects);
  }

  std::move(params->callback)
      .Run(blink::mojom::IDBDatabaseOpenCursorResult::NewValue(
          blink::mojom::IDBDatabaseOpenCursorValue::New(
              std::move(pending_remote), cursor->key(), cursor->primary_key(),
              std::move(mojo_value))));
  return s;
}

Status Database::CountOperation(
    int64_t object_store_id,
    int64_t index_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    blink::mojom::IDBDatabase::CountCallback callback,
    Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::CountOperation", "txn.id",
               transaction->id());

  if (!IsObjectStoreIdAndMaybeIndexIdInMetadata(object_store_id, index_id)) {
    return Status::InvalidArgument("Invalid object_store_id and/or index_id.");
  }

  uint32_t count = 0;
  std::unique_ptr<BackingStore::Cursor> backing_store_cursor;

  Status s = Status::OK();
  if (index_id == IndexedDBIndexMetadata::kInvalidId) {
    backing_store_cursor = backing_store()->OpenObjectStoreKeyCursor(
        transaction->BackingStoreTransaction(), id(), object_store_id,
        *key_range, blink::mojom::IDBCursorDirection::Next, &s);
  } else {
    backing_store_cursor = backing_store()->OpenIndexKeyCursor(
        transaction->BackingStoreTransaction(), id(), object_store_id, index_id,
        *key_range, blink::mojom::IDBCursorDirection::Next, &s);
  }
  if (!s.ok()) {
    DLOG(ERROR) << "Unable perform count operation: " << s.ToString();
    return s;
  }

  if (backing_store_cursor) {
    do {
      if (!s.ok()) {
        return s;
      }
      ++count;
    } while (backing_store_cursor->Continue(&s));
  }

  std::move(callback).Run(/*success=*/true, count);
  return s;
}

Status Database::DeleteRangeOperation(
    int64_t object_store_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    blink::mojom::IDBDatabase::DeleteRangeCallback success_callback,
    Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::DeleteRangeOperation", "txn.id",
               transaction->id());

  Status s;
  if (IsObjectStoreIdInMetadata(object_store_id)) {
    s = backing_store()->DeleteRange(transaction->BackingStoreTransaction(),
                                     id(), object_store_id, *key_range);
  } else {
    s = Status::InvalidArgument("Invalid object_store_id.");
  }
  if (s.ok()) {
    bucket_context_->delegate().on_content_changed.Run(
        metadata_.name, metadata_.object_stores[object_store_id].name);
  }
  std::move(success_callback).Run(s.ok());
  return s;
}

Status Database::GetKeyGeneratorCurrentNumberOperation(
    int64_t object_store_id,
    blink::mojom::IDBDatabase::GetKeyGeneratorCurrentNumberCallback callback,
    Transaction* transaction) {
  if (!IsObjectStoreIdInMetadata(object_store_id)) {
    std::move(callback).Run(
        -1, CreateIDBErrorPtr(blink::mojom::IDBException::kDataError,
                              "Object store id not valid.", transaction));
    return Status::InvalidArgument("Invalid object_store_id.");
  }

  int64_t current_number;
  Status s = backing_store()->GetKeyGeneratorCurrentNumber(
      transaction->BackingStoreTransaction(), id(), object_store_id,
      &current_number);
  if (!s.ok()) {
    std::move(callback).Run(
        -1,
        CreateIDBErrorPtr(blink::mojom::IDBException::kDataError,
                          "Failed to get the current number of key generator.",
                          transaction));
    return s;
  }
  std::move(callback).Run(current_number, nullptr);
  return s;
}

Status Database::ClearOperation(
    int64_t object_store_id,
    blink::mojom::IDBDatabase::ClearCallback success_callback,
    Transaction* transaction) {
  TRACE_EVENT1("IndexedDB", "Database::ClearOperation", "txn.id",
               transaction->id());
  Status s = Status::InvalidArgument("Invalid object_store_id.");
  if (IsObjectStoreIdInMetadata(object_store_id)) {
    s = backing_store()->ClearObjectStore(
        transaction->BackingStoreTransaction(), id(), object_store_id);
  }
  if (s.ok()) {
    bucket_context_->delegate().on_content_changed.Run(
        metadata_.name, metadata_.object_stores[object_store_id].name);
  }
  std::move(success_callback).Run(s.ok());
  return s;
}

bool Database::IsObjectStoreIdInMetadata(int64_t object_store_id) const {
  if (!base::Contains(metadata_.object_stores, object_store_id)) {
    DLOG(ERROR) << "Invalid object_store_id";
    return false;
  }
  return true;
}

bool Database::IsObjectStoreIdAndIndexIdInMetadata(int64_t object_store_id,
                                                   int64_t index_id) const {
  if (!IsObjectStoreIdInMetadata(object_store_id)) {
    return false;
  }
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores.find(object_store_id)->second;
  if (!base::Contains(object_store_metadata.indexes, index_id)) {
    DLOG(ERROR) << "Invalid index_id";
    return false;
  }
  return true;
}

bool Database::IsObjectStoreIdAndMaybeIndexIdInMetadata(
    int64_t object_store_id,
    int64_t index_id) const {
  if (!IsObjectStoreIdInMetadata(object_store_id)) {
    return false;
  }
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores.find(object_store_id)->second;
  if (index_id != IndexedDBIndexMetadata::kInvalidId &&
      !base::Contains(object_store_metadata.indexes, index_id)) {
    DLOG(ERROR) << "Invalid index_id";
    return false;
  }
  return true;
}

bool Database::IsObjectStoreIdInMetadataAndIndexNotInMetadata(
    int64_t object_store_id,
    int64_t index_id) const {
  if (!IsObjectStoreIdInMetadata(object_store_id)) {
    return false;
  }
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores.find(object_store_id)->second;
  if (base::Contains(object_store_metadata.indexes, index_id)) {
    DLOG(ERROR) << "Invalid index_id";
    return false;
  }
  return true;
}

storage::mojom::IdbDatabaseMetadataPtr Database::GetIdbInternalsMetadata()
    const {
  storage::mojom::IdbDatabaseMetadataPtr info =
      storage::mojom::IdbDatabaseMetadata::New();
  info->name = name();
  info->connection_count = ConnectionCount();
  info->active_open_delete = ActiveOpenDeleteCount();
  info->pending_open_delete = PendingOpenDeleteCount();
  for (const Connection* connection : connections()) {
    for (const auto& [_, transaction] : connection->transactions()) {
      info->transactions.push_back(transaction->GetIdbInternalsMetadata());
    }
  }
  return info;
}

void Database::NotifyOfIdbInternalsRelevantChange() {
  // This metadata is included in the context metadata, so call up the chain.
  bucket_context_->NotifyOfIdbInternalsRelevantChange();
}

// kIDBMaxMessageSize is defined based on the original
// IPC::Channel::kMaximumMessageSize value.  We use kIDBMaxMessageSize to
// limit the size of arguments we pass into our Mojo calls.  We want to ensure
// this value is always no bigger than the current kMaximumMessageSize value
// which also ensures it is always no bigger than the current Mojo message
// size limit.
static_assert(
    blink::mojom::kIDBMaxMessageSize <= IPC::Channel::kMaximumMessageSize,
    "kIDBMaxMessageSize is bigger than IPC::Channel::kMaximumMessageSize");

void Database::CallUpgradeTransactionStartedForTesting(int64_t old_version) {
  connection_coordinator_.OnUpgradeTransactionStarted(old_version);
}

Status Database::OpenInternal() {
  bool found = false;
  Status s = backing_store()->ReadMetadataForDatabaseName(metadata_.name,
                                                          &metadata_, &found);
  DCHECK(found == (metadata_.id != kInvalidId))
      << "found = " << found << " id = " << metadata_.id;
  if (!s.ok() || found) {
    return s;
  }

  return backing_store()->CreateDatabase(metadata_);
}

std::unique_ptr<Connection> Database::CreateConnection(
    std::unique_ptr<DatabaseCallbacks> database_callbacks,
    mojo::Remote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker,
    base::UnguessableToken client_token,
    int scheduling_priority) {
  auto connection = std::make_unique<Connection>(
      *bucket_context_, weak_factory_.GetWeakPtr(),
      base::BindRepeating(&Database::VersionChangeIgnored,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(&Database::ConnectionClosed, weak_factory_.GetWeakPtr()),
      std::move(database_callbacks), std::move(client_state_checker),
      client_token, scheduling_priority);
  connections_.insert(connection.get());
  bucket_context_->OnConnectionPriorityUpdated();
  return connection;
}

void Database::VersionChangeIgnored() {
  connection_coordinator_.OnVersionChangeIgnored();
}

bool Database::HasNoConnections() const {
  return force_closing_ || connections().empty();
}

void Database::SendVersionChangeToAllConnections(int64_t old_version,
                                                 int64_t new_version) {
  if (force_closing_) {
    return;
  }
  for (auto* connection : connections()) {
    // Before invoking this method, the `ConnectionCoordinator` had
    // set the request state to `kPendingNoConnections`. Now the request will
    // be blocked until all the existing connections to this database is
    // closed. There are three possible ways for the connection to be closed:
    // 1. If the client is already pending close, then the `VersionChange`
    // event will be ignored and the open request will be deemed blocked until
    // the pending close completes.
    // 2. If the client is active, the `VersionChange` event will be enqueued
    // and the registered event listener will be fired asynchronously. The
    // event listener should be responsible for actively closing the IndexedDB
    // connection. The document won't be eligible for BFCache before the
    // connection is closed if it receives the `versionchange` event.
    // 3. While the above two cases rely on the `VersionChange` event to be
    // delivered to the renderer process, the third case happens purely from
    // the IndexedDB/browser context. If the client is inactive, the
    // `VersionChange` event will not be delivered, instead, a mojo call is
    // sent to the browser process to disallow the activation of the inactive
    // client, which will close the connection as part of the destruction. No
    // matter which path it follows, the `SendVersionChangeToAllConnections`
    // method is executed asynchronously.
    connection->DisallowInactiveClient(
        storage::mojom::DisallowInactiveClientReason::kVersionChangeEvent,
        base::BindOnce(
            [](base::WeakPtr<Connection> connection, int64_t old_version,
               int64_t new_version, bool was_client_active) {
              if (connection && connection->IsConnected() &&
                  was_client_active) {
                connection->callbacks()->OnVersionChange(old_version,
                                                         new_version);
              }
            },
            connection->GetWeakPtr(), old_version, new_version));
  }
}

void Database::ConnectionClosed(Connection* connection) {
  TRACE_EVENT0("IndexedDB", "Database::ConnectionClosed");
  // Ignore connection closes during force close to prevent re-entry.
  if (force_closing_) {
    return;
  }
  connections_.erase(connection);
  bucket_context_->OnConnectionPriorityUpdated();
  connection_coordinator_.OnConnectionClosed(connection);
  if (connections_.empty()) {
    connection_coordinator_.OnNoConnections();
  }
  if (CanBeDestroyed()) {
    bucket_context_->QueueRunTasks();
  }
}

bool Database::CanBeDestroyed() {
  return !connection_coordinator_.HasTasks() && connections_.empty();
}

}  // namespace content::indexed_db
