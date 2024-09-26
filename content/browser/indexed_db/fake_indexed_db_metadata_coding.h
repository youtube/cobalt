// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_FAKE_INDEXED_DB_METADATA_CODING_H_
#define CONTENT_BROWSER_INDEXED_DB_FAKE_INDEXED_DB_METADATA_CODING_H_

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#include "content/browser/indexed_db/indexed_db_metadata_coding.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace blink {
struct IndexedDBDatabaseMetadata;
struct IndexedDBIndexMetadata;
struct IndexedDBObjectStoreMetadata;
}  // namespace blink

namespace content {
class TransactionalLevelDBDatabase;
class TransactionalLevelDBTransaction;

// A fake implementation of IndexedDBMetadataCoding, for testing.
class FakeIndexedDBMetadataCoding : public IndexedDBMetadataCoding {
 public:
  FakeIndexedDBMetadataCoding();

  FakeIndexedDBMetadataCoding(const FakeIndexedDBMetadataCoding&) = delete;
  FakeIndexedDBMetadataCoding& operator=(const FakeIndexedDBMetadataCoding&) =
      delete;

  ~FakeIndexedDBMetadataCoding() override;

  leveldb::Status ReadDatabaseNames(
      TransactionalLevelDBDatabase* db,
      const std::string& origin_identifier,
      std::vector<std::u16string>* names) override;

  leveldb::Status ReadMetadataForDatabaseName(
      TransactionalLevelDBDatabase* db,
      const std::string& origin_identifier,
      const std::u16string& name,
      blink::IndexedDBDatabaseMetadata* metadata,
      bool* found) override;
  leveldb::Status CreateDatabase(
      TransactionalLevelDBDatabase* database,
      const std::string& origin_identifier,
      const std::u16string& name,
      int64_t version,
      blink::IndexedDBDatabaseMetadata* metadata) override;

  leveldb::Status SetDatabaseVersion(
      TransactionalLevelDBTransaction* transaction,
      int64_t row_id,
      int64_t version,
      blink::IndexedDBDatabaseMetadata* metadata) override;

  leveldb::Status FindDatabaseId(TransactionalLevelDBDatabase* db,
                                 const std::string& origin_identifier,
                                 const std::u16string& name,
                                 int64_t* id,
                                 bool* found) override;

  leveldb::Status CreateObjectStore(
      TransactionalLevelDBTransaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      std::u16string name,
      blink::IndexedDBKeyPath key_path,
      bool auto_increment,
      blink::IndexedDBObjectStoreMetadata* metadata) override;

  leveldb::Status RenameObjectStore(
      TransactionalLevelDBTransaction* transaction,
      int64_t database_id,
      std::u16string new_name,
      std::u16string* old_name,
      blink::IndexedDBObjectStoreMetadata* metadata) override;

  leveldb::Status DeleteObjectStore(
      TransactionalLevelDBTransaction* transaction,
      int64_t database_id,
      const blink::IndexedDBObjectStoreMetadata& object_store) override;

  leveldb::Status CreateIndex(TransactionalLevelDBTransaction* transaction,
                              int64_t database_id,
                              int64_t object_store_id,
                              int64_t index_id,
                              std::u16string name,
                              blink::IndexedDBKeyPath key_path,
                              bool is_unique,
                              bool is_multi_entry,
                              blink::IndexedDBIndexMetadata* metadata) override;

  leveldb::Status RenameIndex(TransactionalLevelDBTransaction* transaction,
                              int64_t database_id,
                              int64_t object_store_id,
                              std::u16string new_name,
                              std::u16string* old_name,
                              blink::IndexedDBIndexMetadata* metadata) override;

  leveldb::Status DeleteIndex(
      TransactionalLevelDBTransaction* transaction,
      int64_t database_id,
      int64_t object_store_id,
      const blink::IndexedDBIndexMetadata& metadata) override;
};

}  // namespace content
#endif  // CONTENT_BROWSER_INDEXED_DB_FAKE_INDEXED_DB_METADATA_CODING_H_
