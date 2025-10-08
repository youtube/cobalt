/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_SQLITE_SQL_STATS_TABLE_H_
#define SRC_TRACE_PROCESSOR_SQLITE_SQL_STATS_TABLE_H_

<<<<<<< HEAD
#include <cstddef>

#include "src/trace_processor/sqlite/bindings/sqlite_module.h"

namespace perfetto::trace_processor {
=======
#include <limits>
#include <memory>

#include "src/trace_processor/sqlite/sqlite_table.h"

namespace perfetto {
namespace trace_processor {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

class QueryConstraints;
class TraceStorage;

// A virtual table that allows to introspect performances of the SQL engine
// for the kMaxLogEntries queries.
<<<<<<< HEAD
struct SqlStatsModule : sqlite::Module<SqlStatsModule> {
  using Context = TraceStorage;
  struct Vtab : sqlite::Module<SqlStatsModule>::Vtab {
    TraceStorage* storage = nullptr;
  };
  struct Cursor : sqlite::Module<SqlStatsModule>::Cursor {
    const TraceStorage* storage = nullptr;
    size_t row = 0;
    size_t num_rows = 0;
  };
=======
class SqlStatsTable : public SqliteTable {
 public:
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  enum Column {
    kQuery = 0,
    kTimeStarted = 1,
    kTimeFirstNext = 2,
    kTimeEnded = 3,
  };

<<<<<<< HEAD
  static constexpr auto kType = kEponymousOnly;
  static constexpr bool kSupportsWrites = false;
  static constexpr bool kDoesOverloadFunctions = false;

  static int Connect(sqlite3*,
                     void*,
                     int,
                     const char* const*,
                     sqlite3_vtab**,
                     char**);
  static int Disconnect(sqlite3_vtab*);

  static int BestIndex(sqlite3_vtab*, sqlite3_index_info*);

  static int Open(sqlite3_vtab*, sqlite3_vtab_cursor**);
  static int Close(sqlite3_vtab_cursor*);

  static int Filter(sqlite3_vtab_cursor*,
                    int,
                    const char*,
                    int,
                    sqlite3_value**);
  static int Next(sqlite3_vtab_cursor*);
  static int Eof(sqlite3_vtab_cursor*);
  static int Column(sqlite3_vtab_cursor*, sqlite3_context*, int);
  static int Rowid(sqlite3_vtab_cursor*, sqlite_int64*);

  // This needs to happen at the end as it depends on the functions
  // defined above.
  static constexpr sqlite3_module kModule = CreateModule();
};

}  // namespace perfetto::trace_processor
=======
  // Implementation of the SQLite cursor interface.
  class Cursor : public SqliteTable::Cursor {
   public:
    Cursor(SqlStatsTable* storage);
    ~Cursor() override;

    // Implementation of SqliteTable::Cursor.
    int Filter(const QueryConstraints&,
               sqlite3_value**,
               FilterHistory) override;
    int Next() override;
    int Eof() override;
    int Column(sqlite3_context*, int N) override;

   private:
    Cursor(Cursor&) = delete;
    Cursor& operator=(const Cursor&) = delete;

    Cursor(Cursor&&) noexcept = default;
    Cursor& operator=(Cursor&&) = default;

    size_t row_ = 0;
    size_t num_rows_ = 0;
    const TraceStorage* storage_ = nullptr;
    SqlStatsTable* table_ = nullptr;
  };

  SqlStatsTable(sqlite3*, const TraceStorage* storage);

  static void RegisterTable(sqlite3* db, const TraceStorage* storage);

  // Table implementation.
  base::Status Init(int, const char* const*, Schema*) override;
  std::unique_ptr<SqliteTable::Cursor> CreateCursor() override;
  int BestIndex(const QueryConstraints&, BestIndexInfo*) override;

 private:
  const TraceStorage* const storage_;
};

}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

#endif  // SRC_TRACE_PROCESSOR_SQLITE_SQL_STATS_TABLE_H_
