// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_ERROR_DELEGATE_UTIL_H_
#define SQL_ERROR_DELEGATE_UTIL_H_

#include "base/metrics/histogram.h"
#include "sql/connection.h"
#include "sql/sql_export.h"

namespace sql {

// Returns true if it is highly unlikely that the database can recover from
// |error|.
SQL_EXPORT bool IsErrorCatastrophic(int error);

// Log error in console in debug mode and generate a UMA histogram in release
// mode for |error| for |UniqueT::name()|.
// This function is templated because histograms need to be singletons. That is
// why they are always static at the function scope. The template parameter
// makes the compiler create unique functions that don't share the same static
// variable.
template <class UniqueT>
void LogAndRecordErrorInHistogram(int error,
                                  sql::Connection* connection) {
  LOG(ERROR) << "sqlite error " << error
             << ", errno " << connection->GetLastErrno()
             << ": " << connection->GetErrorMessage();

  // Trim off the extended error codes.
  error &= 0xff;

  // The histogram values from sqlite result codes currently go from 1 to 26
  // but 50 gives them room to grow.
  UMA_HISTOGRAM_ENUMERATION(UniqueT::name(), error, 50);
}

}  // namespace sql

#endif  // SQL_ERROR_DELEGATE_UTIL_H_
