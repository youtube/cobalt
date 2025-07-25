/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_SQLITE_DB_SQLITE_TABLE_H_
#define SRC_TRACE_PROCESSOR_SQLITE_DB_SQLITE_TABLE_H_

#include "src/trace_processor/containers/bit_vector.h"
#include "src/trace_processor/db/table.h"
#include "src/trace_processor/prelude/table_functions/table_function.h"
#include "src/trace_processor/sqlite/query_cache.h"
#include "src/trace_processor/sqlite/sqlite_table.h"

namespace perfetto {
namespace trace_processor {

// Implements the SQLite table interface for db tables.
class DbSqliteTable : public SqliteTable {
 public:
  enum class TableComputation {
    // Mode when the table is static (i.e. passed in at construction
    // time).
    kStatic,

    // Mode when table is dynamically computed at filter time.
    kDynamic,
  };

  class Cursor : public SqliteTable::Cursor {
   public:
    Cursor(DbSqliteTable*, QueryCache*);

    Cursor(Cursor&&) noexcept = default;
    Cursor& operator=(Cursor&&) = default;

    // Implementation of SqliteTable::Cursor.
    int Filter(const QueryConstraints& qc,
               sqlite3_value** argv,
               FilterHistory) override;
    int Next() override;
    int Eof() override;
    int Column(sqlite3_context*, int N) override;

   private:
    enum class Mode {
      kSingleRow,
      kTable,
    };

    // Tries to create a sorted table to cache in |sorted_cache_table_| if the
    // constraint set matches the requirements.
    void TryCacheCreateSortedTable(const QueryConstraints&, FilterHistory);

    const Table* SourceTable() const {
      // Try and use the sorted cache table (if it exists) to speed up the
      // sorting. Otherwise, just use the original table.
      return sorted_cache_table_ ? &*sorted_cache_table_ : upstream_table_;
    }

    Cursor(const Cursor&) = delete;
    Cursor& operator=(const Cursor&) = delete;

    DbSqliteTable* db_sqlite_table_ = nullptr;
    QueryCache* cache_ = nullptr;

    const Table* upstream_table_ = nullptr;

    // Only valid for |db_sqlite_table_->computation_| ==
    // TableComputation::kDynamic.
    std::unique_ptr<Table> dynamic_table_;

    // Only valid for Mode::kSingleRow.
    std::optional<uint32_t> single_row_;

    // Only valid for Mode::kTable.
    std::optional<Table> db_table_;
    std::optional<Table::Iterator> iterator_;

    bool eof_ = true;

    // Stores a sorted version of |db_table_| sorted on a repeated equals
    // constraint. This allows speeding up repeated subqueries in joins
    // significantly.
    std::shared_ptr<Table> sorted_cache_table_;

    // Stores the count of repeated equality queries to decide whether it is
    // wortwhile to sort |db_table_| to create |sorted_cache_table_|.
    uint32_t repeated_cache_count_ = 0;

    Mode mode_ = Mode::kSingleRow;

    std::vector<Constraint> constraints_;
    std::vector<Order> orders_;
  };
  struct QueryCost {
    double cost;
    uint32_t rows;
  };
  struct Context {
    QueryCache* cache;
    TableComputation computation;

    // Only valid when computation == TableComputation::kStatic.
    const Table* static_table;

    // Only valid when computation == TableComputation::kDynamic.
    std::unique_ptr<TableFunction> generator;
  };

  static void RegisterTable(sqlite3* db,
                            QueryCache* cache,
                            const Table* table,
                            const std::string& name);

  static void RegisterTable(sqlite3* db,
                            QueryCache* cache,
                            std::unique_ptr<TableFunction> generator);

  DbSqliteTable(sqlite3*, Context context);
  virtual ~DbSqliteTable() override;

  // Table implementation.
  base::Status Init(int,
                    const char* const*,
                    SqliteTable::Schema*) override final;
  std::unique_ptr<SqliteTable::Cursor> CreateCursor() override;
  int ModifyConstraints(QueryConstraints*) override final;
  int BestIndex(const QueryConstraints&, BestIndexInfo*) override final;

  // These static functions are useful to allow other callers to make use
  // of them.
  static SqliteTable::Schema ComputeSchema(const Table::Schema&,
                                           const char* table_name);
  static void ModifyConstraints(const Table::Schema&, QueryConstraints*);
  static void BestIndex(const Table::Schema&,
                        uint32_t row_count,
                        const QueryConstraints&,
                        BestIndexInfo*);

  // static for testing.
  static QueryCost EstimateCost(const Table::Schema&,
                                uint32_t row_count,
                                const QueryConstraints& qc);

 private:
  QueryCache* cache_ = nullptr;

  TableComputation computation_ = TableComputation::kStatic;

  // Only valid after Init has completed.
  Table::Schema schema_;

  // Only valid when computation_ == TableComputation::kStatic.
  const Table* static_table_ = nullptr;

  // Only valid when computation_ == TableComputation::kDynamic.
  std::unique_ptr<TableFunction> generator_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_SQLITE_DB_SQLITE_TABLE_H_
