// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_DIAGNOSTIC_ERROR_DELEGATE_H_
#define SQL_DIAGNOSTIC_ERROR_DELEGATE_H_

#include "base/logging.h"
#include "sql/connection.h"
#include "sql/error_delegate_util.h"
#include "sql/sql_export.h"

namespace sql {

// This class handles the exceptional sqlite errors that we might encounter
// if for example the db is corrupted. Right now we just generate a UMA
// histogram for release and an assert for debug builds.
// See error_delegate_util.h for an explanation as to why this class is a
// template.
template <class UniqueT>
class DiagnosticErrorDelegate : public ErrorDelegate {
 public:
  DiagnosticErrorDelegate() {}
  virtual ~DiagnosticErrorDelegate() {}

  virtual int OnError(int error, Connection* connection,
                      Statement* stmt) {
    LogAndRecordErrorInHistogram<UniqueT>(error, connection);
    return error;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DiagnosticErrorDelegate);
};

}  // namespace sql

#endif  // SQL_DIAGNOSTIC_ERROR_DELEGATE_H_
