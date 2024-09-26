// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_MOCK_MOJO_INDEXED_DB_CALLBACKS_H_
#define CONTENT_BROWSER_INDEXED_DB_MOCK_MOJO_INDEXED_DB_CALLBACKS_H_

#include <stddef.h>
#include <string>

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace blink {
struct IndexedDBDatabaseMetadata;
}

namespace content {

class MockMojoIndexedDBCallbacks : public blink::mojom::IDBCallbacks {
 public:
  explicit MockMojoIndexedDBCallbacks();

  MockMojoIndexedDBCallbacks(const MockMojoIndexedDBCallbacks&) = delete;
  MockMojoIndexedDBCallbacks& operator=(const MockMojoIndexedDBCallbacks&) =
      delete;

  ~MockMojoIndexedDBCallbacks() override;

  mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
  CreateInterfacePtrAndBind();

  MOCK_METHOD2(Error,
               void(blink::mojom::IDBException code,
                    const std::u16string& message));

  MOCK_METHOD1(SuccessStringList,
               void(const std::vector<std::u16string>& value));

  MOCK_METHOD1(Blocked, void(int64_t existing_version));

  MOCK_METHOD5(MockedUpgradeNeeded,
               void(mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase>*
                        pending_database,
                    int64_t old_version,
                    blink::mojom::IDBDataLoss data_loss,
                    const std::string& data_loss_message,
                    const blink::IndexedDBDatabaseMetadata& metadata));

  // Move-only types not supported by mock methods.
  void UpgradeNeeded(
      mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database,
      int64_t old_version,
      blink::mojom::IDBDataLoss data_loss,
      const std::string& data_loss_message,
      const blink::IndexedDBDatabaseMetadata& metadata) override {
    MockedUpgradeNeeded(&pending_database, old_version, data_loss,
                        data_loss_message, metadata);
  }

  MOCK_METHOD2(MockedSuccessDatabase,
               void(mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase>*
                        pending_database,
                    const blink::IndexedDBDatabaseMetadata& metadata));
  void SuccessDatabase(
      mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_database,
      const blink::IndexedDBDatabaseMetadata& metadata) override {
    MockedSuccessDatabase(&pending_database, metadata);
  }

  MOCK_METHOD1(SuccessInteger, void(int64_t value));
  MOCK_METHOD0(Success, void());

 private:
  mojo::AssociatedReceiver<blink::mojom::IDBCallbacks> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_MOCK_MOJO_INDEXED_DB_CALLBACKS_H_
