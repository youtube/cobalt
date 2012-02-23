// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_DIAGNOSTIC_ERROR_DELEGATE_H_
#define SQL_DIAGNOSTIC_ERROR_DELEGATE_H_
#pragma once

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "sql/connection.h"
#include "sql/sql_export.h"

namespace sql {

// This class handles the exceptional sqlite errors that we might encounter
// if for example the db is corrupted. Right now we just generate a UMA
// histogram for release and an assert for debug builds.
//
// Why is it a template you ask? well, that is a funny story. The histograms
// need to be singletons that is why they are always static at the function
// scope, but we cannot use the Singleton class because they are not default
// constructible. The template parameter makes the compiler to create unique
// classes that don't share the same static variable.
template <class UniqueT>
class DiagnosticErrorDelegate : public ErrorDelegate {
 public:

  virtual int OnError(int error, Connection* connection,
                      Statement* stmt) {
    LOG(ERROR) << "sqlite error " << error
               << ", errno " << connection->GetLastErrno()
               << ": " << connection->GetErrorMessage();
    RecordErrorInHistogram(error);
    return error;
  }

 private:
  static void RecordErrorInHistogram(int error) {
    // Trim off the extended error codes.
    error &= 0xff;

    // The histogram values from sqlite result codes go currently from 1 to
    // 26 currently but 50 gives them room to grow.
    UMA_HISTOGRAM_ENUMERATION(UniqueT::name(), error, 50);
  }
};

}  // namespace sql

#endif  // SQL_DIAGNOSTIC_ERROR_DELEGATE_H_
