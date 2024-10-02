// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_CALLBACKS_H_
#define CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_CALLBACKS_H_

#include <stdint.h>

#include <vector>

#include "base/functional/callback.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

namespace blink {
struct IndexedDBDatabaseMetadata;
}

namespace content {

class MockIndexedDBCallbacks : public IndexedDBCallbacks {
 public:
  MockIndexedDBCallbacks();
  explicit MockIndexedDBCallbacks(bool expect_connection);

  MockIndexedDBCallbacks(const MockIndexedDBCallbacks&) = delete;
  MockIndexedDBCallbacks& operator=(const MockIndexedDBCallbacks&) = delete;

  void OnError(const IndexedDBDatabaseError& error) override;

  void OnSuccess() override;
  void OnSuccess(int64_t result) override;
  void OnSuccess(std::unique_ptr<IndexedDBConnection> connection,
                 const blink::IndexedDBDatabaseMetadata& metadata) override;
  IndexedDBConnection* connection() { return connection_.get(); }

  std::unique_ptr<IndexedDBConnection> TakeConnection() {
    expect_connection_ = false;
    return std::move(connection_);
  }

  void OnUpgradeNeeded(int64_t old_version,
                       std::unique_ptr<IndexedDBConnection> connection,
                       const blink::IndexedDBDatabaseMetadata& metadata,
                       const IndexedDBDataLossInfo& data_loss_info) override;

  void CallOnUpgradeNeeded(base::OnceClosure closure);
  void CallOnDBSuccess(base::OnceClosure closure);
  void CallOnInfoSuccess(base::RepeatingClosure closure);

  bool error_called() { return error_called_; }
  bool upgrade_called() { return upgrade_called_; }
  bool info_called() { return info_called_; }

 protected:
  ~MockIndexedDBCallbacks() override;

  std::unique_ptr<IndexedDBConnection> connection_;

 private:
  bool expect_connection_ = true;
  bool error_called_ = false;
  bool upgrade_called_ = false;
  bool info_called_ = false;
  base::OnceClosure call_on_upgrade_needed_;
  base::OnceClosure call_on_db_success_;
  base::RepeatingClosure call_on_info_success_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_CALLBACKS_H_
