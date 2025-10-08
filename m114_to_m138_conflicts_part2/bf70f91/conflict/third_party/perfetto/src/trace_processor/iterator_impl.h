/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_ITERATOR_IMPL_H_
#define SRC_TRACE_PROCESSOR_ITERATOR_IMPL_H_

#include <sqlite3.h>
<<<<<<< HEAD
#include <cstddef>
#include <cstdint>
#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/base/status.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/status_or.h"
#include "perfetto/trace_processor/basic_types.h"
#include "perfetto/trace_processor/iterator.h"
#include "src/trace_processor/perfetto_sql/engine/perfetto_sql_engine.h"
#include "src/trace_processor/sqlite/sqlite_engine.h"
=======

#include <memory>
#include <optional>
#include <vector>

#include "perfetto/base/build_config.h"
#include "perfetto/base/export.h"
#include "perfetto/trace_processor/basic_types.h"
#include "perfetto/trace_processor/iterator.h"
#include "perfetto/trace_processor/status.h"
#include "src/trace_processor/sqlite/scoped_db.h"
#include "src/trace_processor/sqlite/sqlite_utils.h"
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

namespace perfetto {
namespace trace_processor {

class TraceProcessorImpl;

class IteratorImpl {
 public:
<<<<<<< HEAD
  IteratorImpl(TraceProcessorImpl* impl,
               base::StatusOr<PerfettoSqlEngine::ExecutionResult>,
=======
  struct StmtMetadata {
    uint32_t column_count = 0;
    uint32_t statement_count = 0;
    uint32_t statement_count_with_output = 0;
  };

  IteratorImpl(TraceProcessorImpl* impl,
               sqlite3* db,
               base::Status,
               ScopedStmt,
               StmtMetadata,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
               uint32_t sql_stats_row);
  ~IteratorImpl();

  IteratorImpl(IteratorImpl&) noexcept = delete;
  IteratorImpl& operator=(IteratorImpl&) = delete;

  IteratorImpl(IteratorImpl&&) noexcept = default;
  IteratorImpl& operator=(IteratorImpl&&) = default;

  // Methods called by the base Iterator class.
  bool Next() {
<<<<<<< HEAD
    // In the past, we used to call sqlite3_step for the first time in this
    // function which 1:1 matched Next calls to sqlite3_step calls. However,
    // with the introduction of multi-statement support, we tokenize the
    // queries and so we need to *not* call step the first time Next is
    // called.
    //
    // Aside: if we could, we would change the API to match the new setup
    // (i.e. implement operator bool, make Next return nothing similar to C++
    // iterators); however, too many clients depend on the current behavior so
    // we have to keep the API as is.
=======
    PERFETTO_DCHECK(stmt_ || !status_.ok());

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    if (!called_next_) {
      // Delegate to the cc file to prevent trace_storage.h include in this
      // file.
      RecordFirstNextInSqlStats();
      called_next_ = true;
<<<<<<< HEAD
      return result_.ok() && !result_->stmt.IsDone();
    }
    if (!result_.ok()) {
      return false;
    }

    bool has_more = result_->stmt.Step();
    if (!result_->stmt.status().ok()) {
      PERFETTO_DCHECK(!has_more);
      result_ = result_->stmt.status();
    }
    return has_more;
  }

  SqlValue Get(uint32_t col) const {
    PERFETTO_DCHECK(result_.ok());

    auto column = static_cast<int>(col);
    sqlite3_stmt* stmt = result_->stmt.sqlite_stmt();
    auto col_type = sqlite3_column_type(stmt, column);
=======

      // In the past, we used to call sqlite3_step for the first time in this
      // function which 1:1 matched Next calls to sqlite3_step calls. However,
      // with the introduction of multi-statement support, we call
      // sqlite3_step when tokenizing the queries and so we need to *not* call
      // step the first time Next is called.
      //
      // Aside: if we could, we would change the API to match the new setup
      // (i.e. implement operator bool, make Next return nothing similar to C++
      // iterators); however, too many clients depend on the current behavior so
      // we have to keep the API as is.
      return status_.ok() && !sqlite_utils::IsStmtDone(*stmt_);
    }

    if (!status_.ok())
      return false;

    int ret = sqlite3_step(*stmt_);
    if (PERFETTO_UNLIKELY(ret != SQLITE_ROW && ret != SQLITE_DONE)) {
      status_ = base::ErrStatus("%s", sqlite_utils::FormatErrorMessage(
                                          stmt_.get(), std::nullopt, db_, ret)
                                          .c_message());
      stmt_.reset();
      return false;
    }
    return ret == SQLITE_ROW;
  }

  SqlValue Get(uint32_t col) {
    auto column = static_cast<int>(col);
    auto col_type = sqlite3_column_type(*stmt_, column);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    SqlValue value;
    switch (col_type) {
      case SQLITE_INTEGER:
        value.type = SqlValue::kLong;
<<<<<<< HEAD
        value.long_value = sqlite3_column_int64(stmt, column);
=======
        value.long_value = sqlite3_column_int64(*stmt_, column);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
        break;
      case SQLITE_TEXT:
        value.type = SqlValue::kString;
        value.string_value =
<<<<<<< HEAD
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
        break;
      case SQLITE_FLOAT:
        value.type = SqlValue::kDouble;
        value.double_value = sqlite3_column_double(stmt, column);
        break;
      case SQLITE_BLOB:
        value.type = SqlValue::kBytes;
        value.bytes_value = sqlite3_column_blob(stmt, column);
        value.bytes_count =
            static_cast<size_t>(sqlite3_column_bytes(stmt, column));
=======
            reinterpret_cast<const char*>(sqlite3_column_text(*stmt_, column));
        break;
      case SQLITE_FLOAT:
        value.type = SqlValue::kDouble;
        value.double_value = sqlite3_column_double(*stmt_, column);
        break;
      case SQLITE_BLOB:
        value.type = SqlValue::kBytes;
        value.bytes_value = sqlite3_column_blob(*stmt_, column);
        value.bytes_count =
            static_cast<size_t>(sqlite3_column_bytes(*stmt_, column));
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
        break;
      case SQLITE_NULL:
        value.type = SqlValue::kNull;
        break;
    }
    return value;
  }

<<<<<<< HEAD
  std::string GetColumnName(uint32_t col) const {
    return result_.ok() ? sqlite3_column_name(result_->stmt.sqlite_stmt(),
                                              static_cast<int>(col))
                        : "";
  }

  base::Status Status() const { return result_.status(); }

  uint32_t ColumnCount() const {
    return result_.ok() ? result_->stats.column_count : 0;
  }

  uint32_t StatementCount() const {
    return result_.ok() ? result_->stats.statement_count : 0;
  }

  uint32_t StatementCountWithOutput() const {
    return result_.ok() ? result_->stats.statement_count_with_output : 0;
  }

  std::string LastStatementSql() const {
    return result_.ok() ? result_->stmt.sql() : "";
=======
  std::string GetColumnName(uint32_t col) {
    return stmt_ ? sqlite3_column_name(*stmt_, static_cast<int>(col)) : "";
  }

  base::Status Status() { return status_; }

  uint32_t ColumnCount() { return stmt_metadata_.column_count; }

  uint32_t StatementCount() { return stmt_metadata_.statement_count; }

  uint32_t StatementCountWithOutput() {
    return stmt_metadata_.statement_count_with_output;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }

 private:
  // Dummy function to pass to ScopedResource.
  static int DummyClose(TraceProcessorImpl*) { return 0; }

  // Iterators hold onto an instance of TraceProcessor to track when the query
  // ends in the sql stats table. As iterators are movable, we need to null out
  // the TraceProcessor in the moved out iterator to avoid double recording
  // query ends. We could manually define a move constructor instead, but given
  // the error prone nature of keeping functions up to date, this seems like a
  // nicer approach.
  using ScopedTraceProcessor =
      base::ScopedResource<TraceProcessorImpl*, &DummyClose, nullptr>;

  void RecordFirstNextInSqlStats();

  ScopedTraceProcessor trace_processor_;
<<<<<<< HEAD
  base::StatusOr<PerfettoSqlEngine::ExecutionResult> result_;
=======
  sqlite3* db_ = nullptr;
  base::Status status_;

  ScopedStmt stmt_;
  StmtMetadata stmt_metadata_;

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  uint32_t sql_stats_row_ = 0;
  bool called_next_ = false;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_ITERATOR_IMPL_H_
