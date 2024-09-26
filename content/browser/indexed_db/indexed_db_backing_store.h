// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BACKING_STORE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BACKING_STORE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/dcheck_is_on.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom-forward.h"
#include "components/services/storage/public/mojom/file_system_access_context.mojom-forward.h"
#include "content/browser/indexed_db/indexed_db.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/browser/indexed_db/indexed_db_external_object_storage.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/common/content_export.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace blink {
class IndexedDBKeyRange;
struct IndexedDBDatabaseMetadata;
}  // namespace blink

namespace content {
class AutoDidCommitTransaction;
class IndexedDBBucketState;
class IndexedDBActiveBlobRegistry;
class LevelDBWriteBatch;
class TransactionalLevelDBDatabase;
class TransactionalLevelDBFactory;
class TransactionalLevelDBIterator;
class TransactionalLevelDBTransaction;
struct IndexedDBValue;

namespace indexed_db_backing_store_unittest {
class IndexedDBBackingStoreTest;
FORWARD_DECLARE_TEST(IndexedDBBackingStoreTest, ReadCorruptionInfo);
}  // namespace indexed_db_backing_store_unittest

enum class V2SchemaCorruptionStatus {
  kUnknown = 0,  // Due to other unknown/critical errors.
  kNo = 1,
  kYes = 2,
};

// This class is not thread-safe.
// All accessses to one instance must occur on the same sequence. Currently,
// this must be the IndexedDB task runner's sequence.
class CONTENT_EXPORT IndexedDBBackingStore {
 public:
  // This class is not thread-safe.
  // All accessses to one instance must occur on the same sequence.
  class CONTENT_EXPORT RecordIdentifier {
   public:
    RecordIdentifier();
    RecordIdentifier(std::string primary_key, int64_t version);

    RecordIdentifier(const RecordIdentifier&) = delete;
    RecordIdentifier& operator=(const RecordIdentifier&) = delete;

    ~RecordIdentifier();

    const std::string& primary_key() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return primary_key_;
    }
    int64_t version() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return version_;
    }

    void Reset(std::string primary_key, int64_t version);

   private:
    // TODO(jsbell): Make it more clear that this is the *encoded* version of
    // the key.
    std::string primary_key_ GUARDED_BY_CONTEXT(sequence_checker_);
    int64_t version_ GUARDED_BY_CONTEXT(sequence_checker_) = -1;

    // Data members must be immutable or GUARDED_BY_CONTEXT(sequence_checker_).
    SEQUENCE_CHECKER(sequence_checker_);
  };

  // This class is not thread-safe.
  // All accessses to one instance must occur on the same sequence.
  class CONTENT_EXPORT Transaction {
   public:
    struct BlobWriteState {
      BlobWriteState();
      explicit BlobWriteState(int calls_left, BlobWriteCallback on_complete);
      ~BlobWriteState();
      int calls_left = 0;
      BlobWriteCallback on_complete;
    };

    Transaction(base::WeakPtr<IndexedDBBackingStore> backing_store,
                blink::mojom::IDBTransactionDurability durability,
                blink::mojom::IDBTransactionMode mode);

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    virtual ~Transaction();

    virtual void Begin(std::vector<PartitionedLock> locks);

    // CommitPhaseOne determines what blobs (if any) need to be written to disk
    // and updates the primary blob journal, and kicks off the async writing
    // of the blob files. In case of crash/rollback, the journal indicates what
    // files should be cleaned up.
    // The callback will be called eventually on success or failure, or
    // immediately if phase one is complete due to lack of any blobs to write.
    virtual leveldb::Status CommitPhaseOne(BlobWriteCallback callback);

    // CommitPhaseTwo is called once the blob files (if any) have been written
    // to disk, and commits the actual transaction to the backing store,
    // including blob journal updates, then deletes any blob files deleted
    // by the transaction and not referenced by running scripts.
    virtual leveldb::Status CommitPhaseTwo();

    // When LevelDBScopes is in single-sequence mode then this will return the
    // result of the rollback. Otherwise leveldb::Status::OK() is returned.
    virtual leveldb::Status Rollback();

    void Reset();
    leveldb::Status PutExternalObjectsIfNeeded(
        int64_t database_id,
        const std::string& object_store_data_key,
        std::vector<IndexedDBExternalObject>*);
    void PutExternalObjects(int64_t database_id,
                            const std::string& object_store_data_key,
                            std::vector<IndexedDBExternalObject>*);

    TransactionalLevelDBTransaction* transaction() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return transaction_.get();
    }

    virtual uint64_t GetTransactionSize();

    leveldb::Status GetExternalObjectsForRecord(
        int64_t database_id,
        const std::string& object_store_data_key,
        IndexedDBValue* value);

    base::WeakPtr<Transaction> AsWeakPtr();

    blink::mojom::IDBTransactionMode mode() const { return mode_; }

    IndexedDBBackingStore* backing_store() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return backing_store_.get();
    }

   private:
    // Called by CommitPhaseOne: Identifies the blob entries to write and adds
    // them to the recovery blob journal directly (i.e. not as part of the
    // transaction). Populates blobs_to_write_.
    leveldb::Status HandleBlobPreTransaction();

    // Called by CommitPhaseOne: Populates blob_files_to_remove_ by
    // determining which blobs are deleted as part of the transaction, and
    // adds blob entry cleanup operations to the transaction. Returns true on
    // success, false on failure.
    bool CollectBlobFilesToRemove();

    // Called by CommitPhaseOne: Kicks off the asynchronous writes of blobs
    // identified in HandleBlobPreTransaction. The callback will be called
    // eventually on success or failure.
    leveldb::Status WriteNewBlobs(BlobWriteCallback callback);

    // Called by CommitPhaseTwo: Partition blob references in blobs_to_remove_
    // into live (active references) and dead (no references).
    void PartitionBlobsToRemove(BlobJournalType* dead_blobs,
                                BlobJournalType* live_blobs) const;

    // This does NOT mean that this class can outlive the IndexedDBBackingStore.
    // This is only to protect against security issues before this class is
    // refactored away and this isn't necessary.
    // https://crbug.com/1012918
    base::WeakPtr<IndexedDBBackingStore> backing_store_
        GUARDED_BY_CONTEXT(sequence_checker_);

    const raw_ptr<TransactionalLevelDBFactory> transactional_leveldb_factory_;

    scoped_refptr<TransactionalLevelDBTransaction> transaction_
        GUARDED_BY_CONTEXT(sequence_checker_);

    std::map<std::string, std::unique_ptr<IndexedDBExternalObjectChangeRecord>>
        external_object_change_map_ GUARDED_BY_CONTEXT(sequence_checker_);
    std::map<std::string, std::unique_ptr<IndexedDBExternalObjectChangeRecord>>
        incognito_external_object_map_ GUARDED_BY_CONTEXT(sequence_checker_);
    int64_t database_id_ GUARDED_BY_CONTEXT(sequence_checker_) = -1;

    // List of blob files being newly written as part of this transaction.
    // These will be added to the recovery blob journal prior to commit, then
    // removed after a successful commit.
    BlobJournalType blobs_to_write_ GUARDED_BY_CONTEXT(sequence_checker_);

    // List of blob files being deleted as part of this transaction. These will
    // be added to either the recovery or live blob journal as appropriate
    // following a successful commit.
    BlobJournalType blobs_to_remove_ GUARDED_BY_CONTEXT(sequence_checker_);

    // Populated when blobs are being written to disk. This is saved here (as
    // opposed to being ephemeral and owned by the WriteBlobToFile callbacks)
    // because the transaction needs to be able to cancel this operation in
    // Rollback().
    absl::optional<BlobWriteState> write_state_
        GUARDED_BY_CONTEXT(sequence_checker_);

    // Set to true between CommitPhaseOne and CommitPhaseTwo/Rollback, to
    // indicate that the committing_transaction_count_ on the backing store
    // has been bumped, and journal cleaning should be deferred.
    bool committing_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

    // This flag is passed to LevelDBScopes as `sync_on_commit`, converted
    // via ShouldSyncOnCommit.
    const blink::mojom::IDBTransactionDurability durability_;
    const blink::mojom::IDBTransactionMode mode_;

    // Data members must be immutable or GUARDED_BY_CONTEXT(sequence_checker_).
    SEQUENCE_CHECKER(sequence_checker_);

    base::WeakPtrFactory<Transaction> weak_ptr_factory_
        GUARDED_BY_CONTEXT(sequence_checker_){this};
  };

  // This class is not thread-safe.
  // All accessses to one instance must occur on the same sequence.
  class Cursor {
   public:
    enum IteratorState { READY = 0, SEEK };

    struct CursorOptions {
      CursorOptions();
      CursorOptions(const CursorOptions& other);
      ~CursorOptions();

      int64_t database_id;
      int64_t object_store_id;
      int64_t index_id;
      std::string low_key;
      bool low_open;
      std::string high_key;
      bool high_open;
      bool forward;
      bool unique;
      blink::mojom::IDBTransactionMode mode =
          blink::mojom::IDBTransactionMode::ReadWrite;
    };

    Cursor(const Cursor&) = delete;
    Cursor& operator=(const Cursor&) = delete;

    virtual ~Cursor();

    const blink::IndexedDBKey& key() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return *current_key_;
    }

    bool Continue(leveldb::Status* s) {
      return Continue(nullptr, nullptr, SEEK, s);
    }
    bool Continue(const blink::IndexedDBKey* key,
                  IteratorState state,
                  leveldb::Status* s) {
      return Continue(key, nullptr, state, s);
    }
    bool Continue(const blink::IndexedDBKey* key,
                  const blink::IndexedDBKey* primary_key,
                  IteratorState state,
                  leveldb::Status*);
    bool Advance(uint32_t count, leveldb::Status*);
    bool FirstSeek(leveldb::Status*);

    // Clone may return a nullptr if cloning fails for any reason.
    virtual std::unique_ptr<Cursor> Clone() const = 0;
    virtual const blink::IndexedDBKey& primary_key() const;
    virtual IndexedDBValue* value() = 0;
    virtual const RecordIdentifier& record_identifier() const;
    virtual bool LoadCurrentRow(leveldb::Status* s) = 0;

   protected:
    Cursor(base::WeakPtr<Transaction> transaction,
           int64_t database_id,
           const CursorOptions& cursor_options);
    explicit Cursor(const IndexedDBBackingStore::Cursor* other,
                    std::unique_ptr<TransactionalLevelDBIterator> iterator);

    // May return nullptr.
    static std::unique_ptr<TransactionalLevelDBIterator> CloneIterator(
        const IndexedDBBackingStore::Cursor* other);

    virtual std::string EncodeKey(const blink::IndexedDBKey& key) = 0;
    virtual std::string EncodeKey(const blink::IndexedDBKey& key,
                                  const blink::IndexedDBKey& primary_key) = 0;

    bool IsPastBounds() const;
    bool HaveEnteredRange() const;

    // This does NOT mean that this class can outlive the Transaction.
    // This is only to protect against security issues before this class is
    // refactored away and this isn't necessary.
    // https://crbug.com/1012918
    const base::WeakPtr<Transaction> transaction_;
    const int64_t database_id_;
    const CursorOptions cursor_options_;
    std::unique_ptr<TransactionalLevelDBIterator> iterator_
        GUARDED_BY_CONTEXT(sequence_checker_);
    std::unique_ptr<blink::IndexedDBKey> current_key_
        GUARDED_BY_CONTEXT(sequence_checker_);
    IndexedDBBackingStore::RecordIdentifier record_identifier_
        GUARDED_BY_CONTEXT(sequence_checker_);

    // Data members must be immutable or GUARDED_BY_CONTEXT(sequence_checker_).
    SEQUENCE_CHECKER(sequence_checker_);

   private:
    enum class ContinueResult { LEVELDB_ERROR, DONE, OUT_OF_BOUNDS };

    // For cursors with direction Next or NextNoDuplicate.
    ContinueResult ContinueNext(const blink::IndexedDBKey* key,
                                const blink::IndexedDBKey* primary_key,
                                IteratorState state,
                                leveldb::Status*);
    // For cursors with direction Prev or PrevNoDuplicate. The PrevNoDuplicate
    // case has additional complexity of not being symmetric with
    // NextNoDuplicate.
    ContinueResult ContinuePrevious(const blink::IndexedDBKey* key,
                                    const blink::IndexedDBKey* primary_key,
                                    IteratorState state,
                                    leveldb::Status*);

    base::WeakPtrFactory<Cursor> weak_factory_
        GUARDED_BY_CONTEXT(sequence_checker_){this};
  };

  using BlobFilesCleanedCallback = base::RepeatingClosure;
  using ReportOutstandingBlobsCallback =
      base::RepeatingCallback<void(/*outstanding_blobs=*/bool)>;

  enum class Mode { kInMemory, kOnDisk };

  // Schedule an immediate blob journal cleanup if we reach this number of
  // requests.
  static constexpr const int kMaxJournalCleanRequests = 50;
  // Wait for a maximum of 5 seconds from the first call to the timer since the
  // last journal cleaning.
  static constexpr const base::TimeDelta kMaxJournalCleaningWindowTime =
      base::Seconds(5);
  // Default to a 2 second timer delay before we clean up blobs.
  static constexpr const base::TimeDelta kInitialJournalCleaningWindowTime =
      base::Seconds(2);

  IndexedDBBackingStore(
      Mode backing_store_mode,
      TransactionalLevelDBFactory* transactional_leveldb_factory,
      const storage::BucketLocator& bucket_locator,
      const base::FilePath& blob_path,
      std::unique_ptr<TransactionalLevelDBDatabase> db,
      storage::mojom::BlobStorageContext* blob_storage_context,
      storage::mojom::FileSystemAccessContext* file_system_access_context,
      std::unique_ptr<storage::FilesystemProxy> filesystem_proxy,
      BlobFilesCleanedCallback blob_files_cleaned,
      ReportOutstandingBlobsCallback report_outstanding_blobs,
      scoped_refptr<base::SequencedTaskRunner> idb_task_runner);

  IndexedDBBackingStore(const IndexedDBBackingStore&) = delete;
  IndexedDBBackingStore& operator=(const IndexedDBBackingStore&) = delete;

  virtual ~IndexedDBBackingStore();

  // Initializes the backing store. This must be called before doing any
  // operations or method calls on this object.
  leveldb::Status Initialize(bool clean_active_blob_journal);

  const storage::BucketLocator& bucket_locator() const {
    return bucket_locator_;
  }
  base::SequencedTaskRunner* idb_task_runner() const {
    return idb_task_runner_.get();
  }
  IndexedDBActiveBlobRegistry* active_blob_registry() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return active_blob_registry_.get();
  }

  // Compact is public for testing.
  virtual void Compact();
  virtual leveldb::Status DeleteDatabase(
      const std::u16string& name,
      TransactionalLevelDBTransaction* transaction);

  static bool RecordCorruptionInfo(const base::FilePath& path_base,
                                   const storage::BucketLocator& bucket_locator,
                                   const std::string& message);

  [[nodiscard]] virtual leveldb::Status GetRecord(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKey& key,
      IndexedDBValue* record);
  [[nodiscard]] virtual leveldb::Status PutRecord(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKey& key,
      IndexedDBValue* value,
      RecordIdentifier* record);
  [[nodiscard]] virtual leveldb::Status ClearObjectStore(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id);
  [[nodiscard]] virtual leveldb::Status DeleteRecord(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const RecordIdentifier& record);
  [[nodiscard]] virtual leveldb::Status DeleteRange(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKeyRange&);
  [[nodiscard]] virtual leveldb::Status GetKeyGeneratorCurrentNumber(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t* current_number);
  [[nodiscard]] virtual leveldb::Status MaybeUpdateKeyGeneratorCurrentNumber(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t new_state,
      bool check_current);
  [[nodiscard]] virtual leveldb::Status KeyExistsInObjectStore(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKey& key,
      RecordIdentifier* found_record_identifier,
      bool* found);

  [[nodiscard]] virtual leveldb::Status ClearIndex(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id);
  [[nodiscard]] virtual leveldb::Status PutIndexDataForRecord(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      const RecordIdentifier& record);
  [[nodiscard]] virtual leveldb::Status GetPrimaryKeyViaIndex(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      std::unique_ptr<blink::IndexedDBKey>* primary_key);
  [[nodiscard]] virtual leveldb::Status KeyExistsInIndex(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      std::unique_ptr<blink::IndexedDBKey>* found_primary_key,
      bool* exists);

  // Public for IndexedDBActiveBlobRegistry::MarkBlobInactive.
  virtual void ReportBlobUnused(int64_t database_id, int64_t blob_number);

  base::FilePath GetBlobFileName(int64_t database_id, int64_t key) const;

  virtual std::unique_ptr<Cursor> OpenObjectStoreKeyCursor(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection,
      leveldb::Status*);
  virtual std::unique_ptr<Cursor> OpenObjectStoreCursor(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection,
      leveldb::Status*);
  virtual std::unique_ptr<Cursor> OpenIndexKeyCursor(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection,
      leveldb::Status*);
  virtual std::unique_ptr<Cursor> OpenIndexCursor(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection,
      leveldb::Status*);

  TransactionalLevelDBDatabase* db() { return db_.get(); }

  const std::string& origin_identifier() { return origin_identifier_; }

  // Returns true if a blob cleanup job is pending on journal_cleaning_timer_.
  bool IsBlobCleanupPending();

  int64_t GetInMemoryBlobSize() const;

#if DCHECK_IS_ON()
  int NumBlobFilesDeletedForTesting() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return num_blob_files_deleted_;
  }
  int NumAggregatedJournalCleaningRequestsForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return num_aggregated_journal_cleaning_requests_;
  }
#endif
  void SetExecuteJournalCleaningOnNoTransactionsForTesting() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    execute_journal_cleaning_on_no_txns_ = true;
  }

  // Stops the journal_cleaning_timer_ and runs its pending task.
  void ForceRunBlobCleanup();

  // HasV2SchemaCorruption() returns whether the backing store is v2 and
  // has blob references.
  V2SchemaCorruptionStatus HasV2SchemaCorruption();

  // RevertSchemaToV2() updates a backing store state on disk to override its
  // metadata version to 2.  This allows triggering https://crbug.com/829141 on
  // an otherwise healthy backing store.
  leveldb::Status RevertSchemaToV2();

  bool is_incognito() const { return backing_store_mode_ == Mode::kInMemory; }

  virtual std::unique_ptr<Transaction> CreateTransaction(
      blink::mojom::IDBTransactionDurability durability,
      blink::mojom::IDBTransactionMode mode);

  base::WeakPtr<IndexedDBBackingStore> AsWeakPtr() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return weak_factory_.GetWeakPtr();
  }

  static bool ShouldSyncOnCommit(
      blink::mojom::IDBTransactionDurability durability);

 protected:
  friend class IndexedDBBucketState;

  leveldb::Status AnyDatabaseContainsBlobs(
      TransactionalLevelDBDatabase* database,
      bool* blobs_exist);

  // A helper function for V4 schema migration.
  // It iterates through all blob files.  It will add to the db entry both the
  // size and modified date for the blob based on the written file.  If any blob
  // file in the db is missing on disk, it will return an inconsistency status.
  leveldb::Status UpgradeBlobEntriesToV4(
      TransactionalLevelDBDatabase* database,
      LevelDBWriteBatch* write_batch,
      std::vector<base::FilePath>* empty_blobs_to_delete);
  // A helper function for V5 schema miration.
  // Iterates through all blob files on disk and validates they exist,
  // returning an internal inconsistency corruption error if any are missing.
  leveldb::Status ValidateBlobFiles(TransactionalLevelDBDatabase* database);

  // TODO(dmurph): Move this completely to IndexedDBMetadataFactory.
  leveldb::Status GetCompleteMetadata(
      std::vector<blink::IndexedDBDatabaseMetadata>* output);

  // Remove the referenced file on disk.
  virtual bool RemoveBlobFile(int64_t database_id, int64_t key) const;

  // Schedule a call to CleanRecoveryJournalIgnoreReturn() via
  // an owned timer. If this object is destroyed, the timer
  // will automatically be cancelled.
  virtual void StartJournalCleaningTimer();

  // Attempt to clean the recovery journal. This will remove
  // any referenced files and delete the journal entry. If any
  // transaction is currently committing this will be deferred
  // via StartJournalCleaningTimer().
  void CleanRecoveryJournalIgnoreReturn();

 private:
  friend class AutoDidCommitTransaction;

  leveldb::Status MigrateToV1(LevelDBWriteBatch* write_batch);
  leveldb::Status MigrateToV2(LevelDBWriteBatch* write_batch);
  leveldb::Status MigrateToV3(LevelDBWriteBatch* write_batch);
  leveldb::Status MigrateToV4(LevelDBWriteBatch* write_batch);
  leveldb::Status MigrateToV5(LevelDBWriteBatch* write_batch);

  leveldb::Status FindKeyInIndex(
      IndexedDBBackingStore::Transaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& key,
      std::string* found_encoded_primary_key,
      bool* found);

  // Remove the blob directory for the specified database and all contained
  // blob files.
  bool RemoveBlobDirectory(int64_t database_id) const;

  // Synchronously read the key-specified blob journal entry from the backing
  // store, delete all referenced blob files, and erase the journal entry.
  // This must not be used while temporary entries are present e.g. during
  // a two-stage transaction commit with blobs.
  leveldb::Status CleanUpBlobJournal(const std::string& level_db_key) const;

  // Synchronously delete the files and/or directories on disk referenced by
  // the blob journal.
  leveldb::Status CleanUpBlobJournalEntries(
      const BlobJournalType& journal) const;

  void WillCommitTransaction();
  // Can run a journal cleaning job if one is pending.
  void DidCommitTransaction();

  const Mode backing_store_mode_;
  const raw_ptr<TransactionalLevelDBFactory> transactional_leveldb_factory_;
  const storage::BucketLocator bucket_locator_;
  const base::FilePath blob_path_;

  // IndexedDB can store blobs and File System Access handles. These mojo
  // interfaces are used to make this possible by communicating with the
  // relevant subsystems.
  // Raw pointers are safe because the bindings are owned by
  // IndexedDBContextImpl.
  const raw_ptr<storage::mojom::BlobStorageContext> blob_storage_context_;
  const raw_ptr<storage::mojom::FileSystemAccessContext>
      file_system_access_context_;

  // Filesystem proxy to use for file operations.  nullptr if in memory.
  const std::unique_ptr<storage::FilesystemProxy> filesystem_proxy_;

  // The origin identifier is a key prefix, unique to the storage key's origin,
  // used in the leveldb backing store to partition data by origin. It is a
  // normalized version of the origin URL with a versioning suffix appended,
  // e.g. "http_localhost_81@1." Since only one storage key is stored per
  // backing store this is redundant but necessary for backwards compatibility.
  const std::string origin_identifier_;

  const scoped_refptr<base::SequencedTaskRunner> idb_task_runner_;
  std::map<std::string, std::unique_ptr<IndexedDBExternalObjectChangeRecord>>
      incognito_external_object_map_ GUARDED_BY_CONTEXT(sequence_checker_);

  bool execute_journal_cleaning_on_no_txns_
      GUARDED_BY_CONTEXT(sequence_checker_) = false;
  int num_aggregated_journal_cleaning_requests_
      GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  base::OneShotTimer journal_cleaning_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::TimeTicks journal_cleaning_timer_window_start_
      GUARDED_BY_CONTEXT(sequence_checker_);

#if DCHECK_IS_ON()
  mutable int num_blob_files_deleted_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
#endif

  const std::unique_ptr<TransactionalLevelDBDatabase> db_;

  const BlobFilesCleanedCallback blob_files_cleaned_;

  // Whenever blobs are registered in active_blob_registry_,
  // indexed_db_factory_ will hold a reference to this backing store.
  std::unique_ptr<IndexedDBActiveBlobRegistry> active_blob_registry_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Incremented whenever a transaction starts committing, decremented when
  // complete. While > 0, temporary journal entries may exist so out-of-band
  // journal cleaning must be deferred.
  size_t committing_transaction_count_ GUARDED_BY_CONTEXT(sequence_checker_) =
      0;

#if DCHECK_IS_ON()
  bool initialized_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
#endif

  // Data members must be immutable or GUARDED_BY_CONTEXT(sequence_checker_).
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IndexedDBBackingStore> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BACKING_STORE_H_
