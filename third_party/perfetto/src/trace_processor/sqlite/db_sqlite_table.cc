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

#include "src/trace_processor/sqlite/db_sqlite_table.h"

#include "perfetto/ext/base/small_vector.h"
#include "perfetto/ext/base/string_writer.h"
#include "src/trace_processor/containers/bit_vector.h"
#include "src/trace_processor/sqlite/query_cache.h"
#include "src/trace_processor/sqlite/sqlite_utils.h"
#include "src/trace_processor/tp_metatrace.h"

namespace perfetto {
namespace trace_processor {

namespace {

std::optional<FilterOp> SqliteOpToFilterOp(int sqlite_op) {
  switch (sqlite_op) {
    case SQLITE_INDEX_CONSTRAINT_EQ:
    case SQLITE_INDEX_CONSTRAINT_IS:
      return FilterOp::kEq;
    case SQLITE_INDEX_CONSTRAINT_GT:
      return FilterOp::kGt;
    case SQLITE_INDEX_CONSTRAINT_LT:
      return FilterOp::kLt;
    case SQLITE_INDEX_CONSTRAINT_ISNOT:
    case SQLITE_INDEX_CONSTRAINT_NE:
      return FilterOp::kNe;
    case SQLITE_INDEX_CONSTRAINT_GE:
      return FilterOp::kGe;
    case SQLITE_INDEX_CONSTRAINT_LE:
      return FilterOp::kLe;
    case SQLITE_INDEX_CONSTRAINT_ISNULL:
      return FilterOp::kIsNull;
    case SQLITE_INDEX_CONSTRAINT_ISNOTNULL:
      return FilterOp::kIsNotNull;
    case SQLITE_INDEX_CONSTRAINT_GLOB:
      return FilterOp::kGlob;
    case SQLITE_INDEX_CONSTRAINT_LIKE:
    // TODO(lalitm): start supporting these constraints.
    case SQLITE_INDEX_CONSTRAINT_LIMIT:
    case SQLITE_INDEX_CONSTRAINT_OFFSET:
      return std::nullopt;
    default:
      PERFETTO_FATAL("Currently unsupported constraint");
  }
}

SqlValue SqliteValueToSqlValue(sqlite3_value* sqlite_val) {
  auto col_type = sqlite3_value_type(sqlite_val);
  SqlValue value;
  switch (col_type) {
    case SQLITE_INTEGER:
      value.type = SqlValue::kLong;
      value.long_value = sqlite3_value_int64(sqlite_val);
      break;
    case SQLITE_TEXT:
      value.type = SqlValue::kString;
      value.string_value =
          reinterpret_cast<const char*>(sqlite3_value_text(sqlite_val));
      break;
    case SQLITE_FLOAT:
      value.type = SqlValue::kDouble;
      value.double_value = sqlite3_value_double(sqlite_val);
      break;
    case SQLITE_BLOB:
      value.type = SqlValue::kBytes;
      value.bytes_value = sqlite3_value_blob(sqlite_val);
      value.bytes_count = static_cast<size_t>(sqlite3_value_bytes(sqlite_val));
      break;
    case SQLITE_NULL:
      value.type = SqlValue::kNull;
      break;
  }
  return value;
}

BitVector ColsUsedBitVector(uint64_t sqlite_cols_used, size_t col_count) {
  return BitVector::Range(
      0, static_cast<uint32_t>(col_count), [sqlite_cols_used](uint32_t idx) {
        // If the lowest bit of |sqlite_cols_used| is set, the first column is
        // used. The second lowest bit corresponds to the second column etc. If
        // the most significant bit of |sqlite_cols_used| is set, that means
        // that any column after the first 63 columns could be used.
        return sqlite_cols_used & (1ull << std::min(idx, 63u));
      });
}

class SafeStringWriter {
 public:
  SafeStringWriter() {}
  ~SafeStringWriter() {}

  void AppendString(const char* s) {
    for (const char* c = s; *c; ++c) {
      buffer_.emplace_back(*c);
    }
  }

  void AppendString(const std::string& s) {
    for (char c : s) {
      buffer_.emplace_back(c);
    }
  }

  base::StringView GetStringView() const {
    return base::StringView(buffer_.data(), buffer_.size());
  }

 private:
  base::SmallVector<char, 2048> buffer_;
};

}  // namespace

DbSqliteTable::DbSqliteTable(sqlite3*, Context context)
    : cache_(context.cache),
      computation_(context.computation),
      static_table_(context.static_table),
      generator_(std::move(context.generator)) {}
DbSqliteTable::~DbSqliteTable() = default;

void DbSqliteTable::RegisterTable(sqlite3* db,
                                  QueryCache* cache,
                                  const Table* table,
                                  const std::string& name) {
  Context context{cache, TableComputation::kStatic, table, nullptr};
  SqliteTable::Register<DbSqliteTable, Context>(db, std::move(context), name);
}

void DbSqliteTable::RegisterTable(sqlite3* db,
                                  QueryCache* cache,
                                  std::unique_ptr<TableFunction> generator) {
  // Figure out if the table needs explicit args (in the form of constraints
  // on hidden columns) passed to it in order to make the query valid.
  base::Status status = generator->ValidateConstraints(
      QueryConstraints(std::numeric_limits<uint64_t>::max()));
  bool requires_args = !status.ok();

  std::string table_name = generator->TableName();
  Context context{cache, TableComputation::kDynamic, nullptr,
                  std::move(generator)};
  SqliteTable::Register<DbSqliteTable, Context>(
      db, std::move(context), table_name, false, requires_args);
}

base::Status DbSqliteTable::Init(int, const char* const*, Schema* schema) {
  switch (computation_) {
    case TableComputation::kStatic:
      schema_ = static_table_->ComputeSchema();
      break;
    case TableComputation::kDynamic:
      schema_ = generator_->CreateSchema();
      break;
  }
  *schema = ComputeSchema(schema_, name().c_str());
  return base::OkStatus();
}

SqliteTable::Schema DbSqliteTable::ComputeSchema(const Table::Schema& schema,
                                                 const char* table_name) {
  std::vector<SqliteTable::Column> schema_cols;
  for (uint32_t i = 0; i < schema.columns.size(); ++i) {
    const auto& col = schema.columns[i];
    schema_cols.emplace_back(i, col.name, col.type, col.is_hidden);
  }

  // TODO(lalitm): this is hardcoded to be the id column but change this to be
  // more generic in the future.
  auto it = std::find_if(
      schema.columns.begin(), schema.columns.end(),
      [](const Table::Schema::Column& c) { return c.name == "id"; });
  if (it == schema.columns.end()) {
    PERFETTO_FATAL(
        "id column not found in %s. Currently all db Tables need to contain an "
        "id column; this constraint will be relaxed in the future.",
        table_name);
  }

  std::vector<size_t> primary_keys;
  primary_keys.emplace_back(std::distance(schema.columns.begin(), it));
  return Schema(std::move(schema_cols), std::move(primary_keys));
}

int DbSqliteTable::BestIndex(const QueryConstraints& qc, BestIndexInfo* info) {
  switch (computation_) {
    case TableComputation::kStatic:
      BestIndex(schema_, static_table_->row_count(), qc, info);
      break;
    case TableComputation::kDynamic:
      base::Status status = generator_->ValidateConstraints(qc);
      if (!status.ok())
        return SQLITE_CONSTRAINT;
      BestIndex(schema_, generator_->EstimateRowCount(), qc, info);
      break;
  }
  return SQLITE_OK;
}

void DbSqliteTable::BestIndex(const Table::Schema& schema,
                              uint32_t row_count,
                              const QueryConstraints& qc,
                              BestIndexInfo* info) {
  auto cost_and_rows = EstimateCost(schema, row_count, qc);
  info->estimated_cost = cost_and_rows.cost;
  info->estimated_rows = cost_and_rows.rows;

  const auto& cs = qc.constraints();
  for (uint32_t i = 0; i < cs.size(); ++i) {
    // SqliteOpToFilterOp will return std::nullopt for any constraint which we
    // don't support filtering ourselves. Only omit filtering by SQLite when we
    // can handle filtering.
    std::optional<FilterOp> opt_op = SqliteOpToFilterOp(cs[i].op);
    info->sqlite_omit_constraint[i] = opt_op.has_value();
  }

  // We can sort on any column correctly.
  info->sqlite_omit_order_by = true;
}

int DbSqliteTable::ModifyConstraints(QueryConstraints* qc) {
  ModifyConstraints(schema_, qc);
  return SQLITE_OK;
}

void DbSqliteTable::ModifyConstraints(const Table::Schema& schema,
                                      QueryConstraints* qc) {
  using C = QueryConstraints::Constraint;

  // Reorder constraints to consider the constraints on columns which are
  // cheaper to filter first.
  auto* cs = qc->mutable_constraints();
  std::sort(cs->begin(), cs->end(), [&schema](const C& a, const C& b) {
    uint32_t a_idx = static_cast<uint32_t>(a.column);
    uint32_t b_idx = static_cast<uint32_t>(b.column);
    const auto& a_col = schema.columns[a_idx];
    const auto& b_col = schema.columns[b_idx];

    // Id columns are always very cheap to filter on so try and get them
    // first.
    if (a_col.is_id || b_col.is_id)
      return a_col.is_id && !b_col.is_id;

    // Set id columns are always very cheap to filter on so try and get them
    // second.
    if (a_col.is_set_id || b_col.is_set_id)
      return a_col.is_set_id && !b_col.is_set_id;

    // Sorted columns are also quite cheap to filter so order them after
    // any id/set id columns.
    if (a_col.is_sorted || b_col.is_sorted)
      return a_col.is_sorted && !b_col.is_sorted;

    // TODO(lalitm): introduce more orderings here based on empirical data.
    return false;
  });

  // Remove any order by constraints which also have an equality constraint.
  auto* ob = qc->mutable_order_by();
  {
    auto p = [&cs](const QueryConstraints::OrderBy& o) {
      auto inner_p = [&o](const QueryConstraints::Constraint& c) {
        return c.column == o.iColumn && sqlite_utils::IsOpEq(c.op);
      };
      return std::any_of(cs->begin(), cs->end(), inner_p);
    };
    auto remove_it = std::remove_if(ob->begin(), ob->end(), p);
    ob->erase(remove_it, ob->end());
  }

  // Go through the order by constraints in reverse order and eliminate
  // constraints until the first non-sorted column or the first order by in
  // descending order.
  {
    auto p = [&schema](const QueryConstraints::OrderBy& o) {
      const auto& col = schema.columns[static_cast<uint32_t>(o.iColumn)];
      return o.desc || !col.is_sorted;
    };
    auto first_non_sorted_it = std::find_if(ob->rbegin(), ob->rend(), p);
    auto pop_count = std::distance(ob->rbegin(), first_non_sorted_it);
    ob->resize(ob->size() - static_cast<uint32_t>(pop_count));
  }
}

DbSqliteTable::QueryCost DbSqliteTable::EstimateCost(
    const Table::Schema& schema,
    uint32_t row_count,
    const QueryConstraints& qc) {
  // Currently our cost estimation algorithm is quite simplistic but is good
  // enough for the simplest cases.
  // TODO(lalitm): replace hardcoded constants with either more heuristics
  // based on the exact type of constraint or profiling the queries themselves.

  // We estimate the fixed cost of set-up and tear-down of a query in terms of
  // the number of rows scanned.
  constexpr double kFixedQueryCost = 1000.0;

  // Setup the variables for estimating the number of rows we will have at the
  // end of filtering. Note that |current_row_count| should always be at least 1
  // unless we are absolutely certain that we will return no rows as otherwise
  // SQLite can make some bad choices.
  uint32_t current_row_count = row_count;

  // If the table is empty, any constraint set only pays the fixed cost. Also we
  // can return 0 as the row count as we are certain that we will return no
  // rows.
  if (current_row_count == 0)
    return QueryCost{kFixedQueryCost, 0};

  // Setup the variables for estimating the cost of filtering.
  double filter_cost = 0.0;
  const auto& cs = qc.constraints();
  for (const auto& c : cs) {
    if (current_row_count < 2)
      break;
    const auto& col_schema = schema.columns[static_cast<uint32_t>(c.column)];
    if (sqlite_utils::IsOpEq(c.op) && col_schema.is_id) {
      // If we have an id equality constraint, we can very efficiently filter
      // down to a single row in C++. However, if we're joining with another
      // table, SQLite will do this once per row which can be extremely
      // expensive because of all the virtual table (which is implemented using
      // virtual function calls) machinery. Indicate this by saying that an
      // entire filter call is ~10x the cost of iterating a single row.
      filter_cost += 10;
      current_row_count = 1;
    } else if (sqlite_utils::IsOpEq(c.op)) {
      // If there is only a single equality constraint, we have special logic
      // to sort by that column and then binary search if we see the constraint
      // set often. Model this by dividing by the log of the number of rows as
      // a good approximation. Otherwise, we'll need to do a full table scan.
      // Alternatively, if the column is sorted, we can use the same binary
      // search logic so we have the same low cost (even better because we don't
      // have to sort at all).
      filter_cost += cs.size() == 1 || col_schema.is_sorted
                         ? log2(current_row_count)
                         : current_row_count;

      // As an extremely rough heuristic, assume that an equalty constraint will
      // cut down the number of rows by approximately double log of the number
      // of rows.
      double estimated_rows = current_row_count / (2 * log2(current_row_count));
      current_row_count = std::max(static_cast<uint32_t>(estimated_rows), 1u);
    } else if (col_schema.is_sorted &&
               (sqlite_utils::IsOpLe(c.op) || sqlite_utils::IsOpLt(c.op) ||
                sqlite_utils::IsOpGt(c.op) || sqlite_utils::IsOpGe(c.op))) {
      // On a sorted column, if we see any partition constraints, we can do this
      // filter very efficiently. Model this using the log of the  number of
      // rows as a good approximation.
      filter_cost += log2(current_row_count);

      // As an extremely rough heuristic, assume that an partition constraint
      // will cut down the number of rows by approximately double log of the
      // number of rows.
      double estimated_rows = current_row_count / (2 * log2(current_row_count));
      current_row_count = std::max(static_cast<uint32_t>(estimated_rows), 1u);
    } else {
      // Otherwise, we will need to do a full table scan and we estimate we will
      // maybe (at best) halve the number of rows.
      filter_cost += current_row_count;
      current_row_count = std::max(current_row_count / 2u, 1u);
    }
  }

  // Now, to figure out the cost of sorting, multiply the final row count
  // by |qc.order_by().size()| * log(row count). This should act as a crude
  // estimation of the cost.
  double sort_cost =
      static_cast<double>(qc.order_by().size() * current_row_count) *
      log2(current_row_count);

  // The cost of iterating rows is more expensive than just filtering the rows
  // so multiply by an appropriate factor.
  double iteration_cost = current_row_count * 2.0;

  // To get the final cost, add up all the individual components.
  double final_cost =
      kFixedQueryCost + filter_cost + sort_cost + iteration_cost;
  return QueryCost{final_cost, current_row_count};
}

std::unique_ptr<SqliteTable::Cursor> DbSqliteTable::CreateCursor() {
  return std::unique_ptr<Cursor>(new Cursor(this, cache_));
}

DbSqliteTable::Cursor::Cursor(DbSqliteTable* sqlite_table, QueryCache* cache)
    : SqliteTable::Cursor(sqlite_table),
      db_sqlite_table_(sqlite_table),
      cache_(cache) {}

void DbSqliteTable::Cursor::TryCacheCreateSortedTable(
    const QueryConstraints& qc,
    FilterHistory history) {
  // Check if we have a cache. Some subclasses (e.g. the flamegraph table) may
  // pass nullptr to disable caching.
  if (!cache_)
    return;

  if (history == FilterHistory::kDifferent) {
    repeated_cache_count_ = 0;

    // Check if the new constraint set is cached by another cursor.
    sorted_cache_table_ =
        cache_->GetIfCached(upstream_table_, qc.constraints());
    return;
  }

  PERFETTO_DCHECK(history == FilterHistory::kSame);

  // TODO(lalitm): all of the caching policy below should live in QueryCache and
  // not here. This is only here temporarily to allow migration of sched without
  // regressing UI performance and should be removed ASAP.

  // Only try and create the cached table on exactly the third time we see this
  // constraint set.
  constexpr uint32_t kRepeatedThreshold = 3;
  if (sorted_cache_table_ || repeated_cache_count_++ != kRepeatedThreshold)
    return;

  // If we have more than one constraint, we can't cache the table using
  // this method.
  if (qc.constraints().size() != 1)
    return;

  // If the constraing is not an equality constraint, there's little
  // benefit to caching
  const auto& c = qc.constraints().front();
  if (!sqlite_utils::IsOpEq(c.op))
    return;

  // If the column is already sorted, we don't need to cache at all.
  uint32_t col = static_cast<uint32_t>(c.column);
  if (upstream_table_->GetColumn(col).IsSorted())
    return;

  // Try again to get the result or start caching it.
  sorted_cache_table_ =
      cache_->GetOrCache(upstream_table_, qc.constraints(), [this, col]() {
        return upstream_table_->Sort({Order{col, false}});
      });
}

int DbSqliteTable::Cursor::Filter(const QueryConstraints& qc,
                                  sqlite3_value** argv,
                                  FilterHistory history) {
  // Clear out the iterator before filtering to ensure the destructor is run
  // before the table's destructor.
  iterator_ = std::nullopt;

  // We reuse this vector to reduce memory allocations on nested subqueries.
  constraints_.resize(qc.constraints().size());
  uint32_t constraints_pos = 0;
  for (size_t i = 0; i < qc.constraints().size(); ++i) {
    const auto& cs = qc.constraints()[i];
    uint32_t col = static_cast<uint32_t>(cs.column);

    // If we get a std::nullopt FilterOp, that means we should allow SQLite
    // to handle the constraint.
    std::optional<FilterOp> opt_op = SqliteOpToFilterOp(cs.op);
    if (!opt_op)
      continue;

    SqlValue value = SqliteValueToSqlValue(argv[i]);
    constraints_[constraints_pos++] = Constraint{col, *opt_op, value};
  }
  constraints_.resize(constraints_pos);

  // We reuse this vector to reduce memory allocations on nested subqueries.
  orders_.resize(qc.order_by().size());
  for (size_t i = 0; i < qc.order_by().size(); ++i) {
    const auto& ob = qc.order_by()[i];
    uint32_t col = static_cast<uint32_t>(ob.iColumn);
    orders_[i] = Order{col, static_cast<bool>(ob.desc)};
  }

  // Setup the upstream table based on the computation state.
  switch (db_sqlite_table_->computation_) {
    case TableComputation::kStatic:
      // If we have a static table, just set the upstream table to be the static
      // table.
      upstream_table_ = db_sqlite_table_->static_table_;

      // Tries to create a sorted cached table which can be used to speed up
      // filters below.
      TryCacheCreateSortedTable(qc, history);
      break;
    case TableComputation::kDynamic: {
      PERFETTO_TP_TRACE(metatrace::Category::QUERY, "DYNAMIC_TABLE_GENERATE",
                        [this](metatrace::Record* r) {
                          r->AddArg("Table", db_sqlite_table_->name());
                        });
      // If we have a dynamically created table, regenerate the table based on
      // the new constraints.
      std::unique_ptr<Table> computed_table;
      BitVector cols_used_bv = ColsUsedBitVector(
          qc.cols_used(), db_sqlite_table_->schema_.columns.size());
      auto status = db_sqlite_table_->generator_->ComputeTable(
          constraints_, orders_, cols_used_bv, computed_table);

      if (!status.ok()) {
        auto* sqlite_err = sqlite3_mprintf(
            "%s: %s", db_sqlite_table_->name().c_str(), status.c_message());
        db_sqlite_table_->SetErrorMessage(sqlite_err);
        return SQLITE_CONSTRAINT;
      }
      PERFETTO_DCHECK(computed_table);
      dynamic_table_ = std::move(computed_table);
      upstream_table_ = dynamic_table_.get();
      break;
    }
  }

  PERFETTO_TP_TRACE(
      metatrace::Category::QUERY, "DB_TABLE_FILTER_AND_SORT",
      [this](metatrace::Record* r) {
        const Table* source = SourceTable();
        r->AddArg("Table", db_sqlite_table_->name());
        for (const Constraint& c : constraints_) {
          SafeStringWriter writer;
          writer.AppendString(source->GetColumn(c.col_idx).name());

          writer.AppendString(" ");
          switch (c.op) {
            case FilterOp::kEq:
              writer.AppendString("=");
              break;
            case FilterOp::kGe:
              writer.AppendString(">=");
              break;
            case FilterOp::kGt:
              writer.AppendString(">");
              break;
            case FilterOp::kLe:
              writer.AppendString("<=");
              break;
            case FilterOp::kLt:
              writer.AppendString("<");
              break;
            case FilterOp::kNe:
              writer.AppendString("!=");
              break;
            case FilterOp::kIsNull:
              writer.AppendString("IS");
              break;
            case FilterOp::kIsNotNull:
              writer.AppendString("IS NOT");
              break;
            case FilterOp::kGlob:
              writer.AppendString("GLOB");
              break;
          }
          writer.AppendString(" ");

          switch (c.value.type) {
            case SqlValue::kString:
              writer.AppendString(c.value.AsString());
              break;
            case SqlValue::kBytes:
              writer.AppendString("<bytes>");
              break;
            case SqlValue::kNull:
              writer.AppendString("<null>");
              break;
            case SqlValue::kDouble: {
              writer.AppendString(std::to_string(c.value.AsDouble()));
              break;
            }
            case SqlValue::kLong: {
              writer.AppendString(std::to_string(c.value.AsLong()));
              break;
            }
          }
          r->AddArg("Constraint", writer.GetStringView());
        }

        for (const auto& o : orders_) {
          SafeStringWriter writer;
          writer.AppendString(source->GetColumn(o.col_idx).name());
          if (o.desc)
            writer.AppendString(" desc");
          r->AddArg("Order by", writer.GetStringView());
        }
      });

  // Attempt to filter into a RowMap first - weall figure out whether to apply
  // this to the table or we should use the RowMap directly. Also, if we are
  // going to sort on the RowMap, it makes sense that we optimize for lookup
  // speed so our sorting is not super slow.
  RowMap::OptimizeFor optimize_for = orders_.empty()
                                         ? RowMap::OptimizeFor::kMemory
                                         : RowMap::OptimizeFor::kLookupSpeed;
  RowMap filter_map = SourceTable()->FilterToRowMap(constraints_, optimize_for);

  // If we have no order by constraints and it's cheap for us to use the
  // RowMap, just use the RowMap directoy.
  if (filter_map.IsRange() && filter_map.size() <= 1) {
    // Currently, our criteria where we have a special fast path is if it's
    // a single ranged row. We have tihs fast path for joins on id columns
    // where we get repeated queries filtering down to a single row. The
    // other path performs allocations when creating the new table as well
    // as the iterator on the new table whereas this path only uses a single
    // number and lives entirely on the stack.

    // TODO(lalitm): investigate some other criteria where it is beneficial
    // to have a fast path and expand to them.
    mode_ = Mode::kSingleRow;
    single_row_ = filter_map.size() == 1 ? std::make_optional(filter_map.Get(0))
                                         : std::nullopt;
    eof_ = !single_row_.has_value();
  } else {
    mode_ = Mode::kTable;

    db_table_ = SourceTable()->Apply(std::move(filter_map));
    if (!orders_.empty())
      db_table_ = db_table_->Sort(orders_);

    iterator_ = db_table_->IterateRows();

    eof_ = !*iterator_;
  }

  return SQLITE_OK;
}

int DbSqliteTable::Cursor::Next() {
  if (mode_ == Mode::kSingleRow) {
    eof_ = true;
  } else {
    iterator_->Next();
    eof_ = !*iterator_;
  }
  return SQLITE_OK;
}

int DbSqliteTable::Cursor::Eof() {
  return eof_;
}

int DbSqliteTable::Cursor::Column(sqlite3_context* ctx, int raw_col) {
  uint32_t column = static_cast<uint32_t>(raw_col);
  SqlValue value = mode_ == Mode::kSingleRow
                       ? SourceTable()->GetColumn(column).Get(*single_row_)
                       : iterator_->Get(column);
  // We can say kSqliteStatic for strings  because all strings are expected to
  // come from the string pool and thus will be valid for the lifetime
  // of trace processor.
  // Similarily for bytes we can also use kSqliteStatic because for our iterator
  // will hold onto the pointer as long as we don't call Next() but that only
  // happens with Next() is called on the Cursor itself at which point
  // SQLite no longer cares about the bytes pointer.
  sqlite_utils::ReportSqlValue(ctx, value, sqlite_utils::kSqliteStatic,
                               sqlite_utils::kSqliteStatic);
  return SQLITE_OK;
}

}  // namespace trace_processor
}  // namespace perfetto
