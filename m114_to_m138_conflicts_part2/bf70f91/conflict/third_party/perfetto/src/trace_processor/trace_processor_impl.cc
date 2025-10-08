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

#include "src/trace_processor/trace_processor_impl.h"

#include <algorithm>
<<<<<<< HEAD
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"
#include "perfetto/base/status.h"
#include "perfetto/base/thread_utils.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/clock_snapshots.h"
#include "perfetto/ext/base/flat_hash_map.h"
#include "perfetto/ext/base/small_vector.h"
#include "perfetto/ext/base/status_or.h"
#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "perfetto/public/compiler.h"
#include "perfetto/trace_processor/basic_types.h"
#include "perfetto/trace_processor/iterator.h"
#include "perfetto/trace_processor/trace_blob_view.h"
#include "perfetto/trace_processor/trace_processor.h"
#include "src/trace_processor/importers/android_bugreport/android_dumpstate_event_parser_impl.h"
#include "src/trace_processor/importers/android_bugreport/android_dumpstate_reader.h"
#include "src/trace_processor/importers/android_bugreport/android_log_event_parser_impl.h"
#include "src/trace_processor/importers/android_bugreport/android_log_reader.h"
#include "src/trace_processor/importers/archive/gzip_trace_parser.h"
#include "src/trace_processor/importers/archive/tar_trace_reader.h"
#include "src/trace_processor/importers/archive/zip_trace_reader.h"
#include "src/trace_processor/importers/art_hprof/art_hprof_parser.h"
#include "src/trace_processor/importers/art_method/art_method_parser_impl.h"
#include "src/trace_processor/importers/art_method/art_method_tokenizer.h"
#include "src/trace_processor/importers/common/clock_tracker.h"
#include "src/trace_processor/importers/common/trace_file_tracker.h"
#include "src/trace_processor/importers/common/trace_parser.h"
#include "src/trace_processor/importers/fuchsia/fuchsia_trace_parser.h"
#include "src/trace_processor/importers/fuchsia/fuchsia_trace_tokenizer.h"
#include "src/trace_processor/importers/gecko/gecko_trace_parser_impl.h"
#include "src/trace_processor/importers/gecko/gecko_trace_tokenizer.h"
#include "src/trace_processor/importers/json/json_trace_parser_impl.h"
#include "src/trace_processor/importers/json/json_trace_tokenizer.h"
#include "src/trace_processor/importers/json/json_utils.h"
#include "src/trace_processor/importers/ninja/ninja_log_parser.h"
#include "src/trace_processor/importers/perf/perf_data_tokenizer.h"
#include "src/trace_processor/importers/perf/perf_event.h"
#include "src/trace_processor/importers/perf/perf_tracker.h"
#include "src/trace_processor/importers/perf/record_parser.h"
#include "src/trace_processor/importers/perf/spe_record_parser.h"
#include "src/trace_processor/importers/perf_text/perf_text_trace_parser_impl.h"
#include "src/trace_processor/importers/perf_text/perf_text_trace_tokenizer.h"
#include "src/trace_processor/importers/proto/additional_modules.h"
#include "src/trace_processor/importers/systrace/systrace_trace_parser.h"
#include "src/trace_processor/iterator_impl.h"
#include "src/trace_processor/metrics/all_chrome_metrics.descriptor.h"
#include "src/trace_processor/metrics/all_webview_metrics.descriptor.h"
#include "src/trace_processor/metrics/metrics.descriptor.h"
#include "src/trace_processor/metrics/metrics.h"
#include "src/trace_processor/metrics/sql/amalgamated_sql_metrics.h"
#include "src/trace_processor/perfetto_sql/engine/perfetto_sql_engine.h"
#include "src/trace_processor/perfetto_sql/engine/table_pointer_module.h"
#include "src/trace_processor/perfetto_sql/generator/structured_query_generator.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/base64.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/clock_functions.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/counter_intervals.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/create_function.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/create_view_function.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/dominator_tree.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/graph_scan.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/graph_traversal.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/import.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/interval_intersect.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/layout_functions.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/math.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/pprof_functions.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/replace_numbers_function.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/sqlite3_str_split.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/stack_functions.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/structural_tree_partition.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/to_ftrace.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/type_builders.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/utils.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/window_functions.h"
#include "src/trace_processor/perfetto_sql/intrinsics/operators/counter_mipmap_operator.h"
#include "src/trace_processor/perfetto_sql/intrinsics/operators/slice_mipmap_operator.h"
#include "src/trace_processor/perfetto_sql/intrinsics/operators/span_join_operator.h"
#include "src/trace_processor/perfetto_sql/intrinsics/operators/window_operator.h"
#include "src/trace_processor/perfetto_sql/intrinsics/table_functions/ancestor.h"
#include "src/trace_processor/perfetto_sql/intrinsics/table_functions/connected_flow.h"
#include "src/trace_processor/perfetto_sql/intrinsics/table_functions/descendant.h"
#include "src/trace_processor/perfetto_sql/intrinsics/table_functions/dfs_weight_bounded.h"
#include "src/trace_processor/perfetto_sql/intrinsics/table_functions/experimental_annotated_stack.h"
#include "src/trace_processor/perfetto_sql/intrinsics/table_functions/experimental_flamegraph.h"
#include "src/trace_processor/perfetto_sql/intrinsics/table_functions/experimental_flat_slice.h"
#include "src/trace_processor/perfetto_sql/intrinsics/table_functions/experimental_slice_layout.h"
#include "src/trace_processor/perfetto_sql/intrinsics/table_functions/table_info.h"
#include "src/trace_processor/perfetto_sql/intrinsics/table_functions/winscope_proto_to_args_with_defaults.h"
#include "src/trace_processor/perfetto_sql/stdlib/stdlib.h"
#include "src/trace_processor/sqlite/bindings/sqlite_aggregate_function.h"
#include "src/trace_processor/sqlite/bindings/sqlite_result.h"
#include "src/trace_processor/sqlite/sql_source.h"
#include "src/trace_processor/sqlite/sql_stats_table.h"
#include "src/trace_processor/sqlite/stats_table.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/tp_metatrace.h"
#include "src/trace_processor/trace_processor_storage_impl.h"
#include "src/trace_processor/trace_reader_registry.h"
#include "src/trace_processor/trace_summary/summary.h"
#include "src/trace_processor/trace_summary/trace_summary.descriptor.h"
#include "src/trace_processor/types/trace_processor_context.h"
#include "src/trace_processor/util/descriptors.h"
#include "src/trace_processor/util/gzip_utils.h"
#include "src/trace_processor/util/protozero_to_json.h"
#include "src/trace_processor/util/protozero_to_text.h"
#include "src/trace_processor/util/regex.h"
#include "src/trace_processor/util/sql_modules.h"
#include "src/trace_processor/util/status_macros.h"
#include "src/trace_processor/util/trace_type.h"

=======
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "perfetto/base/logging.h"
#include "perfetto/base/status.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/flat_hash_map.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/trace_processor/basic_types.h"
#include "src/trace_processor/importers/android_bugreport/android_bugreport_parser.h"
#include "src/trace_processor/importers/common/clock_converter.h"
#include "src/trace_processor/importers/common/clock_tracker.h"
#include "src/trace_processor/importers/common/metadata_tracker.h"
#include "src/trace_processor/importers/ftrace/sched_event_tracker.h"
#include "src/trace_processor/importers/fuchsia/fuchsia_trace_parser.h"
#include "src/trace_processor/importers/fuchsia/fuchsia_trace_tokenizer.h"
#include "src/trace_processor/importers/gzip/gzip_trace_parser.h"
#include "src/trace_processor/importers/json/json_trace_parser.h"
#include "src/trace_processor/importers/json/json_trace_tokenizer.h"
#include "src/trace_processor/importers/json/json_utils.h"
#include "src/trace_processor/importers/ninja/ninja_log_parser.h"
#include "src/trace_processor/importers/proto/additional_modules.h"
#include "src/trace_processor/importers/proto/content_analyzer.h"
#include "src/trace_processor/importers/systrace/systrace_trace_parser.h"
#include "src/trace_processor/iterator_impl.h"
#include "src/trace_processor/prelude/functions/clock_functions.h"
#include "src/trace_processor/prelude/functions/create_function.h"
#include "src/trace_processor/prelude/functions/create_view_function.h"
#include "src/trace_processor/prelude/functions/import.h"
#include "src/trace_processor/prelude/functions/layout_functions.h"
#include "src/trace_processor/prelude/functions/pprof_functions.h"
#include "src/trace_processor/prelude/functions/register_function.h"
#include "src/trace_processor/prelude/functions/sqlite3_str_split.h"
#include "src/trace_processor/prelude/functions/stack_functions.h"
#include "src/trace_processor/prelude/functions/to_ftrace.h"
#include "src/trace_processor/prelude/functions/utils.h"
#include "src/trace_processor/prelude/functions/window_functions.h"
#include "src/trace_processor/prelude/operators/span_join_operator.h"
#include "src/trace_processor/prelude/operators/window_operator.h"
#include "src/trace_processor/prelude/table_functions/ancestor.h"
#include "src/trace_processor/prelude/table_functions/connected_flow.h"
#include "src/trace_processor/prelude/table_functions/descendant.h"
#include "src/trace_processor/prelude/table_functions/experimental_annotated_stack.h"
#include "src/trace_processor/prelude/table_functions/experimental_counter_dur.h"
#include "src/trace_processor/prelude/table_functions/experimental_flamegraph.h"
#include "src/trace_processor/prelude/table_functions/experimental_flat_slice.h"
#include "src/trace_processor/prelude/table_functions/experimental_sched_upid.h"
#include "src/trace_processor/prelude/table_functions/experimental_slice_layout.h"
#include "src/trace_processor/prelude/table_functions/table_function.h"
#include "src/trace_processor/prelude/table_functions/view.h"
#include "src/trace_processor/prelude/tables_views/tables_views.h"
#include "src/trace_processor/sqlite/scoped_db.h"
#include "src/trace_processor/sqlite/sql_stats_table.h"
#include "src/trace_processor/sqlite/sqlite_table.h"
#include "src/trace_processor/sqlite/sqlite_utils.h"
#include "src/trace_processor/sqlite/stats_table.h"
#include "src/trace_processor/tp_metatrace.h"
#include "src/trace_processor/types/variadic.h"
#include "src/trace_processor/util/protozero_to_text.h"
#include "src/trace_processor/util/sql_modules.h"
#include "src/trace_processor/util/status_macros.h"

#include "protos/perfetto/common/builtin_clock.pbzero.h"
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
#include "protos/perfetto/trace/clock_snapshot.pbzero.h"
#include "protos/perfetto/trace/perfetto/perfetto_metatrace.pbzero.h"
#include "protos/perfetto/trace/trace.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

<<<<<<< HEAD
#if PERFETTO_BUILDFLAG(PERFETTO_TP_INSTRUMENTS)
#include "src/trace_processor/importers/instruments/instruments_xml_tokenizer.h"
#include "src/trace_processor/importers/instruments/row_parser.h"
#endif

#if PERFETTO_BUILDFLAG(PERFETTO_ENABLE_ETM_IMPORTER)
#include "src/trace_processor/importers/etm/etm_tracker.h"
#include "src/trace_processor/importers/etm/etm_v4_stream_demultiplexer.h"
#include "src/trace_processor/importers/etm/file_tracker.h"
#include "src/trace_processor/perfetto_sql/intrinsics/operators/etm_decode_trace_vtable.h"
#include "src/trace_processor/perfetto_sql/intrinsics/operators/etm_iterate_range_vtable.h"
#endif

namespace perfetto::trace_processor {
namespace {

template <typename SqlFunction, typename Ptr = typename SqlFunction::Context*>
void RegisterFunction(PerfettoSqlEngine* engine,
=======
#include "src/trace_processor/metrics/all_chrome_metrics.descriptor.h"
#include "src/trace_processor/metrics/all_webview_metrics.descriptor.h"
#include "src/trace_processor/metrics/metrics.descriptor.h"
#include "src/trace_processor/metrics/metrics.h"
#include "src/trace_processor/metrics/sql/amalgamated_sql_metrics.h"
#include "src/trace_processor/stdlib/amalgamated_stdlib.h"

// In Android and Chromium tree builds, we don't have the percentile module.
// Just don't include it.
#if PERFETTO_BUILDFLAG(PERFETTO_TP_PERCENTILE)
// defined in sqlite_src/ext/misc/percentile.c
extern "C" int sqlite3_percentile_init(sqlite3* db,
                                       char** error,
                                       const sqlite3_api_routines* api);
#endif  // PERFETTO_BUILDFLAG(PERFETTO_TP_PERCENTILE)

namespace perfetto {
namespace trace_processor {
namespace {

const char kAllTablesQuery[] =
    "SELECT tbl_name, type FROM (SELECT * FROM sqlite_master UNION ALL SELECT "
    "* FROM sqlite_temp_master)";

template <typename SqlFunction, typename Ptr = typename SqlFunction::Context*>
void RegisterFunction(sqlite3* db,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
                      const char* name,
                      int argc,
                      Ptr context = nullptr,
                      bool deterministic = true) {
<<<<<<< HEAD
  auto status = engine->RegisterStaticFunction<SqlFunction>(
      name, argc, std::move(context), deterministic);
=======
  auto status = RegisterSqlFunction<SqlFunction>(
      db, name, argc, std::move(context), deterministic);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  if (!status.ok())
    PERFETTO_ELOG("%s", status.c_message());
}

<<<<<<< HEAD
base::Status RegisterAllProtoBuilderFunctions(
    DescriptorPool* pool,
    std::unordered_map<std::string, std::string>* proto_fn_name_to_path,
    PerfettoSqlEngine* engine,
    TraceProcessor* tp) {
  for (uint32_t i = 0; i < pool->descriptors().size(); ++i) {
    // Convert the full name (e.g. .perfetto.protos.TraceMetrics.SubMetric)
    // into a function name of the form (TraceMetrics_SubMetric).
    const auto& desc = pool->descriptors()[i];
    auto fn_name = desc.full_name().substr(desc.package_name().size() + 1);
    std::replace(fn_name.begin(), fn_name.end(), '.', '_');
    auto registered_fn = proto_fn_name_to_path->find(fn_name);
    if (registered_fn != proto_fn_name_to_path->end() &&
        registered_fn->second != desc.full_name()) {
      return base::ErrStatus(
          "Attempt to create new metric function '%s' for different descriptor "
          "'%s' that conflicts with '%s'",
          fn_name.c_str(), desc.full_name().c_str(),
          registered_fn->second.c_str());
    }
    RegisterFunction<metrics::BuildProto>(
        engine, fn_name.c_str(), -1,
        std::make_unique<metrics::BuildProto::Context>(
            metrics::BuildProto::Context{tp, pool, i}));
    proto_fn_name_to_path->emplace(fn_name, desc.full_name());
  }
  return base::OkStatus();
=======
void InitializeSqlite(sqlite3* db) {
  char* error = nullptr;
  sqlite3_exec(db, "PRAGMA temp_store=2", nullptr, nullptr, &error);
  if (error) {
    PERFETTO_FATAL("Error setting pragma temp_store: %s", error);
  }
  sqlite3_str_split_init(db);
// In Android tree builds, we don't have the percentile module.
// Just don't include it.
#if PERFETTO_BUILDFLAG(PERFETTO_TP_PERCENTILE)
  sqlite3_percentile_init(db, &error, nullptr);
  if (error) {
    PERFETTO_ELOG("Error initializing: %s", error);
    sqlite3_free(error);
  }
#endif
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
}

void BuildBoundsTable(sqlite3* db, std::pair<int64_t, int64_t> bounds) {
  char* error = nullptr;
<<<<<<< HEAD
  sqlite3_exec(db, "DELETE FROM _trace_bounds", nullptr, nullptr, &error);
=======
  sqlite3_exec(db, "DELETE FROM trace_bounds", nullptr, nullptr, &error);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  if (error) {
    PERFETTO_ELOG("Error deleting from bounds table: %s", error);
    sqlite3_free(error);
    return;
  }

<<<<<<< HEAD
  base::StackString<1024> sql("INSERT INTO _trace_bounds VALUES(%" PRId64
=======
  base::StackString<1024> sql("INSERT INTO trace_bounds VALUES(%" PRId64
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
                              ", %" PRId64 ")",
                              bounds.first, bounds.second);
  sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error);
  if (error) {
    PERFETTO_ELOG("Error inserting bounds table: %s", error);
    sqlite3_free(error);
  }
}

<<<<<<< HEAD
class ValueAtMaxTs : public SqliteAggregateFunction<ValueAtMaxTs> {
 public:
  static constexpr char kName[] = "VALUE_AT_MAX_TS";
  static constexpr int kArgCount = 2;
  struct Context {
    bool initialized;
    int value_type;

    int64_t max_ts;
    int64_t int_value_at_max_ts;
    double double_value_at_max_ts;
  };

  static void Step(sqlite3_context* ctx, int, sqlite3_value** argv) {
    sqlite3_value* ts = argv[0];
    sqlite3_value* value = argv[1];

    // Note that sqlite3_aggregate_context zeros the memory for us so all the
    // variables of the struct should be zero.
    auto* fn_ctx = reinterpret_cast<Context*>(
        sqlite3_aggregate_context(ctx, sizeof(Context)));

    // For performance reasons, we only do the check for the type of ts and
    // value on the first call of the function.
    if (PERFETTO_UNLIKELY(!fn_ctx->initialized)) {
      if (sqlite3_value_type(ts) != SQLITE_INTEGER) {
        return sqlite::result::Error(
            ctx, "VALUE_AT_MAX_TS: ts passed was not an integer");
      }

      fn_ctx->value_type = sqlite3_value_type(value);
      if (fn_ctx->value_type != SQLITE_INTEGER &&
          fn_ctx->value_type != SQLITE_FLOAT) {
        return sqlite::result::Error(
            ctx, "VALUE_AT_MAX_TS: value passed was not an integer or float");
      }

      fn_ctx->max_ts = std::numeric_limits<int64_t>::min();
      fn_ctx->initialized = true;
    }

    // On dcheck builds however, we check every passed ts and value.
#if PERFETTO_DCHECK_IS_ON()
    if (sqlite3_value_type(ts) != SQLITE_INTEGER) {
      return sqlite::result::Error(
          ctx, "VALUE_AT_MAX_TS: ts passed was not an integer");
    }
    if (sqlite3_value_type(value) != fn_ctx->value_type) {
      return sqlite::result::Error(
          ctx, "VALUE_AT_MAX_TS: value type is inconsistent");
    }
#endif

    int64_t ts_int = sqlite3_value_int64(ts);
    if (PERFETTO_LIKELY(fn_ctx->max_ts <= ts_int)) {
      fn_ctx->max_ts = ts_int;

      if (fn_ctx->value_type == SQLITE_INTEGER) {
        fn_ctx->int_value_at_max_ts = sqlite3_value_int64(value);
      } else {
        fn_ctx->double_value_at_max_ts = sqlite3_value_double(value);
      }
    }
  }

  static void Final(sqlite3_context* ctx) {
    auto* fn_ctx = static_cast<Context*>(sqlite3_aggregate_context(ctx, 0));
    if (!fn_ctx) {
      sqlite::result::Null(ctx);
      return;
    }
    if (fn_ctx->value_type == SQLITE_INTEGER) {
      sqlite::result::Long(ctx, fn_ctx->int_value_at_max_ts);
    } else {
      sqlite::result::Double(ctx, fn_ctx->double_value_at_max_ts);
    }
  }
};

void RegisterValueAtMaxTsFunction(PerfettoSqlEngine& engine) {
  base::Status status =
      engine.RegisterSqliteAggregateFunction<ValueAtMaxTs>(nullptr);
  if (!status.ok()) {
=======
struct ValueAtMaxTsContext {
  bool initialized;
  int value_type;

  int64_t max_ts;
  int64_t int_value_at_max_ts;
  double double_value_at_max_ts;
};

void ValueAtMaxTsStep(sqlite3_context* ctx, int, sqlite3_value** argv) {
  sqlite3_value* ts = argv[0];
  sqlite3_value* value = argv[1];

  // Note that sqlite3_aggregate_context zeros the memory for us so all the
  // variables of the struct should be zero.
  ValueAtMaxTsContext* fn_ctx = reinterpret_cast<ValueAtMaxTsContext*>(
      sqlite3_aggregate_context(ctx, sizeof(ValueAtMaxTsContext)));

  // For performance reasons, we only do the check for the type of ts and value
  // on the first call of the function.
  if (PERFETTO_UNLIKELY(!fn_ctx->initialized)) {
    if (sqlite3_value_type(ts) != SQLITE_INTEGER) {
      sqlite3_result_error(ctx, "VALUE_AT_MAX_TS: ts passed was not an integer",
                           -1);
      return;
    }

    fn_ctx->value_type = sqlite3_value_type(value);
    if (fn_ctx->value_type != SQLITE_INTEGER &&
        fn_ctx->value_type != SQLITE_FLOAT) {
      sqlite3_result_error(
          ctx, "VALUE_AT_MAX_TS: value passed was not an integer or float", -1);
      return;
    }

    fn_ctx->max_ts = std::numeric_limits<int64_t>::min();
    fn_ctx->initialized = true;
  }

  // On dcheck builds however, we check every passed ts and value.
#if PERFETTO_DCHECK_IS_ON()
  if (sqlite3_value_type(ts) != SQLITE_INTEGER) {
    sqlite3_result_error(ctx, "VALUE_AT_MAX_TS: ts passed was not an integer",
                         -1);
    return;
  }
  if (sqlite3_value_type(value) != fn_ctx->value_type) {
    sqlite3_result_error(ctx, "VALUE_AT_MAX_TS: value type is inconsistent",
                         -1);
    return;
  }
#endif

  int64_t ts_int = sqlite3_value_int64(ts);
  if (PERFETTO_LIKELY(fn_ctx->max_ts <= ts_int)) {
    fn_ctx->max_ts = ts_int;

    if (fn_ctx->value_type == SQLITE_INTEGER) {
      fn_ctx->int_value_at_max_ts = sqlite3_value_int64(value);
    } else {
      fn_ctx->double_value_at_max_ts = sqlite3_value_double(value);
    }
  }
}

void ValueAtMaxTsFinal(sqlite3_context* ctx) {
  ValueAtMaxTsContext* fn_ctx =
      reinterpret_cast<ValueAtMaxTsContext*>(sqlite3_aggregate_context(ctx, 0));
  if (!fn_ctx) {
    sqlite3_result_null(ctx);
    return;
  }
  if (fn_ctx->value_type == SQLITE_INTEGER) {
    sqlite3_result_int64(ctx, fn_ctx->int_value_at_max_ts);
  } else {
    sqlite3_result_double(ctx, fn_ctx->double_value_at_max_ts);
  }
}

void RegisterValueAtMaxTsFunction(sqlite3* db) {
  auto ret = sqlite3_create_function_v2(
      db, "VALUE_AT_MAX_TS", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr,
      nullptr, &ValueAtMaxTsStep, &ValueAtMaxTsFinal, nullptr);
  if (ret) {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    PERFETTO_ELOG("Error initializing VALUE_AT_MAX_TS");
  }
}

std::vector<std::string> SanitizeMetricMountPaths(
    const std::vector<std::string>& mount_paths) {
  std::vector<std::string> sanitized;
  for (const auto& path : mount_paths) {
<<<<<<< HEAD
    if (path.empty())
=======
    if (path.length() == 0)
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      continue;
    sanitized.push_back(path);
    if (path.back() != '/')
      sanitized.back().append("/");
  }
  return sanitized;
}

<<<<<<< HEAD
void InsertIntoTraceMetricsTable(sqlite3* db, const std::string& metric_name) {
  char* insert_sql = sqlite3_mprintf(
      "INSERT INTO _trace_metrics(name) VALUES('%q')", metric_name.c_str());
=======
void SetupMetrics(TraceProcessor* tp,
                  sqlite3* db,
                  std::vector<metrics::SqlMetricFile>* sql_metrics,
                  const std::vector<std::string>& extension_paths) {
  const std::vector<std::string> sanitized_extension_paths =
      SanitizeMetricMountPaths(extension_paths);
  std::vector<std::string> skip_prefixes;
  skip_prefixes.reserve(sanitized_extension_paths.size());
  for (const auto& path : sanitized_extension_paths) {
    skip_prefixes.push_back(kMetricProtoRoot + path);
  }
  tp->ExtendMetricsProto(kMetricsDescriptor.data(), kMetricsDescriptor.size(),
                         skip_prefixes);
  tp->ExtendMetricsProto(kAllChromeMetricsDescriptor.data(),
                         kAllChromeMetricsDescriptor.size(), skip_prefixes);
  tp->ExtendMetricsProto(kAllWebviewMetricsDescriptor.data(),
                         kAllWebviewMetricsDescriptor.size(), skip_prefixes);

  // TODO(lalitm): remove this special casing and change
  // SanitizeMetricMountPaths if/when we move all protos for builtin metrics to
  // match extension protos.
  bool skip_all_sql = std::find(extension_paths.begin(), extension_paths.end(),
                                "") != extension_paths.end();
  if (!skip_all_sql) {
    for (const auto& file_to_sql : sql_metrics::kFileToSql) {
      if (base::StartsWithAny(file_to_sql.path, sanitized_extension_paths))
        continue;
      tp->RegisterMetric(file_to_sql.path, file_to_sql.sql);
    }
  }

  RegisterFunction<metrics::NullIfEmpty>(db, "NULL_IF_EMPTY", 1);
  RegisterFunction<metrics::UnwrapMetricProto>(db, "UNWRAP_METRIC_PROTO", 2);
  RegisterFunction<metrics::RunMetric>(
      db, "RUN_METRIC", -1,
      std::unique_ptr<metrics::RunMetric::Context>(
          new metrics::RunMetric::Context{tp, sql_metrics}));

  // TODO(lalitm): migrate this over to using RegisterFunction once aggregate
  // functions are supported.
  {
    auto ret = sqlite3_create_function_v2(
        db, "RepeatedField", 1, SQLITE_UTF8, nullptr, nullptr,
        metrics::RepeatedFieldStep, metrics::RepeatedFieldFinal, nullptr);
    if (ret)
      PERFETTO_FATAL("Error initializing RepeatedField");
  }
}

void EnsureSqliteInitialized() {
  // sqlite3_initialize isn't actually thread-safe despite being documented
  // as such; we need to make sure multiple TraceProcessorImpl instances don't
  // call it concurrently and only gets called once per process, instead.
  static bool init_once = [] { return sqlite3_initialize() == SQLITE_OK; }();
  PERFETTO_CHECK(init_once);
}

void InsertIntoTraceMetricsTable(sqlite3* db, const std::string& metric_name) {
  char* insert_sql = sqlite3_mprintf(
      "INSERT INTO trace_metrics(name) VALUES('%q')", metric_name.c_str());
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  char* insert_error = nullptr;
  sqlite3_exec(db, insert_sql, nullptr, nullptr, &insert_error);
  sqlite3_free(insert_sql);
  if (insert_error) {
    PERFETTO_ELOG("Error registering table: %s", insert_error);
    sqlite3_free(insert_error);
  }
}

<<<<<<< HEAD
sql_modules::NameToPackage GetStdlibPackages() {
  sql_modules::NameToPackage packages;
  for (const auto& file_to_sql : stdlib::kFileToSql) {
    std::string module_name = sql_modules::GetIncludeKey(file_to_sql.path);
    std::string package_name = sql_modules::GetPackageName(module_name);
    packages.Insert(package_name, {})
        .first->push_back({module_name, file_to_sql.sql});
  }
  return packages;
}

std::pair<int64_t, int64_t> GetTraceTimestampBoundsNs(
    const TraceStorage& storage) {
  int64_t start_ns = std::numeric_limits<int64_t>::max();
  int64_t end_ns = std::numeric_limits<int64_t>::min();
  for (auto it = storage.ftrace_event_table().IterateRows(); it; ++it) {
    start_ns = std::min(it.ts(), start_ns);
    end_ns = std::max(it.ts(), end_ns);
  }
  for (auto it = storage.sched_slice_table().IterateRows(); it; ++it) {
    start_ns = std::min(it.ts(), start_ns);
    end_ns = std::max(it.ts() + it.dur(), end_ns);
  }
  for (auto it = storage.counter_table().IterateRows(); it; ++it) {
    start_ns = std::min(it.ts(), start_ns);
    end_ns = std::max(it.ts(), end_ns);
  }
  for (auto it = storage.slice_table().IterateRows(); it; ++it) {
    start_ns = std::min(it.ts(), start_ns);
    end_ns = std::max(it.ts() + it.dur(), end_ns);
  }
  for (auto it = storage.heap_profile_allocation_table().IterateRows(); it;
       ++it) {
    start_ns = std::min(it.ts(), start_ns);
    end_ns = std::max(it.ts(), end_ns);
  }
  for (auto it = storage.thread_state_table().IterateRows(); it; ++it) {
    start_ns = std::min(it.ts(), start_ns);
    end_ns = std::max(it.ts() + it.dur(), end_ns);
  }
  for (auto it = storage.android_log_table().IterateRows(); it; ++it) {
    start_ns = std::min(it.ts(), start_ns);
    end_ns = std::max(it.ts(), end_ns);
  }
  for (auto it = storage.heap_graph_object_table().IterateRows(); it; ++it) {
    start_ns = std::min(it.graph_sample_ts(), start_ns);
    end_ns = std::max(it.graph_sample_ts(), end_ns);
  }
  for (auto it = storage.perf_sample_table().IterateRows(); it; ++it) {
    start_ns = std::min(it.ts(), start_ns);
    end_ns = std::max(it.ts(), end_ns);
  }
  for (auto it = storage.instruments_sample_table().IterateRows(); it; ++it) {
    start_ns = std::min(it.ts(), start_ns);
    end_ns = std::max(it.ts(), end_ns);
  }
  for (auto it = storage.cpu_profile_stack_sample_table().IterateRows(); it;
       ++it) {
    start_ns = std::min(it.ts(), start_ns);
    end_ns = std::max(it.ts(), end_ns);
  }
  if (start_ns == std::numeric_limits<int64_t>::max()) {
    return std::make_pair(0, 0);
  }
  if (start_ns == end_ns) {
    end_ns += 1;
  }
  return std::make_pair(start_ns, end_ns);
}

}  // namespace

TraceProcessorImpl::TraceProcessorImpl(const Config& cfg)
    : TraceProcessorStorageImpl(cfg), config_(cfg) {
  context_.reader_registry->RegisterTraceReader<AndroidDumpstateReader>(
      kAndroidDumpstateTraceType);
  context_.android_dumpstate_event_parser =
      std::make_unique<AndroidDumpstateEventParserImpl>(&context_);

  context_.reader_registry->RegisterTraceReader<AndroidLogReader>(
      kAndroidLogcatTraceType);
  context_.android_log_event_parser =
      std::make_unique<AndroidLogEventParserImpl>(&context_);

  context_.reader_registry->RegisterTraceReader<FuchsiaTraceTokenizer>(
      kFuchsiaTraceType);
  context_.fuchsia_record_parser =
      std::make_unique<FuchsiaTraceParser>(&context_);

  context_.reader_registry->RegisterTraceReader<SystraceTraceParser>(
      kSystraceTraceType);
  context_.reader_registry->RegisterTraceReader<NinjaLogParser>(
      kNinjaLogTraceType);

  context_.reader_registry
      ->RegisterTraceReader<perf_importer::PerfDataTokenizer>(
          kPerfDataTraceType);
  context_.perf_record_parser =
      std::make_unique<perf_importer::RecordParser>(&context_);
  context_.spe_record_parser =
      std::make_unique<perf_importer::SpeRecordParserImpl>(&context_);

#if PERFETTO_BUILDFLAG(PERFETTO_TP_INSTRUMENTS)
  context_.reader_registry
      ->RegisterTraceReader<instruments_importer::InstrumentsXmlTokenizer>(
          kInstrumentsXmlTraceType);
  context_.instruments_row_parser =
      std::make_unique<instruments_importer::RowParser>(&context_);
#endif

  if constexpr (util::IsGzipSupported()) {
    context_.reader_registry->RegisterTraceReader<GzipTraceParser>(
        kGzipTraceType);
    context_.reader_registry->RegisterTraceReader<GzipTraceParser>(
        kCtraceTraceType);
    context_.reader_registry->RegisterTraceReader<ZipTraceReader>(kZipFile);
  }

  if constexpr (json::IsJsonSupported()) {
    context_.reader_registry->RegisterTraceReader<JsonTraceTokenizer>(
        kJsonTraceType);
    context_.json_trace_parser =
        std::make_unique<JsonTraceParserImpl>(&context_);

    context_.reader_registry
        ->RegisterTraceReader<gecko_importer::GeckoTraceTokenizer>(
            kGeckoTraceType);
    context_.gecko_trace_parser =
        std::make_unique<gecko_importer::GeckoTraceParserImpl>(&context_);
  }

  context_.reader_registry->RegisterTraceReader<art_method::ArtMethodTokenizer>(
      kArtMethodTraceType);
  context_.art_method_parser =
      std::make_unique<art_method::ArtMethodParserImpl>(&context_);

  context_.reader_registry->RegisterTraceReader<art_hprof::ArtHprofParser>(
      kArtHprofTraceType);

  context_.reader_registry
      ->RegisterTraceReader<perf_text_importer::PerfTextTraceTokenizer>(
          kPerfTextTraceType);
  context_.perf_text_parser =
      std::make_unique<perf_text_importer::PerfTextTraceParserImpl>(&context_);

  context_.reader_registry->RegisterTraceReader<TarTraceReader>(kTarTraceType);

#if PERFETTO_BUILDFLAG(PERFETTO_ENABLE_ETM_IMPORTER)
  perf_importer::PerfTracker::GetOrCreate(&context_)->RegisterAuxTokenizer(
      PERF_AUXTRACE_CS_ETM, etm::CreateEtmV4StreamDemultiplexer);
#endif

  const std::vector<std::string> sanitized_extension_paths =
      SanitizeMetricMountPaths(config_.skip_builtin_metric_paths);
  std::vector<std::string> skip_prefixes;
  skip_prefixes.reserve(sanitized_extension_paths.size());
  for (const auto& path : sanitized_extension_paths) {
    skip_prefixes.push_back(kMetricProtoRoot + path);
  }

  // Add metrics to descriptor pool
  metrics_descriptor_pool_.AddFromFileDescriptorSet(
      kMetricsDescriptor.data(), kMetricsDescriptor.size(), skip_prefixes);
  metrics_descriptor_pool_.AddFromFileDescriptorSet(
      kAllChromeMetricsDescriptor.data(), kAllChromeMetricsDescriptor.size(),
      skip_prefixes);
  metrics_descriptor_pool_.AddFromFileDescriptorSet(
      kAllWebviewMetricsDescriptor.data(), kAllWebviewMetricsDescriptor.size(),
      skip_prefixes);

  // Add the summary descriptor to the summary pool.
  {
    base::Status status = context_.descriptor_pool_->AddFromFileDescriptorSet(
        kTraceSummaryDescriptor.data(), kTraceSummaryDescriptor.size());
    PERFETTO_CHECK(status.ok());
  }

  RegisterAdditionalModules(&context_);
  InitPerfettoSqlEngine();

  sqlite_objects_post_prelude_ = engine_->SqliteRegisteredObjectCount();

  bool skip_all_sql = std::find(config_.skip_builtin_metric_paths.begin(),
                                config_.skip_builtin_metric_paths.end(),
                                "") != config_.skip_builtin_metric_paths.end();
  if (!skip_all_sql) {
    for (const auto& file_to_sql : sql_metrics::kFileToSql) {
      if (base::StartsWithAny(file_to_sql.path, sanitized_extension_paths))
        continue;
      RegisterMetric(file_to_sql.path, file_to_sql.sql);
=======
void IncrementCountForStmt(sqlite3_stmt* stmt,
                           IteratorImpl::StmtMetadata* metadata) {
  metadata->statement_count++;

  // If the stmt is already done, it clearly didn't have any output.
  if (sqlite_utils::IsStmtDone(stmt))
    return;

  if (sqlite3_column_count(stmt) == 1) {
    sqlite3_value* value = sqlite3_column_value(stmt, 0);

    // If the "VOID" pointer associated to the return value is not null,
    // that means this is a function which is forced to return a value
    // (because all functions in SQLite have to) but doesn't actually
    // wait to (i.e. it wants to be treated like CREATE TABLE or similar).
    // Because of this, ignore the return value of this function.
    // See |WrapSqlFunction| for where this is set.
    if (sqlite3_value_pointer(value, "VOID") != nullptr) {
      return;
    }

    // If the statement only has a single column and that column is named
    // "suppress_query_output", treat it as a statement without output for
    // accounting purposes. This allows an escape hatch for cases where the
    // user explicitly wants to ignore functions as having output.
    if (strcmp(sqlite3_column_name(stmt, 0), "suppress_query_output") == 0) {
      return;
    }
  }

  // Otherwise, the statement has output and so increment the count.
  metadata->statement_count_with_output++;
}

base::Status PrepareAndStepUntilLastValidStmt(
    sqlite3* db,
    const std::string& sql,
    ScopedStmt* output_stmt,
    IteratorImpl::StmtMetadata* metadata) {
  ScopedStmt prev_stmt;
  // A sql string can contain several statements. Some of them might be comment
  // only, e.g. "SELECT 1; /* comment */; SELECT 2;". Here we process one
  // statement on each iteration. SQLite's sqlite_prepare_v2 (wrapped by
  // PrepareStmt) returns on each iteration a pointer to the unprocessed string.
  //
  // Unfortunately we cannot call PrepareStmt and tokenize all statements
  // upfront because sqlite_prepare_v2 also semantically checks the statement
  // against the schema. In some cases statements might depend on the execution
  // of previous ones (e.e. CREATE VIEW x; SELECT FROM x; DELETE VIEW x;).
  //
  // Also, unfortunately, we need to PrepareStmt to find out if a statement is a
  // comment or a real statement.
  //
  // The logic here is the following:
  //  - We invoke PrepareStmt on each statement.
  //  - If the statement is a comment we simply skip it.
  //  - If the statement is valid, we step once to make sure side effects take
  //    effect.
  //  - If we encounter a valid statement afterwards, we step internally through
  //    all rows of the previous one. This ensures that any further side effects
  //    take hold *before* we step into the next statement.
  //  - Once no further non-comment statements are encountered, we return an
  //    iterator to the last valid statement.
  for (const char* rem_sql = sql.c_str(); rem_sql && rem_sql[0];) {
    ScopedStmt cur_stmt;
    {
      PERFETTO_TP_TRACE(metatrace::Category::QUERY, "QUERY_PREPARE");
      const char* tail = nullptr;
      RETURN_IF_ERROR(sqlite_utils::PrepareStmt(db, rem_sql, &cur_stmt, &tail));
      rem_sql = tail;
    }

    // The only situation where we'd have an ok status but also no prepared
    // statement is if the statement of SQL we parsed was a pure comment. In
    // this case, just continue to the next statement.
    if (!cur_stmt)
      continue;

    // Before stepping into |cur_stmt|, we need to finish iterating through
    // the previous statement so we don't have two clashing statements (e.g.
    // SELECT * FROM v and DROP VIEW v) partially stepped into.
    if (prev_stmt) {
      PERFETTO_TP_TRACE(metatrace::Category::QUERY, "STMT_STEP_UNTIL_DONE",
                        [&prev_stmt](metatrace::Record* record) {
                          auto expanded_sql =
                              sqlite_utils::ExpandedSqlForStmt(*prev_stmt);
                          record->AddArg("SQL", expanded_sql.get());
                        });
      RETURN_IF_ERROR(sqlite_utils::StepStmtUntilDone(prev_stmt.get()));
    }

    PERFETTO_DLOG("Executing statement: %s", sqlite3_sql(*cur_stmt));

    {
      PERFETTO_TP_TRACE(metatrace::Category::TOPLEVEL, "STMT_FIRST_STEP",
                        [&cur_stmt](metatrace::Record* record) {
                          auto expanded_sql =
                              sqlite_utils::ExpandedSqlForStmt(*cur_stmt);
                          record->AddArg("SQL", expanded_sql.get());
                        });

      // Now step once into |cur_stmt| so that when we prepare the next statment
      // we will have executed any dependent bytecode in this one.
      int err = sqlite3_step(*cur_stmt);
      if (err != SQLITE_ROW && err != SQLITE_DONE) {
        return base::ErrStatus(
            "%s", sqlite_utils::FormatErrorMessage(
                      prev_stmt.get(), base::StringView(sql), db, err)
                      .c_message());
      }
    }

    // Increment the neecessary counts for the statement.
    IncrementCountForStmt(cur_stmt.get(), metadata);

    // Propogate the current statement to the next iteration.
    prev_stmt = std::move(cur_stmt);
  }

  // If we didn't manage to prepare a single statment, that means everything
  // in the SQL was treated as a comment.
  if (!prev_stmt)
    return base::ErrStatus("No valid SQL to run");

  // Update the output statment and column count.
  *output_stmt = std::move(prev_stmt);
  metadata->column_count =
      static_cast<uint32_t>(sqlite3_column_count(output_stmt->get()));
  return base::OkStatus();
}

const char* TraceTypeToString(TraceType trace_type) {
  switch (trace_type) {
    case kUnknownTraceType:
      return "unknown";
    case kProtoTraceType:
      return "proto";
    case kJsonTraceType:
      return "json";
    case kFuchsiaTraceType:
      return "fuchsia";
    case kSystraceTraceType:
      return "systrace";
    case kGzipTraceType:
      return "gzip";
    case kCtraceTraceType:
      return "ctrace";
    case kNinjaLogTraceType:
      return "ninja_log";
    case kAndroidBugreportTraceType:
      return "android_bugreport";
  }
  PERFETTO_FATAL("For GCC");
}

// Register SQL functions only used in local development instances.
void RegisterDevFunctions(sqlite3* db) {
  RegisterFunction<WriteFile>(db, "WRITE_FILE", 2);
}

sql_modules::NameToModule GetStdlibModules() {
  sql_modules::NameToModule modules;
  for (const auto& file_to_sql : stdlib::kFileToSql) {
    std::string import_key = sql_modules::GetImportKey(file_to_sql.path);
    std::string module = sql_modules::GetModuleName(import_key);
    modules.Insert(module, {}).first->push_back({import_key, file_to_sql.sql});
  }
  return modules;
}

void InitializePreludeTablesViews(sqlite3* db) {
  for (const auto& file_to_sql : prelude::tables_views::kFileToSql) {
    char* errmsg_raw = nullptr;
    int err = sqlite3_exec(db, file_to_sql.sql, nullptr, nullptr, &errmsg_raw);
    ScopedSqliteString errmsg(errmsg_raw);
    if (err != SQLITE_OK) {
      PERFETTO_FATAL("Failed to initialize prelude %s", errmsg_raw);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    }
  }
}

<<<<<<< HEAD
TraceProcessorImpl::~TraceProcessorImpl() = default;

// =================================================================
// |        TraceProcessorStorage implementation starts here       |
// =================================================================
=======
}  // namespace

template <typename View>
void TraceProcessorImpl::RegisterView(const View& view) {
  RegisterTableFunction(std::unique_ptr<TableFunction>(
      new ViewTableFunction(&view, View::Name())));
}

TraceProcessorImpl::TraceProcessorImpl(const Config& cfg)
    : TraceProcessorStorageImpl(cfg) {
  context_.fuchsia_trace_tokenizer.reset(new FuchsiaTraceTokenizer(&context_));
  context_.fuchsia_trace_parser.reset(new FuchsiaTraceParser(&context_));

  context_.ninja_log_parser.reset(new NinjaLogParser(&context_));

  context_.systrace_trace_parser.reset(new SystraceTraceParser(&context_));

  if (util::IsGzipSupported()) {
    context_.gzip_trace_parser.reset(new GzipTraceParser(&context_));
    context_.android_bugreport_parser.reset(
        new AndroidBugreportParser(&context_));
  }

  if (json::IsJsonSupported()) {
    context_.json_trace_tokenizer.reset(new JsonTraceTokenizer(&context_));
    context_.json_trace_parser.reset(new JsonTraceParser(&context_));
  }

  if (context_.config.analyze_trace_proto_content) {
    context_.content_analyzer.reset(new ProtoContentAnalyzer(&context_));
  }

  RegisterAdditionalModules(&context_);

  sqlite3* db = nullptr;
  EnsureSqliteInitialized();
  PERFETTO_CHECK(sqlite3_open(":memory:", &db) == SQLITE_OK);
  InitializeSqlite(db);
  InitializePreludeTablesViews(db);
  db_.reset(std::move(db));

  // New style function registration.
  if (cfg.enable_dev_features) {
    RegisterDevFunctions(db);
  }
  RegisterFunction<Glob>(db, "glob", 2);
  RegisterFunction<Hash>(db, "HASH", -1);
  RegisterFunction<Base64Encode>(db, "BASE64_ENCODE", 1);
  RegisterFunction<Demangle>(db, "DEMANGLE", 1);
  RegisterFunction<SourceGeq>(db, "SOURCE_GEQ", -1);
  RegisterFunction<ExportJson>(db, "EXPORT_JSON", 1, context_.storage.get(),
                               false);
  RegisterFunction<ExtractArg>(db, "EXTRACT_ARG", 2, context_.storage.get());
  RegisterFunction<AbsTimeStr>(db, "ABS_TIME_STR", 1,
                               context_.clock_converter.get());
  RegisterFunction<ToMonotonic>(db, "TO_MONOTONIC", 1,
                                context_.clock_converter.get());
  RegisterFunction<CreateFunction>(
      db, "CREATE_FUNCTION", 3,
      std::unique_ptr<CreateFunction::Context>(
          new CreateFunction::Context{db_.get(), &create_function_state_}));
  RegisterFunction<CreateViewFunction>(
      db, "CREATE_VIEW_FUNCTION", 3,
      std::unique_ptr<CreateViewFunction::Context>(
          new CreateViewFunction::Context{db_.get()}));
  RegisterFunction<Import>(db, "IMPORT", 1,
                           std::unique_ptr<Import::Context>(new Import::Context{
                               db_.get(), this, &sql_modules_}));
  RegisterFunction<ToFtrace>(
      db, "TO_FTRACE", 1,
      std::unique_ptr<ToFtrace::Context>(new ToFtrace::Context{
          context_.storage.get(), SystraceSerializer(&context_)}));

  // Old style function registration.
  // TODO(lalitm): migrate this over to using RegisterFunction once aggregate
  // functions are supported.
  RegisterLastNonNullFunction(db);
  RegisterValueAtMaxTsFunction(db);
  {
    base::Status status = RegisterStackFunctions(db, &context_);
    if (!status.ok())
      PERFETTO_ELOG("%s", status.c_message());
  }
  {
    base::Status status = PprofFunctions::Register(db, &context_);
    if (!status.ok())
      PERFETTO_ELOG("%s", status.c_message());
  }
  {
    base::Status status = LayoutFunctions::Register(db, &context_);
    if (!status.ok())
      PERFETTO_ELOG("%s", status.c_message());
  }

  auto stdlib_modules = GetStdlibModules();
  for (auto module_it = stdlib_modules.GetIterator(); module_it; ++module_it) {
    base::Status status =
        RegisterSqlModule({module_it.key(), module_it.value(), false});
    if (!status.ok())
      PERFETTO_ELOG("%s", status.c_message());
  }

  SetupMetrics(this, *db_, &sql_metrics_, cfg.skip_builtin_metric_paths);

  // Setup the query cache.
  query_cache_.reset(new QueryCache());

  const TraceStorage* storage = context_.storage.get();

  SqlStatsTable::RegisterTable(*db_, storage);
  StatsTable::RegisterTable(*db_, storage);

  // Operator tables.
  SpanJoinOperatorTable::RegisterTable(*db_, storage);
  WindowOperatorTable::RegisterTable(*db_, storage);
  CreateViewFunction::RegisterTable(*db_);

  // Tables dynamically generated at query time.
  RegisterTableFunction(std::unique_ptr<ExperimentalFlamegraph>(
      new ExperimentalFlamegraph(&context_)));
  RegisterTableFunction(std::unique_ptr<ExperimentalCounterDur>(
      new ExperimentalCounterDur(storage->counter_table())));
  RegisterTableFunction(std::unique_ptr<ExperimentalSliceLayout>(
      new ExperimentalSliceLayout(context_.storage.get()->mutable_string_pool(),
                                  &storage->slice_table())));
  RegisterTableFunction(std::unique_ptr<Ancestor>(
      new Ancestor(Ancestor::Type::kSlice, context_.storage.get())));
  RegisterTableFunction(std::unique_ptr<Ancestor>(new Ancestor(
      Ancestor::Type::kStackProfileCallsite, context_.storage.get())));
  RegisterTableFunction(std::unique_ptr<Ancestor>(
      new Ancestor(Ancestor::Type::kSliceByStack, context_.storage.get())));
  RegisterTableFunction(std::unique_ptr<Descendant>(
      new Descendant(Descendant::Type::kSlice, context_.storage.get())));
  RegisterTableFunction(std::unique_ptr<Descendant>(
      new Descendant(Descendant::Type::kSliceByStack, context_.storage.get())));
  RegisterTableFunction(std::unique_ptr<ConnectedFlow>(new ConnectedFlow(
      ConnectedFlow::Mode::kDirectlyConnectedFlow, context_.storage.get())));
  RegisterTableFunction(std::unique_ptr<ConnectedFlow>(new ConnectedFlow(
      ConnectedFlow::Mode::kPrecedingFlow, context_.storage.get())));
  RegisterTableFunction(std::unique_ptr<ConnectedFlow>(new ConnectedFlow(
      ConnectedFlow::Mode::kFollowingFlow, context_.storage.get())));
  RegisterTableFunction(
      std::unique_ptr<ExperimentalSchedUpid>(new ExperimentalSchedUpid(
          storage->sched_slice_table(), storage->thread_table())));
  RegisterTableFunction(std::unique_ptr<ExperimentalAnnotatedStack>(
      new ExperimentalAnnotatedStack(&context_)));
  RegisterTableFunction(std::unique_ptr<ExperimentalFlatSlice>(
      new ExperimentalFlatSlice(&context_)));

  // Views.
  RegisterView(storage->thread_slice_view());

  // New style db-backed tables.
  // Note: if adding a table here which might potentially contain many rows
  // (O(rows in sched/slice/counter)), then consider calling ShrinkToFit on
  // that table in TraceStorage::ShrinkToFitTables.
  RegisterDbTable(storage->arg_table());
  RegisterDbTable(storage->raw_table());
  RegisterDbTable(storage->ftrace_event_table());
  RegisterDbTable(storage->thread_table());
  RegisterDbTable(storage->process_table());
  RegisterDbTable(storage->filedescriptor_table());

  RegisterDbTable(storage->slice_table());
  RegisterDbTable(storage->flow_table());
  RegisterDbTable(storage->slice_table());
  RegisterDbTable(storage->sched_slice_table());
  RegisterDbTable(storage->thread_state_table());
  RegisterDbTable(storage->gpu_slice_table());

  RegisterDbTable(storage->track_table());
  RegisterDbTable(storage->thread_track_table());
  RegisterDbTable(storage->process_track_table());
  RegisterDbTable(storage->cpu_track_table());
  RegisterDbTable(storage->gpu_track_table());

  RegisterDbTable(storage->counter_table());

  RegisterDbTable(storage->counter_track_table());
  RegisterDbTable(storage->process_counter_track_table());
  RegisterDbTable(storage->thread_counter_track_table());
  RegisterDbTable(storage->cpu_counter_track_table());
  RegisterDbTable(storage->irq_counter_track_table());
  RegisterDbTable(storage->softirq_counter_track_table());
  RegisterDbTable(storage->gpu_counter_track_table());
  RegisterDbTable(storage->gpu_counter_group_table());
  RegisterDbTable(storage->perf_counter_track_table());
  RegisterDbTable(storage->energy_counter_track_table());
  RegisterDbTable(storage->uid_counter_track_table());
  RegisterDbTable(storage->energy_per_uid_counter_track_table());

  RegisterDbTable(storage->heap_graph_object_table());
  RegisterDbTable(storage->heap_graph_reference_table());
  RegisterDbTable(storage->heap_graph_class_table());

  RegisterDbTable(storage->symbol_table());
  RegisterDbTable(storage->heap_profile_allocation_table());
  RegisterDbTable(storage->cpu_profile_stack_sample_table());
  RegisterDbTable(storage->perf_sample_table());
  RegisterDbTable(storage->stack_profile_callsite_table());
  RegisterDbTable(storage->stack_profile_mapping_table());
  RegisterDbTable(storage->stack_profile_frame_table());
  RegisterDbTable(storage->package_list_table());
  RegisterDbTable(storage->profiler_smaps_table());

  RegisterDbTable(storage->android_log_table());
  RegisterDbTable(storage->android_dumpstate_table());
  RegisterDbTable(storage->android_game_intervention_list_table());

  RegisterDbTable(storage->vulkan_memory_allocations_table());

  RegisterDbTable(storage->graphics_frame_slice_table());

  RegisterDbTable(storage->expected_frame_timeline_slice_table());
  RegisterDbTable(storage->actual_frame_timeline_slice_table());

  RegisterDbTable(storage->metadata_table());
  RegisterDbTable(storage->cpu_table());
  RegisterDbTable(storage->cpu_freq_table());
  RegisterDbTable(storage->clock_snapshot_table());

  RegisterDbTable(storage->memory_snapshot_table());
  RegisterDbTable(storage->process_memory_snapshot_table());
  RegisterDbTable(storage->memory_snapshot_node_table());
  RegisterDbTable(storage->memory_snapshot_edge_table());

  RegisterDbTable(storage->experimental_proto_path_table());
  RegisterDbTable(storage->experimental_proto_content_table());

  RegisterDbTable(storage->experimental_missing_chrome_processes_table());
}

TraceProcessorImpl::~TraceProcessorImpl() = default;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

base::Status TraceProcessorImpl::Parse(TraceBlobView blob) {
  bytes_parsed_ += blob.size();
  return TraceProcessorStorageImpl::Parse(std::move(blob));
}

<<<<<<< HEAD
void TraceProcessorImpl::Flush() {
  TraceProcessorStorageImpl::Flush();
  BuildBoundsTable(engine_->sqlite_engine()->db(),
                   GetTraceTimestampBoundsNs(*context_.storage));
}

base::Status TraceProcessorImpl::NotifyEndOfFile() {
  if (notify_eof_called_) {
    const char kMessage[] =
        "NotifyEndOfFile should only be called once. Try calling Flush instead "
        "if trying to commit the contents of the trace to tables.";
    PERFETTO_ELOG(kMessage);
    return base::ErrStatus(kMessage);
=======
std::string TraceProcessorImpl::GetCurrentTraceName() {
  if (current_trace_name_.empty())
    return "";
  auto size = " (" + std::to_string(bytes_parsed_ / 1024 / 1024) + " MB)";
  return current_trace_name_ + size;
}

void TraceProcessorImpl::SetCurrentTraceName(const std::string& name) {
  current_trace_name_ = name;
}

void TraceProcessorImpl::Flush() {
  TraceProcessorStorageImpl::Flush();

  context_.metadata_tracker->SetMetadata(
      metadata::trace_size_bytes,
      Variadic::Integer(static_cast<int64_t>(bytes_parsed_)));
  const StringId trace_type_id =
      context_.storage->InternString(TraceTypeToString(context_.trace_type));
  context_.metadata_tracker->SetMetadata(metadata::trace_type,
                                         Variadic::String(trace_type_id));
  BuildBoundsTable(*db_, context_.storage->GetTraceTimestampBoundsNs());
}

void TraceProcessorImpl::NotifyEndOfFile() {
  if (notify_eof_called_) {
    PERFETTO_ELOG(
        "NotifyEndOfFile should only be called once. Try calling Flush instead "
        "if trying to commit the contents of the trace to tables.");
    return;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }
  notify_eof_called_ = true;

  if (current_trace_name_.empty())
    current_trace_name_ = "Unnamed trace";

  // Last opportunity to flush all pending data.
  Flush();

<<<<<<< HEAD
#if PERFETTO_BUILDFLAG(PERFETTO_ENABLE_ETM_IMPORTER)
  if (context_.etm_tracker) {
    RETURN_IF_ERROR(etm::EtmTracker::GetOrCreate(&context_)->Finalize());
  }
#endif

  RETURN_IF_ERROR(TraceProcessorStorageImpl::NotifyEndOfFile());
  if (context_.perf_tracker) {
    perf_importer::PerfTracker::GetOrCreate(&context_)->NotifyEndOfFile();
  }
=======
  TraceProcessorStorageImpl::NotifyEndOfFile();

  // Create a snapshot list of all tables and views created so far. This is so
  // later we can drop all extra tables created by the UI and reset to the
  // original state (see RestoreInitialTables).
  initial_tables_.clear();
  auto it = ExecuteQuery(kAllTablesQuery);
  while (it.Next()) {
    auto value = it.Get(0);
    PERFETTO_CHECK(value.type == SqlValue::Type::kString);
    initial_tables_.push_back(value.string_value);
  }

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  context_.storage->ShrinkToFitTables();

  // Rebuild the bounds table once everything has been completed: we do this
  // so that if any data was added to tables in
  // TraceProcessorStorageImpl::NotifyEndOfFile, this will be counted in
  // trace bounds: this is important for parsers like ninja which wait until
  // the end to flush all their data.
<<<<<<< HEAD
  BuildBoundsTable(engine_->sqlite_engine()->db(),
                   GetTraceTimestampBoundsNs(*context_.storage));

  TraceProcessorStorageImpl::DestroyContext();

  IncludeAfterEofPrelude();
  sqlite_objects_post_prelude_ = engine_->SqliteRegisteredObjectCount();
  return base::OkStatus();
}

// =================================================================
// |        PerfettoSQL related functionality starts here          |
// =================================================================

Iterator TraceProcessorImpl::ExecuteQuery(const std::string& sql) {
  PERFETTO_TP_TRACE(metatrace::Category::API_TIMELINE, "EXECUTE_QUERY",
                    [&](metatrace::Record* r) { r->AddArg("query", sql); });
=======
  BuildBoundsTable(*db_, context_.storage->GetTraceTimestampBoundsNs());

  TraceProcessorStorageImpl::DestroyContext();
}

size_t TraceProcessorImpl::RestoreInitialTables() {
  // Step 1: figure out what tables/views/indices we need to delete.
  std::vector<std::pair<std::string, std::string>> deletion_list;
  std::string msg = "Resetting DB to initial state, deleting table/views:";
  for (auto it = ExecuteQuery(kAllTablesQuery); it.Next();) {
    std::string name(it.Get(0).string_value);
    std::string type(it.Get(1).string_value);
    if (std::find(initial_tables_.begin(), initial_tables_.end(), name) ==
        initial_tables_.end()) {
      msg += " " + name;
      deletion_list.push_back(std::make_pair(type, name));
    }
  }

  PERFETTO_LOG("%s", msg.c_str());

  // Step 2: actually delete those tables/views/indices.
  for (const auto& tn : deletion_list) {
    std::string query = "DROP " + tn.first + " " + tn.second;
    auto it = ExecuteQuery(query);
    while (it.Next()) {
    }
    // Index deletion can legitimately fail. If one creates an index "i" on a
    // table "t" but issues the deletion in the order (t, i), the DROP index i
    // will fail with "no such index" because deleting the table "t"
    // automatically deletes all associated indexes.
    if (!it.Status().ok() && tn.first != "index")
      PERFETTO_FATAL("%s -> %s", query.c_str(), it.Status().c_message());
  }
  return deletion_list.size();
}

Iterator TraceProcessorImpl::ExecuteQuery(const std::string& sql) {
  PERFETTO_TP_TRACE(metatrace::Category::TOPLEVEL, "QUERY_EXECUTE");
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  uint32_t sql_stats_row =
      context_.storage->mutable_sql_stats()->RecordQueryBegin(
          sql, base::GetWallTimeNs().count());
<<<<<<< HEAD
  std::string non_breaking_sql = base::ReplaceAll(sql, "\u00A0", " ");
  base::StatusOr<PerfettoSqlEngine::ExecutionResult> result =
      engine_->ExecuteUntilLastStatement(
          SqlSource::FromExecuteQuery(std::move(non_breaking_sql)));
  std::unique_ptr<IteratorImpl> impl(
      new IteratorImpl(this, std::move(result), sql_stats_row));
  return Iterator(std::move(impl));
}

base::Status TraceProcessorImpl::RegisterSqlPackage(SqlPackage sql_package) {
  sql_modules::RegisteredPackage new_package;
  std::string name = sql_package.name;
  if (engine_->FindPackage(name) && !sql_package.allow_override) {
    return base::ErrStatus(
        "Package '%s' is already registered. Choose a different name.\n"
        "If you want to replace the existing package using trace processor "
        "shell, you need to pass the --dev flag and use "
        "--override-sql-package "
        "to pass the module path.",
        name.c_str());
  }
  for (auto const& module_name_and_sql : sql_package.modules) {
    if (sql_modules::GetPackageName(module_name_and_sql.first) != name) {
      return base::ErrStatus(
          "Module name doesn't match the package name. First part of module "
          "name should be package name. Import key: '%s', package name: '%s'.",
          module_name_and_sql.first.c_str(), name.c_str());
    }
    new_package.modules.Insert(module_name_and_sql.first,
                               {module_name_and_sql.second, false});
  }
  manually_registered_sql_packages_.push_back(SqlPackage(sql_package));
  engine_->RegisterPackage(name, std::move(new_package));
  return base::OkStatus();
}

base::Status TraceProcessorImpl::RegisterSqlModule(SqlModule module) {
  SqlPackage package;
  package.name = std::move(module.name);
  package.modules = std::move(module.files);
  package.allow_override = module.allow_module_override;
  return RegisterSqlPackage(package);
}

// =================================================================
// |  Trace-based metrics (v2) related functionality starts here   |
// =================================================================

base::Status TraceProcessorImpl::Summarize(
    const TraceSummaryComputationSpec& computation,
    const std::vector<TraceSummarySpecBytes>& specs,
    std::vector<uint8_t>* output,
    const TraceSummaryOutputSpec& output_spec) {
  return summary::Summarize(this, *context_.descriptor_pool_, computation,
                            specs, output, output_spec);
}

// =================================================================
// |        Metatracing related functionality starts here          |
// =================================================================
=======

  ScopedStmt stmt;
  IteratorImpl::StmtMetadata metadata;
  base::Status status =
      PrepareAndStepUntilLastValidStmt(*db_, sql, &stmt, &metadata);
  PERFETTO_DCHECK((status.ok() && stmt) || (!status.ok() && !stmt));

  std::unique_ptr<IteratorImpl> impl(new IteratorImpl(
      this, *db_, status, std::move(stmt), std::move(metadata), sql_stats_row));
  return Iterator(std::move(impl));
}

void TraceProcessorImpl::InterruptQuery() {
  if (!db_)
    return;
  query_interrupted_.store(true);
  sqlite3_interrupt(db_.get());
}

bool TraceProcessorImpl::IsRootMetricField(const std::string& metric_name) {
  std::optional<uint32_t> desc_idx =
      pool_.FindDescriptorIdx(".perfetto.protos.TraceMetrics");
  if (!desc_idx.has_value())
    return false;
  auto field_idx = pool_.descriptors()[*desc_idx].FindFieldByName(metric_name);
  return field_idx != nullptr;
}

base::Status TraceProcessorImpl::RegisterSqlModule(SqlModule sql_module) {
  sql_modules::RegisteredModule new_module;
  std::string name = sql_module.name;
  if (sql_modules_.Find(name) && !sql_module.allow_module_override) {
    return base::ErrStatus(
        "Module '%s' is already registered. Choose a different name.\n"
        "If you want to replace the existing module using trace processor "
        "shell, you need to pass the --dev flag and use --override-sql-module "
        "to pass the module path.",
        name.c_str());
  }
  for (auto const& name_and_sql : sql_module.files) {
    if (sql_modules::GetModuleName(name_and_sql.first) != name) {
      return base::ErrStatus(
          "File import key doesn't match the module name. First part of import "
          "key should be module name. Import key: %s, module name: %s.",
          name_and_sql.first.c_str(), name.c_str());
    }
    new_module.import_key_to_file.Insert(name_and_sql.first,
                                         {name_and_sql.second, false});
  }
  sql_modules_.Insert(name, std::move(new_module));
  return base::OkStatus();
}

base::Status TraceProcessorImpl::RegisterMetric(const std::string& path,
                                                const std::string& sql) {
  std::string stripped_sql;
  for (base::StringSplitter sp(sql, '\n'); sp.Next();) {
    if (strncmp(sp.cur_token(), "--", 2) != 0) {
      stripped_sql.append(sp.cur_token());
      stripped_sql.push_back('\n');
    }
  }

  // Check if the metric with the given path already exists and if it does,
  // just update the SQL associated with it.
  auto it = std::find_if(
      sql_metrics_.begin(), sql_metrics_.end(),
      [&path](const metrics::SqlMetricFile& m) { return m.path == path; });
  if (it != sql_metrics_.end()) {
    it->sql = stripped_sql;
    return base::OkStatus();
  }

  auto sep_idx = path.rfind('/');
  std::string basename =
      sep_idx == std::string::npos ? path : path.substr(sep_idx + 1);

  auto sql_idx = basename.rfind(".sql");
  if (sql_idx == std::string::npos) {
    return base::ErrStatus("Unable to find .sql extension for metric");
  }
  auto no_ext_name = basename.substr(0, sql_idx);

  metrics::SqlMetricFile metric;
  metric.path = path;
  metric.sql = stripped_sql;

  if (IsRootMetricField(no_ext_name)) {
    metric.proto_field_name = no_ext_name;
    metric.output_table_name = no_ext_name + "_output";

    auto field_it_and_inserted =
        proto_field_to_sql_metric_path_.emplace(*metric.proto_field_name, path);
    if (!field_it_and_inserted.second) {
      // We already had a metric with this field name in the map. However, if
      // this was the case, we should have found the metric in
      // |path_to_sql_metric_file_| above if we are simply overriding the
      // metric. Return an error since this means we have two different SQL
      // files which are trying to output the same metric.
      const auto& prev_path = field_it_and_inserted.first->second;
      PERFETTO_DCHECK(prev_path != path);
      return base::ErrStatus(
          "RegisterMetric Error: Metric paths %s (which is already "
          "registered) "
          "and %s are both trying to output the proto field %s",
          prev_path.c_str(), path.c_str(), metric.proto_field_name->c_str());
    }

    InsertIntoTraceMetricsTable(*db_, no_ext_name);
  }

  sql_metrics_.emplace_back(metric);
  return base::OkStatus();
}

base::Status TraceProcessorImpl::ExtendMetricsProto(const uint8_t* data,
                                                    size_t size) {
  return ExtendMetricsProto(data, size, /*skip_prefixes*/ {});
}

base::Status TraceProcessorImpl::ExtendMetricsProto(
    const uint8_t* data,
    size_t size,
    const std::vector<std::string>& skip_prefixes) {
  base::Status status =
      pool_.AddFromFileDescriptorSet(data, size, skip_prefixes);
  if (!status.ok())
    return status;

  for (uint32_t i = 0; i < pool_.descriptors().size(); ++i) {
    // Convert the full name (e.g. .perfetto.protos.TraceMetrics.SubMetric)
    // into a function name of the form (TraceMetrics_SubMetric).
    const auto& desc = pool_.descriptors()[i];
    auto fn_name = desc.full_name().substr(desc.package_name().size() + 1);
    std::replace(fn_name.begin(), fn_name.end(), '.', '_');
    RegisterFunction<metrics::BuildProto>(
        db_.get(), fn_name.c_str(), -1,
        std::unique_ptr<metrics::BuildProto::Context>(
            new metrics::BuildProto::Context{this, &pool_, i}));
  }
  return base::OkStatus();
}

base::Status TraceProcessorImpl::ComputeMetric(
    const std::vector<std::string>& metric_names,
    std::vector<uint8_t>* metrics_proto) {
  auto opt_idx = pool_.FindDescriptorIdx(".perfetto.protos.TraceMetrics");
  if (!opt_idx.has_value())
    return base::Status("Root metrics proto descriptor not found");

  const auto& root_descriptor = pool_.descriptors()[opt_idx.value()];
  return metrics::ComputeMetrics(this, metric_names, sql_metrics_, pool_,
                                 root_descriptor, metrics_proto);
}

base::Status TraceProcessorImpl::ComputeMetricText(
    const std::vector<std::string>& metric_names,
    TraceProcessor::MetricResultFormat format,
    std::string* metrics_string) {
  std::vector<uint8_t> metrics_proto;
  base::Status status = ComputeMetric(metric_names, &metrics_proto);
  if (!status.ok())
    return status;
  switch (format) {
    case TraceProcessor::MetricResultFormat::kProtoText:
      *metrics_string = protozero_to_text::ProtozeroToText(
          pool_, ".perfetto.protos.TraceMetrics",
          protozero::ConstBytes{metrics_proto.data(), metrics_proto.size()},
          protozero_to_text::kIncludeNewLines);
      break;
    case TraceProcessor::MetricResultFormat::kJson:
      // TODO(dproy): Implement this.
      PERFETTO_FATAL("Json formatted metrics not supported yet.");
      break;
  }
  return status;
}

std::vector<uint8_t> TraceProcessorImpl::GetMetricDescriptors() {
  return pool_.SerializeAsDescriptorSet();
}
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

void TraceProcessorImpl::EnableMetatrace(MetatraceConfig config) {
  metatrace::Enable(config);
}

<<<<<<< HEAD
// =================================================================
// |                      Experimental                             |
// =================================================================

base::Status TraceProcessorImpl::AnalyzeStructuredQueries(
    const std::vector<StructuredQueryBytes>& sqs,
    std::vector<AnalyzedStructuredQuery>* output) {
  auto opt_idx = metrics_descriptor_pool_.FindDescriptorIdx(
      ".perfetto.protos.TraceSummarySpec");
  if (!opt_idx) {
    metrics_descriptor_pool_.AddFromFileDescriptorSet(
        kTraceSummaryDescriptor.data(), kTraceSummaryDescriptor.size());
  }
  perfetto_sql::generator::StructuredQueryGenerator sqg;
  for (const auto& sq : sqs) {
    AnalyzedStructuredQuery analyzed_sq;
    ASSIGN_OR_RETURN(analyzed_sq.sql, sqg.Generate(sq.ptr, sq.size));
    analyzed_sq.textproto =
        perfetto::trace_processor::protozero_to_text::ProtozeroToText(
            metrics_descriptor_pool_,
            ".perfetto.protos.PerfettoSqlStructuredQuery",
            protozero::ConstBytes{sq.ptr, sq.size},
            perfetto::trace_processor::protozero_to_text::kIncludeNewLines);
    analyzed_sq.modules = sqg.ComputeReferencedModules();
    analyzed_sq.preambles = sqg.ComputePreambles();
    sqg.AddQuery(sq.ptr, sq.size);
    output->push_back(analyzed_sq);
  }
  return base::OkStatus();
}

=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
namespace {

class StringInterner {
 public:
  StringInterner(protos::pbzero::PerfettoMetatrace& event,
                 base::FlatHashMap<std::string, uint64_t>& interned_strings)
      : event_(event), interned_strings_(interned_strings) {}

  ~StringInterner() {
    for (const auto& interned_string : new_interned_strings_) {
      auto* interned_string_proto = event_.add_interned_strings();
      interned_string_proto->set_iid(interned_string.first);
      interned_string_proto->set_value(interned_string.second);
    }
  }

  uint64_t InternString(const std::string& str) {
    uint64_t new_iid = interned_strings_.size();
    auto insert_result = interned_strings_.Insert(str, new_iid);
    if (insert_result.second) {
      new_interned_strings_.emplace_back(new_iid, str);
    }
    return *insert_result.first;
  }

 private:
  protos::pbzero::PerfettoMetatrace& event_;
  base::FlatHashMap<std::string, uint64_t>& interned_strings_;

  base::SmallVector<std::pair<uint64_t, std::string>, 16> new_interned_strings_;
};

}  // namespace

base::Status TraceProcessorImpl::DisableAndReadMetatrace(
    std::vector<uint8_t>* trace_proto) {
  protozero::HeapBuffered<protos::pbzero::Trace> trace;

<<<<<<< HEAD
  auto* clock_snapshot = trace->add_packet()->set_clock_snapshot();
  for (const auto& [clock_id, ts] : base::CaptureClockSnapshots()) {
    auto* clock = clock_snapshot->add_clocks();
    clock->set_clock_id(clock_id);
    clock->set_timestamp(ts);
  }

  auto tid = static_cast<uint32_t>(base::GetThreadId());
  base::FlatHashMap<std::string, uint64_t> interned_strings;
  metatrace::DisableAndReadBuffer(
      [&trace, &interned_strings, tid](metatrace::Record* record) {
        auto* packet = trace->add_packet();
        packet->set_timestamp(record->timestamp_ns);
        auto* evt = packet->set_perfetto_metatrace();

        StringInterner interner(*evt, interned_strings);

        evt->set_event_name_iid(interner.InternString(record->event_name));
        evt->set_event_duration_ns(record->duration_ns);
        evt->set_thread_id(tid);

        if (record->args_buffer_size == 0)
          return;

        base::StringSplitter s(
            record->args_buffer, record->args_buffer_size, '\0',
            base::StringSplitter::EmptyTokenMode::ALLOW_EMPTY_TOKENS);
        for (; s.Next();) {
          auto* arg_proto = evt->add_args();
          arg_proto->set_key_iid(interner.InternString(s.cur_token()));

          bool has_next = s.Next();
          PERFETTO_CHECK(has_next);
          arg_proto->set_value_iid(interner.InternString(s.cur_token()));
        }
      });
=======
  {
    uint64_t realtime_timestamp = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch() /
        std::chrono::nanoseconds(1));
    uint64_t boottime_timestamp = metatrace::TraceTimeNowNs();
    auto* clock_snapshot = trace->add_packet()->set_clock_snapshot();
    {
      auto* realtime_clock = clock_snapshot->add_clocks();
      realtime_clock->set_clock_id(
          protos::pbzero::BuiltinClock::BUILTIN_CLOCK_REALTIME);
      realtime_clock->set_timestamp(realtime_timestamp);
    }
    {
      auto* boottime_clock = clock_snapshot->add_clocks();
      boottime_clock->set_clock_id(
          protos::pbzero::BuiltinClock::BUILTIN_CLOCK_BOOTTIME);
      boottime_clock->set_timestamp(boottime_timestamp);
    }
  }

  base::FlatHashMap<std::string, uint64_t> interned_strings;
  metatrace::DisableAndReadBuffer([&trace, &interned_strings](
                                      metatrace::Record* record) {
    auto packet = trace->add_packet();
    packet->set_timestamp(record->timestamp_ns);
    auto* evt = packet->set_perfetto_metatrace();

    StringInterner interner(*evt, interned_strings);

    evt->set_event_name_iid(interner.InternString(record->event_name));
    evt->set_event_duration_ns(record->duration_ns);
    evt->set_thread_id(1);  // Not really important, just required for the ui.

    if (record->args_buffer_size == 0)
      return;

    base::StringSplitter s(
        record->args_buffer, record->args_buffer_size, '\0',
        base::StringSplitter::EmptyTokenMode::ALLOW_EMPTY_TOKENS);
    for (; s.Next();) {
      auto* arg_proto = evt->add_args();
      arg_proto->set_key_iid(interner.InternString(s.cur_token()));

      bool has_next = s.Next();
      PERFETTO_CHECK(has_next);
      arg_proto->set_value_iid(interner.InternString(s.cur_token()));
    }
  });
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  *trace_proto = trace.SerializeAsArray();
  return base::OkStatus();
}

<<<<<<< HEAD
// =================================================================
// |              Advanced functionality starts here               |
// =================================================================

std::string TraceProcessorImpl::GetCurrentTraceName() {
  if (current_trace_name_.empty())
    return "";
  auto size = " (" + std::to_string(bytes_parsed_ / 1024 / 1024) + " MB)";
  return current_trace_name_ + size;
}

void TraceProcessorImpl::SetCurrentTraceName(const std::string& name) {
  current_trace_name_ = name;
}

base::Status TraceProcessorImpl::RegisterFileContent(
    [[maybe_unused]] const std::string& path,
    [[maybe_unused]] TraceBlobView content) {
#if PERFETTO_BUILDFLAG(PERFETTO_ENABLE_ETM_IMPORTER)
  return etm::FileTracker::GetOrCreate(&context_)->AddFile(path,
                                                           std::move(content));
#else
  return base::OkStatus();
#endif
}

void TraceProcessorImpl::InterruptQuery() {
  if (!engine_->sqlite_engine()->db())
    return;
  query_interrupted_.store(true);
  sqlite3_interrupt(engine_->sqlite_engine()->db());
}

size_t TraceProcessorImpl::RestoreInitialTables() {
  // We should always have at least as many objects now as we did in the
  // constructor.
  uint64_t registered_count_before = engine_->SqliteRegisteredObjectCount();
  PERFETTO_CHECK(registered_count_before >= sqlite_objects_post_prelude_);

  InitPerfettoSqlEngine();

  // The registered count should now be the same as it was in the constructor.
  uint64_t registered_count_after = engine_->SqliteRegisteredObjectCount();
  PERFETTO_CHECK(registered_count_after == sqlite_objects_post_prelude_);
  return static_cast<size_t>(registered_count_before - registered_count_after);
}

// =================================================================
// |  Trace-based metrics (v1) related functionality starts here   |
// =================================================================

base::Status TraceProcessorImpl::RegisterMetric(const std::string& path,
                                                const std::string& sql) {
  // Check if the metric with the given path already exists and if it does,
  // just update the SQL associated with it.
  auto it = std::find_if(
      sql_metrics_.begin(), sql_metrics_.end(),
      [&path](const metrics::SqlMetricFile& m) { return m.path == path; });
  if (it != sql_metrics_.end()) {
    it->sql = sql;
    return base::OkStatus();
  }

  auto sep_idx = path.rfind('/');
  std::string basename =
      sep_idx == std::string::npos ? path : path.substr(sep_idx + 1);

  auto sql_idx = basename.rfind(".sql");
  if (sql_idx == std::string::npos) {
    return base::ErrStatus("Unable to find .sql extension for metric");
  }
  auto no_ext_name = basename.substr(0, sql_idx);

  metrics::SqlMetricFile metric;
  metric.path = path;
  metric.sql = sql;

  if (IsRootMetricField(no_ext_name)) {
    metric.proto_field_name = no_ext_name;
    metric.output_table_name = no_ext_name + "_output";

    auto field_it_and_inserted =
        proto_field_to_sql_metric_path_.emplace(*metric.proto_field_name, path);
    if (!field_it_and_inserted.second) {
      // We already had a metric with this field name in the map. However, if
      // this was the case, we should have found the metric in
      // |path_to_sql_metric_file_| above if we are simply overriding the
      // metric. Return an error since this means we have two different SQL
      // files which are trying to output the same metric.
      const auto& prev_path = field_it_and_inserted.first->second;
      PERFETTO_DCHECK(prev_path != path);
      return base::ErrStatus(
          "RegisterMetric Error: Metric paths %s (which is already "
          "registered) "
          "and %s are both trying to output the proto field %s",
          prev_path.c_str(), path.c_str(), metric.proto_field_name->c_str());
    }
  }

  if (metric.proto_field_name) {
    InsertIntoTraceMetricsTable(engine_->sqlite_engine()->db(),
                                *metric.proto_field_name);
  }
  sql_metrics_.emplace_back(metric);
  return base::OkStatus();
}

base::Status TraceProcessorImpl::ExtendMetricsProto(const uint8_t* data,
                                                    size_t size) {
  return ExtendMetricsProto(data, size, /*skip_prefixes*/ {});
}

base::Status TraceProcessorImpl::ExtendMetricsProto(
    const uint8_t* data,
    size_t size,
    const std::vector<std::string>& skip_prefixes) {
  RETURN_IF_ERROR(metrics_descriptor_pool_.AddFromFileDescriptorSet(
      data, size, skip_prefixes));
  RETURN_IF_ERROR(RegisterAllProtoBuilderFunctions(
      &metrics_descriptor_pool_, &proto_fn_name_to_path_, engine_.get(), this));
  return base::OkStatus();
}

base::Status TraceProcessorImpl::ComputeMetric(
    const std::vector<std::string>& metric_names,
    std::vector<uint8_t>* metrics_proto) {
  auto opt_idx = metrics_descriptor_pool_.FindDescriptorIdx(
      ".perfetto.protos.TraceMetrics");
  if (!opt_idx.has_value())
    return base::Status("Root metrics proto descriptor not found");

  const auto& root_descriptor =
      metrics_descriptor_pool_.descriptors()[opt_idx.value()];
  return metrics::ComputeMetrics(engine_.get(), metric_names, sql_metrics_,
                                 metrics_descriptor_pool_, root_descriptor,
                                 metrics_proto);
}

base::Status TraceProcessorImpl::ComputeMetricText(
    const std::vector<std::string>& metric_names,
    TraceProcessor::MetricResultFormat format,
    std::string* metrics_string) {
  std::vector<uint8_t> metrics_proto;
  base::Status status = ComputeMetric(metric_names, &metrics_proto);
  if (!status.ok())
    return status;
  switch (format) {
    case TraceProcessor::MetricResultFormat::kProtoText:
      *metrics_string = protozero_to_text::ProtozeroToText(
          metrics_descriptor_pool_, ".perfetto.protos.TraceMetrics",
          protozero::ConstBytes{metrics_proto.data(), metrics_proto.size()},
          protozero_to_text::kIncludeNewLines);
      break;
    case TraceProcessor::MetricResultFormat::kJson:
      *metrics_string = protozero_to_json::ProtozeroToJson(
          metrics_descriptor_pool_, ".perfetto.protos.TraceMetrics",
          protozero::ConstBytes{metrics_proto.data(), metrics_proto.size()},
          protozero_to_json::kPretty | protozero_to_json::kInlineErrors |
              protozero_to_json::kInlineAnnotations);
      break;
  }
  return status;
}

std::vector<uint8_t> TraceProcessorImpl::GetMetricDescriptors() {
  return metrics_descriptor_pool_.SerializeAsDescriptorSet();
}

void TraceProcessorImpl::InitPerfettoSqlEngine() {
  engine_ = std::make_unique<PerfettoSqlEngine>(
      context_.storage->mutable_string_pool(), &dataframe_shared_storage_,
      config_.enable_extra_checks);
  sqlite3* db = engine_->sqlite_engine()->db();
  sqlite3_str_split_init(db);

  // Register SQL functions only used in local development instances.
  if (config_.enable_dev_features) {
    RegisterFunction<WriteFile>(engine_.get(), "WRITE_FILE", 2);
  }
  RegisterFunction<Glob>(engine_.get(), "glob", 2);
  RegisterFunction<Hash>(engine_.get(), "HASH", -1);
  RegisterFunction<Base64Encode>(engine_.get(), "BASE64_ENCODE", 1);
  RegisterFunction<Demangle>(engine_.get(), "DEMANGLE", 1);
  RegisterFunction<SourceGeq>(engine_.get(), "SOURCE_GEQ", -1);
  RegisterFunction<TablePtrBind>(engine_.get(), "__intrinsic_table_ptr_bind",
                                 -1);
  RegisterFunction<ExportJson>(engine_.get(), "EXPORT_JSON", 1,
                               context_.storage.get(), false);
  RegisterFunction<ExtractArg>(engine_.get(), "EXTRACT_ARG", 2,
                               context_.storage.get());
  RegisterFunction<AbsTimeStr>(engine_.get(), "ABS_TIME_STR", 1,
                               context_.clock_converter.get());
  RegisterFunction<Reverse>(engine_.get(), "REVERSE", 1);
  RegisterFunction<ToMonotonic>(engine_.get(), "TO_MONOTONIC", 1,
                                context_.clock_converter.get());
  RegisterFunction<ToRealtime>(engine_.get(), "TO_REALTIME", 1,
                               context_.clock_converter.get());
  RegisterFunction<ToTimecode>(engine_.get(), "TO_TIMECODE", 1);
  RegisterFunction<CreateFunction>(engine_.get(), "CREATE_FUNCTION", 3,
                                   engine_.get());
  RegisterFunction<CreateViewFunction>(engine_.get(), "CREATE_VIEW_FUNCTION", 3,
                                       engine_.get());
  RegisterFunction<ExperimentalMemoize>(engine_.get(), "EXPERIMENTAL_MEMOIZE",
                                        1, engine_.get());
  RegisterFunction<Import>(
      engine_.get(), "IMPORT", 1,
      std::make_unique<Import::Context>(Import::Context{engine_.get()}));
  RegisterFunction<ToFtrace>(
      engine_.get(), "TO_FTRACE", 1,
      std::make_unique<ToFtrace::Context>(ToFtrace::Context{
          context_.storage.get(), SystraceSerializer(&context_)}));

  if constexpr (regex::IsRegexSupported()) {
    RegisterFunction<Regex>(engine_.get(), "regexp", 2);
  }
  // Old style function registration.
  // TODO(lalitm): migrate this over to using RegisterFunction once aggregate
  // functions are supported.
  RegisterValueAtMaxTsFunction(*engine_);
  {
    base::Status status = RegisterLastNonNullFunction(*engine_);
    if (!status.ok())
      PERFETTO_FATAL("%s", status.c_message());
  }
  {
    base::Status status = RegisterStackFunctions(engine_.get(), &context_);
    if (!status.ok())
      PERFETTO_FATAL("%s", status.c_message());
  }
  {
    base::Status status = RegisterStripHexFunction(engine_.get(), &context_);
    if (!status.ok())
      PERFETTO_FATAL("%s", status.c_message());
  }
  {
    base::Status status = PprofFunctions::Register(*engine_, &context_);
    if (!status.ok())
      PERFETTO_FATAL("%s", status.c_message());
  }
  {
    base::Status status = RegisterLayoutFunctions(*engine_);
    if (!status.ok())
      PERFETTO_FATAL("%s", status.c_message());
  }
  {
    base::Status status = RegisterMathFunctions(*engine_);
    if (!status.ok())
      PERFETTO_FATAL("%s", status.c_message());
  }
  {
    base::Status status = RegisterBase64Functions(*engine_);
    if (!status.ok())
      PERFETTO_FATAL("%s", status.c_message());
  }
  {
    base::Status status = RegisterTypeBuilderFunctions(*engine_);
    if (!status.ok())
      PERFETTO_FATAL("%s", status.c_message());
  }
  {
    base::Status status = RegisterGraphScanFunctions(
        *engine_, context_.storage->mutable_string_pool());
    if (!status.ok())
      PERFETTO_FATAL("%s", status.c_message());
  }
  {
    base::Status status = RegisterGraphTraversalFunctions(
        *engine_, *context_.storage->mutable_string_pool());
    if (!status.ok())
      PERFETTO_FATAL("%s", status.c_message());
  }
  {
    base::Status status = perfetto_sql::RegisterIntervalIntersectFunctions(
        *engine_, context_.storage->mutable_string_pool());
  }
  {
    base::Status status = perfetto_sql::RegisterCounterIntervalsFunctions(
        *engine_, context_.storage->mutable_string_pool());
  }

  TraceStorage* storage = context_.storage.get();

  // Operator tables.
  engine_->RegisterVirtualTableModule<SpanJoinOperatorModule>(
      "span_join",
      std::make_unique<SpanJoinOperatorModule::Context>(engine_.get()));
  engine_->RegisterVirtualTableModule<SpanJoinOperatorModule>(
      "span_left_join",
      std::make_unique<SpanJoinOperatorModule::Context>(engine_.get()));
  engine_->RegisterVirtualTableModule<SpanJoinOperatorModule>(
      "span_outer_join",
      std::make_unique<SpanJoinOperatorModule::Context>(engine_.get()));
  engine_->RegisterVirtualTableModule<WindowOperatorModule>(
      "__intrinsic_window", nullptr);
  engine_->RegisterVirtualTableModule<CounterMipmapOperator>(
      "__intrinsic_counter_mipmap",
      std::make_unique<CounterMipmapOperator::Context>(engine_.get()));
  engine_->RegisterVirtualTableModule<SliceMipmapOperator>(
      "__intrinsic_slice_mipmap",
      std::make_unique<SliceMipmapOperator::Context>(engine_.get()));
#if PERFETTO_BUILDFLAG(PERFETTO_ENABLE_ETM_IMPORTER)
  engine_->RegisterVirtualTableModule<etm::EtmDecodeTraceVtable>(
      "__intrinsic_etm_decode_trace", storage);
  engine_->RegisterVirtualTableModule<etm::EtmIterateRangeVtable>(
      "__intrinsic_etm_iterate_instruction_range", storage);
#endif

  // Register stdlib packages.
  auto packages = GetStdlibPackages();
  for (auto package = packages.GetIterator(); package; ++package) {
    base::Status status =
        RegisterSqlPackage({/*name=*/package.key(), /*modules=*/package.value(),
                            /*allow_override=*/false});
    if (!status.ok())
      PERFETTO_ELOG("%s", status.c_message());
  }

  // Register metrics functions.
  {
    base::Status status =
        engine_->RegisterSqliteAggregateFunction<metrics::RepeatedField>(
            nullptr);
    if (!status.ok())
      PERFETTO_ELOG("%s", status.c_message());
  }

  RegisterFunction<metrics::NullIfEmpty>(engine_.get(), "NULL_IF_EMPTY", 1);
  RegisterFunction<metrics::UnwrapMetricProto>(engine_.get(),
                                               "UNWRAP_METRIC_PROTO", 2);
  RegisterFunction<metrics::RunMetric>(
      engine_.get(), "RUN_METRIC", -1,
      std::make_unique<metrics::RunMetric::Context>(
          metrics::RunMetric::Context{engine_.get(), &sql_metrics_}));

  // Legacy tables.
  engine_->RegisterVirtualTableModule<SqlStatsModule>("sqlstats", storage);
  engine_->RegisterVirtualTableModule<StatsModule>("stats", storage);
  engine_->RegisterVirtualTableModule<TablePointerModule>(
      "__intrinsic_table_ptr", nullptr);

  // New style db-backed tables.
  // Note: if adding a table here which might potentially contain many rows
  // (O(rows in sched/slice/counter)), then consider calling ShrinkToFit on
  // that table in TraceStorage::ShrinkToFitTables.
  RegisterStaticTable(storage->mutable_machine_table());
  RegisterStaticTable(storage->mutable_arg_table());
  RegisterStaticTable(storage->mutable_chrome_raw_table());
  RegisterStaticTable(storage->mutable_ftrace_event_table());
  RegisterStaticTable(storage->mutable_thread_table());
  RegisterStaticTable(storage->mutable_process_table());
  RegisterStaticTable(storage->mutable_filedescriptor_table());
  RegisterStaticTable(storage->mutable_trace_file_table());

  RegisterStaticTable(storage->mutable_slice_table());
  RegisterStaticTable(storage->mutable_flow_table());
  RegisterStaticTable(storage->mutable_sched_slice_table());
  RegisterStaticTable(storage->mutable_spurious_sched_wakeup_table());
  RegisterStaticTable(storage->mutable_thread_state_table());

  RegisterStaticTable(storage->mutable_track_table());

  RegisterStaticTable(storage->mutable_counter_table());

  RegisterStaticTable(storage->mutable_gpu_counter_group_table());

  RegisterStaticTable(storage->mutable_heap_graph_object_table());
  RegisterStaticTable(storage->mutable_heap_graph_reference_table());
  RegisterStaticTable(storage->mutable_heap_graph_class_table());

  RegisterStaticTable(storage->mutable_symbol_table());
  RegisterStaticTable(storage->mutable_heap_profile_allocation_table());
  RegisterStaticTable(storage->mutable_cpu_profile_stack_sample_table());
  RegisterStaticTable(storage->mutable_perf_session_table());
  RegisterStaticTable(storage->mutable_perf_sample_table());
  RegisterStaticTable(storage->mutable_instruments_sample_table());
  RegisterStaticTable(storage->mutable_stack_profile_callsite_table());
  RegisterStaticTable(storage->mutable_stack_profile_mapping_table());
  RegisterStaticTable(storage->mutable_stack_profile_frame_table());
  RegisterStaticTable(storage->mutable_package_list_table());
  RegisterStaticTable(storage->mutable_profiler_smaps_table());

  RegisterStaticTable(storage->mutable_android_log_table());
  RegisterStaticTable(storage->mutable_android_dumpstate_table());
  RegisterStaticTable(storage->mutable_android_game_intervenion_list_table());
  RegisterStaticTable(storage->mutable_android_key_events_table());
  RegisterStaticTable(storage->mutable_android_motion_events_table());
  RegisterStaticTable(storage->mutable_android_input_event_dispatch_table());

  RegisterStaticTable(storage->mutable_vulkan_memory_allocations_table());

  RegisterStaticTable(storage->mutable_android_network_packets_table());

  RegisterStaticTable(storage->mutable_v8_isolate_table());
  RegisterStaticTable(storage->mutable_v8_js_script_table());
  RegisterStaticTable(storage->mutable_v8_wasm_script_table());
  RegisterStaticTable(storage->mutable_v8_js_function_table());
  RegisterStaticTable(storage->mutable_v8_js_code_table());
  RegisterStaticTable(storage->mutable_v8_internal_code_table());
  RegisterStaticTable(storage->mutable_v8_wasm_code_table());
  RegisterStaticTable(storage->mutable_v8_regexp_code_table());

  RegisterStaticTable(storage->mutable_jit_code_table());
  RegisterStaticTable(storage->mutable_jit_frame_table());

  RegisterStaticTable(storage->mutable_etm_v4_configuration_table());
  RegisterStaticTable(storage->mutable_etm_v4_session_table());
  RegisterStaticTable(storage->mutable_etm_v4_trace_table());
  RegisterStaticTable(storage->mutable_elf_file_table());
  RegisterStaticTable(storage->mutable_file_table());

  RegisterStaticTable(storage->mutable_spe_record_table());
  RegisterStaticTable(storage->mutable_mmap_record_table());

  RegisterStaticTable(storage->mutable_inputmethod_clients_table());
  RegisterStaticTable(storage->mutable_inputmethod_manager_service_table());
  RegisterStaticTable(storage->mutable_inputmethod_service_table());

  RegisterStaticTable(storage->mutable_surfaceflinger_layers_snapshot_table());
  RegisterStaticTable(storage->mutable_surfaceflinger_layer_table());
  RegisterStaticTable(storage->mutable_surfaceflinger_transactions_table());
  RegisterStaticTable(storage->mutable_surfaceflinger_transaction_table());
  RegisterStaticTable(storage->mutable_surfaceflinger_transaction_flag_table());

  RegisterStaticTable(storage->mutable_viewcapture_table());
  RegisterStaticTable(storage->mutable_viewcapture_view_table());

  RegisterStaticTable(storage->mutable_windowmanager_table());

  RegisterStaticTable(
      storage->mutable_window_manager_shell_transitions_table());
  RegisterStaticTable(
      storage->mutable_window_manager_shell_transition_handlers_table());
  RegisterStaticTable(
      storage->mutable_window_manager_shell_transition_participants_table());
  RegisterStaticTable(
      storage->mutable_window_manager_shell_transition_protos_table());

  RegisterStaticTable(storage->mutable_protolog_table());

  RegisterStaticTable(storage->mutable_metadata_table());
  RegisterStaticTable(storage->mutable_cpu_table());
  RegisterStaticTable(storage->mutable_cpu_freq_table());
  RegisterStaticTable(storage->mutable_clock_snapshot_table());

  RegisterStaticTable(storage->mutable_memory_snapshot_table());
  RegisterStaticTable(storage->mutable_process_memory_snapshot_table());
  RegisterStaticTable(storage->mutable_memory_snapshot_node_table());
  RegisterStaticTable(storage->mutable_memory_snapshot_edge_table());

  RegisterStaticTable(storage->mutable_experimental_proto_path_table());
  RegisterStaticTable(storage->mutable_experimental_proto_content_table());

  RegisterStaticTable(
      storage->mutable_experimental_missing_chrome_processes_table());

  // Tables dynamically generated at query time.
  engine_->RegisterStaticTableFunction(
      std::make_unique<ExperimentalFlamegraph>(&context_));
  engine_->RegisterStaticTableFunction(
      std::make_unique<ExperimentalSliceLayout>(
          context_.storage->mutable_string_pool(), &storage->slice_table()));
  engine_->RegisterStaticTableFunction(std::make_unique<TableInfo>(
      context_.storage->mutable_string_pool(), engine_.get()));
  engine_->RegisterStaticTableFunction(std::make_unique<Ancestor>(
      Ancestor::Type::kSlice, context_.storage.get()));
  engine_->RegisterStaticTableFunction(std::make_unique<Ancestor>(
      Ancestor::Type::kStackProfileCallsite, context_.storage.get()));
  engine_->RegisterStaticTableFunction(std::make_unique<Ancestor>(
      Ancestor::Type::kSliceByStack, context_.storage.get()));
  engine_->RegisterStaticTableFunction(std::make_unique<Descendant>(
      Descendant::Type::kSlice, context_.storage.get()));
  engine_->RegisterStaticTableFunction(std::make_unique<Descendant>(
      Descendant::Type::kSliceByStack, context_.storage.get()));
  engine_->RegisterStaticTableFunction(std::make_unique<ConnectedFlow>(
      ConnectedFlow::Mode::kDirectlyConnectedFlow, context_.storage.get()));
  engine_->RegisterStaticTableFunction(std::make_unique<ConnectedFlow>(
      ConnectedFlow::Mode::kPrecedingFlow, context_.storage.get()));
  engine_->RegisterStaticTableFunction(std::make_unique<ConnectedFlow>(
      ConnectedFlow::Mode::kFollowingFlow, context_.storage.get()));
  engine_->RegisterStaticTableFunction(
      std::make_unique<ExperimentalAnnotatedStack>(&context_));
  engine_->RegisterStaticTableFunction(
      std::make_unique<ExperimentalFlatSlice>(&context_));
  engine_->RegisterStaticTableFunction(std::make_unique<DfsWeightBounded>(
      context_.storage->mutable_string_pool()));
  engine_->RegisterStaticTableFunction(
      std::make_unique<WinscopeProtoToArgsWithDefaults>(
          context_.storage->mutable_string_pool(), engine_.get(), &context_));

  // Value table aggregate functions.
  engine_->RegisterSqliteAggregateFunction<DominatorTree>(
      context_.storage->mutable_string_pool());
  engine_->RegisterSqliteAggregateFunction<StructuralTreePartition>(
      context_.storage->mutable_string_pool());

  // Metrics.
  {
    auto status = RegisterAllProtoBuilderFunctions(&metrics_descriptor_pool_,
                                                   &proto_fn_name_to_path_,
                                                   engine_.get(), this);
    if (!status.ok()) {
      PERFETTO_FATAL("%s", status.c_message());
    }
  }

  // Import prelude package.
  IncludeBeforeEofPrelude();
  if (notify_eof_called_) {
    IncludeAfterEofPrelude();
  }

  for (const auto& metric : sql_metrics_) {
    if (metric.proto_field_name) {
      InsertIntoTraceMetricsTable(db, *metric.proto_field_name);
    }
  }

  // Fill trace bounds table.
  BuildBoundsTable(db, GetTraceTimestampBoundsNs(*context_.storage));

  // Reregister manually added stdlib packages.
  for (const auto& package : manually_registered_sql_packages_) {
    RegisterSqlPackage(package);
  }
}

void TraceProcessorImpl::IncludeBeforeEofPrelude() {
  auto result = engine_->Execute(SqlSource::FromTraceProcessorImplementation(
      "INCLUDE PERFETTO MODULE prelude.before_eof.*"));
  if (!result.status().ok()) {
    PERFETTO_FATAL("Failed to import prelude: %s", result.status().c_message());
  }
}

void TraceProcessorImpl::IncludeAfterEofPrelude() {
  auto result = engine_->Execute(SqlSource::FromTraceProcessorImplementation(
      "INCLUDE PERFETTO MODULE prelude.after_eof.*"));
  if (!result.status().ok()) {
    PERFETTO_FATAL("Failed to import prelude: %s", result.status().c_message());
  }
}

bool TraceProcessorImpl::IsRootMetricField(const std::string& metric_name) {
  std::optional<uint32_t> desc_idx = metrics_descriptor_pool_.FindDescriptorIdx(
      ".perfetto.protos.TraceMetrics");
  if (!desc_idx.has_value())
    return false;
  const auto* field_idx =
      metrics_descriptor_pool_.descriptors()[*desc_idx].FindFieldByName(
          metric_name);
  return field_idx != nullptr;
}

}  // namespace perfetto::trace_processor
=======
}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
