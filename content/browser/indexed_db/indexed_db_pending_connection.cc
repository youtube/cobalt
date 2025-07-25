// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_pending_connection.h"

#include <utility>

namespace content {

IndexedDBPendingConnection::IndexedDBPendingConnection(
    std::unique_ptr<IndexedDBFactoryClient> factory_client,
    scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
    int64_t transaction_id,
    int64_t version,
    mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
        pending_mojo_receiver)
    : factory_client(std::move(factory_client)),
      database_callbacks(database_callbacks),
      transaction_id(transaction_id),
      version(version),
      pending_mojo_receiver(std::move(pending_mojo_receiver)) {}

IndexedDBPendingConnection::~IndexedDBPendingConnection() {}

}  // namespace content
