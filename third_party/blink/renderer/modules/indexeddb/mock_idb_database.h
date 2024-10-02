// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_IDB_DATABASE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_IDB_DATABASE_H_

#include <gmock/gmock.h>
#include <memory>

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"

namespace blink {

class MockIDBDatabase : public testing::StrictMock<mojom::blink::IDBDatabase> {
 public:
  MOCK_METHOD(void,
              RenameObjectStore,
              (int64_t transaction_id,
               int64_t object_store_id,
               const String& new_name),
              (override));
  MOCK_METHOD(
      void,
      CreateTransaction,
      (mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction> receiver,
       int64_t id,
       const Vector<int64_t>& scope,
       mojom::blink::IDBTransactionMode,
       mojom::blink::IDBTransactionDurability),
      (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(void, VersionChangeIgnored, (), (override));
  MOCK_METHOD(void, Abort, (int64_t transaction_id), (override));
  MOCK_METHOD(void,
              CreateIndex,
              (int64_t transaction_id,
               int64_t object_store_id,
               int64_t index_id,
               const String& name,
               const IDBKeyPath&,
               bool unique,
               bool multi_entry),
              (override));
  MOCK_METHOD(void,
              DeleteIndex,
              (int64_t transaction_id,
               int64_t object_store_id,
               int64_t index_id),
              (override));
  MOCK_METHOD(void,
              RenameIndex,
              (int64_t transaction_id,
               int64_t object_store_id,
               int64_t index_id,
               const String& new_name),
              (override));
  MOCK_METHOD(void,
              Get,
              (int64_t transaction_id,
               int64_t object_store_id,
               int64_t index_id,
               mojom::blink::IDBKeyRangePtr,
               bool key_only,
               GetCallback),
              (override));
  MOCK_METHOD(void,
              GetAll,
              (int64_t transaction_id,
               int64_t object_store_id,
               int64_t index_id,
               mojom::blink::IDBKeyRangePtr,
               bool key_only,
               int64_t max_count,
               GetAllCallback),
              (override));
  MOCK_METHOD(void,
              BatchGetAll,
              (int64_t transaction_id,
               int64_t object_store_id,
               int64_t index_id,
               Vector<mojom::blink::IDBKeyRangePtr> key_ranges,
               uint32_t max_count,
               BatchGetAllCallback),
              (override));
  MOCK_METHOD(void,
              SetIndexKeys,
              (int64_t transaction_id,
               int64_t object_store_id,
               std::unique_ptr<IDBKey> primary_key,
               Vector<IDBIndexKeys>),
              (override));
  MOCK_METHOD(void,
              SetIndexesReady,
              (int64_t transaction_id,
               int64_t object_store_id,
               const Vector<int64_t>& index_ids),
              (override));
  MOCK_METHOD(void,
              OpenCursor,
              (int64_t transaction_id,
               int64_t object_store_id,
               int64_t index_id,
               mojom::blink::IDBKeyRangePtr,
               mojom::blink::IDBCursorDirection,
               bool key_only,
               mojom::blink::IDBTaskType,
               OpenCursorCallback),
              (override));
  MOCK_METHOD(void,
              Count,
              (int64_t transaction_id,
               int64_t object_store_id,
               int64_t index_id,
               mojom::blink::IDBKeyRangePtr,
               mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks>),
              (override));
  MOCK_METHOD(void,
              DeleteRange,
              (int64_t transaction_id,
               int64_t object_store_id,
               mojom::blink::IDBKeyRangePtr,
               mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks>),
              (override));
  MOCK_METHOD(void,
              GetKeyGeneratorCurrentNumber,
              (int64_t transaction_id,
               int64_t object_store_id,
               mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks>),
              (override));
  MOCK_METHOD(void,
              Clear,
              (int64_t transaction_id,
               int64_t object_store_id,
               mojo::PendingAssociatedRemote<mojom::blink::IDBCallbacks>),
              (override));
  MOCK_METHOD(void, DidBecomeInactive, (), (override));

  void Bind(mojo::PendingAssociatedReceiver<mojom::blink::IDBDatabase>);
  mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase>
  BindNewEndpointAndPassDedicatedRemote();

  void Flush() { receiver_.FlushForTesting(); }

 private:
  mojo::AssociatedReceiver<mojom::blink::IDBDatabase> receiver_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_IDB_DATABASE_H_
