/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_PRELUDE_FUNCTIONS_CLOCK_FUNCTIONS_H_
#define SRC_TRACE_PROCESSOR_PRELUDE_FUNCTIONS_CLOCK_FUNCTIONS_H_

#include <sqlite3.h>
#include <unordered_map>
#include "perfetto/ext/base/base64.h"
#include "protos/perfetto/common/builtin_clock.pbzero.h"
#include "src/trace_processor/importers/common/clock_converter.h"
#include "src/trace_processor/prelude/functions/create_function_internal.h"
#include "src/trace_processor/util/status_macros.h"

#include "src/trace_processor/prelude/functions/register_function.h"

namespace perfetto {
namespace trace_processor {

struct AbsTimeStr : public SqlFunction {
  using Context = ClockConverter;
  static base::Status Run(ClockConverter* tracker,
                          size_t argc,
                          sqlite3_value** argv,
                          SqlValue& out,
                          Destructors& destructors);
};

base::Status AbsTimeStr::Run(ClockConverter* tracker,
                             size_t argc,
                             sqlite3_value** argv,
                             SqlValue& out,
                             Destructors& destructors) {
  if (argc != 1) {
    return base::ErrStatus("ABS_TIME_STR: 1 arg required");
  }

  // If the timestamp is null, just return null as the result.
  if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
    return base::OkStatus();
  }
  if (sqlite3_value_type(argv[0]) != SQLITE_INTEGER) {
    return base::ErrStatus("ABS_TIME_STR: first argument should be timestamp");
  }

  int64_t ts = sqlite3_value_int64(argv[0]);
  base::StatusOr<std::string> iso8601 = tracker->ToAbsTime(ts);
  if (!iso8601.ok()) {
    // We are returning an OkStatus, because one bad timestamp shouldn't stop
    // the query.
    return base::OkStatus();
  }

  std::unique_ptr<char, base::FreeDeleter> s(
      static_cast<char*>(malloc(iso8601->size() + 1)));
  memcpy(s.get(), iso8601->c_str(), iso8601->size() + 1);

  destructors.string_destructor = free;
  out = SqlValue::String(s.release());
  return base::OkStatus();
}

struct ToMonotonic : public SqlFunction {
  using Context = ClockConverter;
  static base::Status Run(ClockConverter* tracker,
                          size_t argc,
                          sqlite3_value** argv,
                          SqlValue& out,
                          Destructors& destructors);
};

base::Status ToMonotonic::Run(ClockConverter* tracker,
                              size_t argc,
                              sqlite3_value** argv,
                              SqlValue& out,
                              Destructors&) {
  if (argc != 1) {
    return base::ErrStatus("TO_MONOTONIC: 1 arg required");
  }

  // If the timestamp is null, just return null as the result.
  if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
    return base::OkStatus();
  }
  if (sqlite3_value_type(argv[0]) != SQLITE_INTEGER) {
    return base::ErrStatus("TO_MONOTONIC: first argument should be timestamp");
  }

  int64_t ts = sqlite3_value_int64(argv[0]);
  base::StatusOr<int64_t> monotonic = tracker->ToMonotonic(ts);

  if (!monotonic.ok()) {
    // We are returning an OkStatus, because one bad timestamp shouldn't stop
    // the query.
    return base::OkStatus();
  }

  out = SqlValue::Long(*monotonic);
  return base::OkStatus();
}

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_PRELUDE_FUNCTIONS_CLOCK_FUNCTIONS_H_
