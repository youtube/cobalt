// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/mock_browsertest_indexed_db_class_factory.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scope.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_iterator.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_transaction.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_metadata_coding.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

using storage::mojom::FailClass;
using storage::mojom::FailMethod;

namespace {

class FunctionTracer {
 public:
  FunctionTracer(const std::string& class_name,
                 const std::string& method_name,
                 int instance_num)
      : class_name_(class_name),
        method_name_(method_name),
        instance_count_(instance_num),
        current_call_num_(0) {}

  void log_call() {
    current_call_num_++;
    VLOG(0) << class_name_ << '[' << instance_count_ << "]::" << method_name_
            << "()[" << current_call_num_ << ']';
  }

 private:
  std::string class_name_;
  std::string method_name_;
  int instance_count_;
  int current_call_num_;
};

}  // namespace

namespace content {

class IndexedDBTestDatabase : public IndexedDBDatabase {
 public:
  IndexedDBTestDatabase(
      const std::u16string& name,
      IndexedDBBackingStore* backing_store,
      IndexedDBFactory* factory,
      IndexedDBClassFactory* class_factory,
      TasksAvailableCallback tasks_available_callback,
      std::unique_ptr<IndexedDBMetadataCoding> metadata_coding,
      const Identifier& unique_identifier,
      PartitionedLockManager* transaction_lock_manager)
      : IndexedDBDatabase(name,
                          backing_store,
                          factory,
                          class_factory,
                          std::move(tasks_available_callback),
                          std::move(metadata_coding),
                          unique_identifier,
                          transaction_lock_manager) {}
  ~IndexedDBTestDatabase() override {}

 protected:
  size_t GetUsableMessageSizeInBytes() const override {
    return 10 * 1024 * 1024;  // 10MB
  }
};

class IndexedDBTestTransaction : public IndexedDBTransaction {
 public:
  IndexedDBTestTransaction(
      int64_t id,
      IndexedDBConnection* connection,
      const std::set<int64_t>& scope,
      blink::mojom::IDBTransactionMode mode,
      TasksAvailableCallback tasks_available_callback,
      IndexedDBTransaction::TearDownCallback tear_down_callback,
      IndexedDBBackingStore::Transaction* backing_store_transaction)
      : IndexedDBTransaction(id,
                             connection,
                             scope,
                             mode,
                             std::move(tasks_available_callback),
                             std::move(tear_down_callback),
                             backing_store_transaction) {}
  ~IndexedDBTestTransaction() override {}

 protected:
  // Browser tests run under memory/address sanitizers (etc) may trip the
  // default 60s timeout, so relax it during tests.
  base::TimeDelta GetInactivityTimeout() const override {
    return base::Seconds(60 * 60);
  }
};

class LevelDBTestDatabase : public TransactionalLevelDBDatabase {
 public:
  LevelDBTestDatabase(scoped_refptr<LevelDBState> level_db_state,
                      std::unique_ptr<LevelDBScopes> leveldb_scopes,
                      TransactionalLevelDBFactory* factory,
                      scoped_refptr<base::SequencedTaskRunner> task_runner,
                      size_t max_open_iterators,
                      FailMethod fail_method,
                      int fail_on_call_num)
      : TransactionalLevelDBDatabase(std::move(level_db_state),
                                     std::move(leveldb_scopes),
                                     factory,
                                     std::move(task_runner),
                                     max_open_iterators),
        fail_method_(fail_method),
        fail_on_call_num_(fail_on_call_num),
        current_call_num_(0) {
    DCHECK(fail_method != FailMethod::NOTHING);
    DCHECK_GT(fail_on_call_num, 0);
  }
  ~LevelDBTestDatabase() override = default;

  leveldb::Status Get(const base::StringPiece& key,
                      std::string* value,
                      bool* found) override {
    if (fail_method_ != FailMethod::GET ||
        ++current_call_num_ != fail_on_call_num_)
      return TransactionalLevelDBDatabase::Get(key, value, found);
    *found = false;
    return leveldb::Status::Corruption("Corrupted for the test");
  }

  leveldb::Status Write(LevelDBWriteBatch* write_batch) override {
    if ((fail_method_ != FailMethod::WRITE) ||
        ++current_call_num_ != fail_on_call_num_)
      return TransactionalLevelDBDatabase::Write(write_batch);
    return leveldb::Status::Corruption("Corrupted for the test");
  }

 private:
  FailMethod fail_method_;
  int fail_on_call_num_;
  int current_call_num_;
};

class LevelDBTestDirectTransaction : public LevelDBDirectTransaction {
 public:
  LevelDBTestDirectTransaction(TransactionalLevelDBDatabase* db,
                               FailMethod fail_method,
                               int fail_on_call_num)
      : LevelDBDirectTransaction(db),
        fail_method_(fail_method),
        fail_on_call_num_(fail_on_call_num),
        current_call_num_(0) {
    DCHECK(fail_method != FailMethod::NOTHING);
    DCHECK_GT(fail_on_call_num, 0);
  }
  ~LevelDBTestDirectTransaction() override = default;

  leveldb::Status Get(const base::StringPiece& key,
                      std::string* value,
                      bool* found) override {
    if (fail_method_ != FailMethod::GET ||
        ++current_call_num_ != fail_on_call_num_)
      return LevelDBTestDirectTransaction::Get(key, value, found);

    *found = false;
    return leveldb::Status::Corruption("Corrupted for the test");
  }

 private:
  FailMethod fail_method_;
  int fail_on_call_num_;
  int current_call_num_;
};

class LevelDBTestTransaction : public TransactionalLevelDBTransaction {
 public:
  LevelDBTestTransaction(TransactionalLevelDBDatabase* db,
                         std::unique_ptr<LevelDBScope> scope,
                         FailMethod fail_method,
                         int fail_on_call_num)
      : TransactionalLevelDBTransaction(db, std::move(scope)),
        fail_method_(fail_method),
        fail_on_call_num_(fail_on_call_num),
        current_call_num_(0) {
    DCHECK(fail_method != FailMethod::NOTHING);
    DCHECK_GT(fail_on_call_num, 0);
  }

  leveldb::Status Get(const base::StringPiece& key,
                      std::string* value,
                      bool* found) override {
    if (fail_method_ != FailMethod::GET ||
        ++current_call_num_ != fail_on_call_num_)
      return TransactionalLevelDBTransaction::Get(key, value, found);

    *found = false;
    return leveldb::Status::Corruption("Corrupted for the test");
  }

  leveldb::Status Commit(bool sync_on_commit) override {
    if (++current_call_num_ != fail_on_call_num_)
      return TransactionalLevelDBTransaction::Commit(sync_on_commit);

    if (fail_method_ == FailMethod::COMMIT)
      return leveldb::Status::Corruption("Corrupted for the test");

    if (fail_method_ == FailMethod::COMMIT_DISK_FULL) {
      return leveldb_env::MakeIOError("dummy filename", "Disk Full",
                                      leveldb_env::kWritableFileAppend,
                                      base::File::FILE_ERROR_NO_SPACE);
    }

    if (fail_method_ == FailMethod::COMMIT_SYNC && sync_on_commit) {
      return leveldb_env::MakeIOError("dummy filename", "Sync on commit",
                                      leveldb_env::kWritableFileAppend,
                                      base::File::FILE_ERROR_FAILED);
    }

    return TransactionalLevelDBTransaction::Commit(sync_on_commit);
  }

 private:
  ~LevelDBTestTransaction() override {}

  FailMethod fail_method_;
  int fail_on_call_num_;
  int current_call_num_;
};

class LevelDBTraceTransaction : public TransactionalLevelDBTransaction {
 public:
  LevelDBTraceTransaction(TransactionalLevelDBDatabase* db,
                          std::unique_ptr<LevelDBScope> scope,
                          int tx_num)
      : TransactionalLevelDBTransaction(db, std::move(scope)),
        class_name_("LevelDBTransaction"),
        commit_tracer_(class_name_, "Commit", tx_num),
        get_tracer_(class_name_, "Get", tx_num) {}

  leveldb::Status Get(const base::StringPiece& key,
                      std::string* value,
                      bool* found) override {
    get_tracer_.log_call();
    return TransactionalLevelDBTransaction::Get(key, value, found);
  }

  leveldb::Status Commit(bool sync_on_commit) override {
    commit_tracer_.log_call();
    return TransactionalLevelDBTransaction::Commit(sync_on_commit);
  }

 private:
  const std::string class_name_;

  ~LevelDBTraceTransaction() override = default;

  FunctionTracer commit_tracer_;
  FunctionTracer get_tracer_;
};

class LevelDBTraceIterator : public TransactionalLevelDBIterator {
 public:
  LevelDBTraceIterator(std::unique_ptr<leveldb::Iterator> iterator,
                       base::WeakPtr<TransactionalLevelDBDatabase> db,
                       base::WeakPtr<TransactionalLevelDBTransaction> txn,
                       std::unique_ptr<LevelDBSnapshot> snapshot,
                       int inst_num)
      : TransactionalLevelDBIterator(std::move(iterator),
                                     std::move(db),
                                     std::move(txn),
                                     std::move(snapshot)),
        class_name_("LevelDBIterator"),
        is_valid_tracer_(class_name_, "IsValid", inst_num),
        seek_to_last_tracer_(class_name_, "SeekToLast", inst_num),
        seek_tracer_(class_name_, "Seek", inst_num),
        next_tracer_(class_name_, "Next", inst_num),
        prev_tracer_(class_name_, "Prev", inst_num),
        key_tracer_(class_name_, "Key", inst_num),
        value_tracer_(class_name_, "Value", inst_num) {}
  ~LevelDBTraceIterator() override = default;

 private:
  const std::string class_name_;

  bool IsValid() const override {
    is_valid_tracer_.log_call();
    return TransactionalLevelDBIterator::IsValid();
  }
  leveldb::Status SeekToLast() override {
    seek_to_last_tracer_.log_call();
    return TransactionalLevelDBIterator::SeekToLast();
  }
  leveldb::Status Seek(const base::StringPiece& target) override {
    seek_tracer_.log_call();
    return TransactionalLevelDBIterator::Seek(target);
  }
  leveldb::Status Next() override {
    next_tracer_.log_call();
    return TransactionalLevelDBIterator::Next();
  }
  leveldb::Status Prev() override {
    prev_tracer_.log_call();
    return TransactionalLevelDBIterator::Prev();
  }
  base::StringPiece Key() const override {
    key_tracer_.log_call();
    return TransactionalLevelDBIterator::Key();
  }
  base::StringPiece Value() const override {
    value_tracer_.log_call();
    return TransactionalLevelDBIterator::Value();
  }

  mutable FunctionTracer is_valid_tracer_;
  mutable FunctionTracer seek_to_last_tracer_;
  mutable FunctionTracer seek_tracer_;
  mutable FunctionTracer next_tracer_;
  mutable FunctionTracer prev_tracer_;
  mutable FunctionTracer key_tracer_;
  mutable FunctionTracer value_tracer_;
};

class LevelDBTestIterator : public content::TransactionalLevelDBIterator {
 public:
  LevelDBTestIterator(std::unique_ptr<leveldb::Iterator> iterator,
                      base::WeakPtr<TransactionalLevelDBDatabase> db,
                      base::WeakPtr<TransactionalLevelDBTransaction> txn,
                      std::unique_ptr<LevelDBSnapshot> snapshot,
                      FailMethod fail_method,
                      int fail_on_call_num)
      : TransactionalLevelDBIterator(std::move(iterator),
                                     std::move(db),
                                     std::move(txn),
                                     std::move(snapshot)),
        fail_method_(fail_method),
        fail_on_call_num_(fail_on_call_num),
        current_call_num_(0) {}
  ~LevelDBTestIterator() override {}

 private:
  leveldb::Status Seek(const base::StringPiece& target) override {
    if (fail_method_ != FailMethod::SEEK ||
        ++current_call_num_ != fail_on_call_num_)
      return TransactionalLevelDBIterator::Seek(target);
    return leveldb::Status::Corruption("Corrupted for test");
  }

  FailMethod fail_method_;
  int fail_on_call_num_;
  int current_call_num_;
};

MockBrowserTestIndexedDBClassFactory::MockBrowserTestIndexedDBClassFactory()
    : failure_class_(FailClass::NOTHING),
      failure_method_(FailMethod::NOTHING),
      only_trace_calls_(false) {}

MockBrowserTestIndexedDBClassFactory::~MockBrowserTestIndexedDBClassFactory() =
    default;

TransactionalLevelDBFactory&
MockBrowserTestIndexedDBClassFactory::transactional_leveldb_factory() {
  return *this;
}

std::pair<std::unique_ptr<IndexedDBDatabase>, leveldb::Status>
MockBrowserTestIndexedDBClassFactory::CreateIndexedDBDatabase(
    const std::u16string& name,
    IndexedDBBackingStore* backing_store,
    IndexedDBFactory* factory,
    TasksAvailableCallback tasks_available_callback,
    std::unique_ptr<IndexedDBMetadataCoding> metadata_coding,
    const IndexedDBDatabase::Identifier& unique_identifier,
    PartitionedLockManager* transaction_lock_manager) {
  std::unique_ptr<IndexedDBTestDatabase> database =
      std::make_unique<IndexedDBTestDatabase>(
          name, backing_store, factory, this,
          std::move(tasks_available_callback), std::move(metadata_coding),
          unique_identifier, transaction_lock_manager);
  leveldb::Status s = database->OpenInternal();
  if (!s.ok())
    database.reset();
  return {std::move(database), s};
}

std::unique_ptr<IndexedDBTransaction>
MockBrowserTestIndexedDBClassFactory::CreateIndexedDBTransaction(
    int64_t id,
    IndexedDBConnection* connection,
    const std::set<int64_t>& scope,
    blink::mojom::IDBTransactionMode mode,
    TasksAvailableCallback tasks_available_callback,
    IndexedDBTransaction::TearDownCallback tear_down_callback,
    IndexedDBBackingStore::Transaction* backing_store_transaction) {
  return std::make_unique<IndexedDBTestTransaction>(
      id, connection, scope, mode, std::move(tasks_available_callback),
      std::move(tear_down_callback), backing_store_transaction);
}

std::unique_ptr<TransactionalLevelDBDatabase>
MockBrowserTestIndexedDBClassFactory::CreateLevelDBDatabase(
    scoped_refptr<LevelDBState> state,
    std::unique_ptr<LevelDBScopes> scopes,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    size_t max_open_iterators) {
  instance_count_[FailClass::LEVELDB_DATABASE] =
      instance_count_[FailClass::LEVELDB_DATABASE] + 1;
  if (failure_class_ == FailClass::LEVELDB_DATABASE &&
      instance_count_[FailClass::LEVELDB_DATABASE] ==
          fail_on_instance_num_[FailClass::LEVELDB_DATABASE]) {
    return std::make_unique<LevelDBTestDatabase>(
        std::move(state), std::move(scopes), this, std::move(task_runner),
        max_open_iterators, failure_method_,
        fail_on_call_num_[FailClass::LEVELDB_DATABASE]);
  } else {
    return DefaultTransactionalLevelDBFactory::CreateLevelDBDatabase(
        std::move(state), std::move(scopes), std::move(task_runner),
        max_open_iterators);
  }
}

std::unique_ptr<LevelDBDirectTransaction>
MockBrowserTestIndexedDBClassFactory::CreateLevelDBDirectTransaction(
    TransactionalLevelDBDatabase* db) {
  instance_count_[FailClass::LEVELDB_DIRECT_TRANSACTION] =
      instance_count_[FailClass::LEVELDB_DIRECT_TRANSACTION] + 1;
  if (failure_class_ == FailClass::LEVELDB_DIRECT_TRANSACTION &&
      instance_count_[FailClass::LEVELDB_DIRECT_TRANSACTION] ==
          fail_on_instance_num_[FailClass::LEVELDB_DIRECT_TRANSACTION]) {
    return std::make_unique<LevelDBTestDirectTransaction>(
        db, failure_method_,
        fail_on_call_num_[FailClass::LEVELDB_DIRECT_TRANSACTION]);
  } else {
    return DefaultTransactionalLevelDBFactory::CreateLevelDBDirectTransaction(
        db);
  }
}

scoped_refptr<TransactionalLevelDBTransaction>
MockBrowserTestIndexedDBClassFactory::CreateLevelDBTransaction(
    TransactionalLevelDBDatabase* db,
    std::unique_ptr<LevelDBScope> scope) {
  instance_count_[FailClass::LEVELDB_TRANSACTION] =
      instance_count_[FailClass::LEVELDB_TRANSACTION] + 1;
  if (only_trace_calls_) {
    return base::MakeRefCounted<LevelDBTraceTransaction>(
        db, std::move(scope), instance_count_[FailClass::LEVELDB_TRANSACTION]);
  } else {
    if (failure_class_ == FailClass::LEVELDB_TRANSACTION &&
        instance_count_[FailClass::LEVELDB_TRANSACTION] ==
            fail_on_instance_num_[FailClass::LEVELDB_TRANSACTION]) {
      return base::MakeRefCounted<LevelDBTestTransaction>(
          db, std::move(scope), failure_method_,
          fail_on_call_num_[FailClass::LEVELDB_TRANSACTION]);
    } else {
      return DefaultTransactionalLevelDBFactory::CreateLevelDBTransaction(
          db, std::move(scope));
    }
  }
}

std::unique_ptr<TransactionalLevelDBIterator>
MockBrowserTestIndexedDBClassFactory::CreateIterator(
    std::unique_ptr<leveldb::Iterator> iterator,
    base::WeakPtr<TransactionalLevelDBDatabase> db,
    base::WeakPtr<TransactionalLevelDBTransaction> txn,
    std::unique_ptr<LevelDBSnapshot> snapshot) {
  instance_count_[FailClass::LEVELDB_ITERATOR] =
      instance_count_[FailClass::LEVELDB_ITERATOR] + 1;
  if (only_trace_calls_) {
    return std::make_unique<LevelDBTraceIterator>(
        std::move(iterator), db, std::move(txn), std::move(snapshot),
        instance_count_[FailClass::LEVELDB_ITERATOR]);
  } else {
    if (failure_class_ == FailClass::LEVELDB_ITERATOR &&
        instance_count_[FailClass::LEVELDB_ITERATOR] ==
            fail_on_instance_num_[FailClass::LEVELDB_ITERATOR]) {
      return std::make_unique<LevelDBTestIterator>(
          std::move(iterator), db, std::move(txn), std::move(snapshot),
          failure_method_, fail_on_call_num_[FailClass::LEVELDB_ITERATOR]);
    } else {
      return DefaultTransactionalLevelDBFactory::CreateIterator(
          std::move(iterator), db, std::move(txn), std::move(snapshot));
    }
  }
}

void MockBrowserTestIndexedDBClassFactory::FailOperation(
    storage::mojom::FailClass failure_class,
    storage::mojom::FailMethod failure_method,
    int fail_on_instance_num,
    int fail_on_call_num,
    base::OnceClosure callback) {
  VLOG(0) << "FailOperation: class=" << failure_class
          << ", method=" << failure_method
          << ", instanceNum=" << fail_on_instance_num
          << ", callNum=" << fail_on_call_num;
  DCHECK(failure_class != FailClass::NOTHING);
  DCHECK(failure_method != FailMethod::NOTHING);
  failure_class_ = failure_class;
  failure_method_ = failure_method;
  fail_on_instance_num_[failure_class_] = fail_on_instance_num;
  fail_on_call_num_[failure_class_] = fail_on_call_num;
  instance_count_.clear();

  std::move(callback).Run();
}

void MockBrowserTestIndexedDBClassFactory::Reset() {
  failure_class_ = FailClass::NOTHING;
  failure_method_ = FailMethod::NOTHING;
  instance_count_.clear();
  fail_on_instance_num_.clear();
  fail_on_call_num_.clear();
}

}  // namespace content
