// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/transaction.h"

#include "base/logging.h"
#include "sql/connection.h"

namespace sql {

Transaction::Transaction(Connection* connection)
    : connection_(connection),
      is_open_(false) {
}

Transaction::~Transaction() {
  if (is_open_)
    connection_->RollbackTransaction();
}

bool Transaction::Begin() {
  if (is_open_) {
    NOTREACHED() << "Beginning a transaction twice!";
    return false;
  }
  is_open_ = connection_->BeginTransaction();
  return is_open_;
}

void Transaction::Rollback() {
  if (!is_open_) {
    NOTREACHED() << "Attempting to roll back a nonexistent transaction. "
                 << "Did you remember to call Begin() and check its return?";
    return;
  }
  is_open_ = false;
  connection_->RollbackTransaction();
}

bool Transaction::Commit() {
  if (!is_open_) {
    NOTREACHED() << "Attempting to commit a nonexistent transaction. "
                 << "Did you remember to call Begin() and check its return?";
    return false;
  }
  is_open_ = false;
  return connection_->CommitTransaction();
}

}  // namespace sql
