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

#ifndef SRC_TRACE_PROCESSOR_TRACE_PROCESSOR_IMPL_H_
#define SRC_TRACE_PROCESSOR_TRACE_PROCESSOR_IMPL_H_

#include <sqlite3.h>

#include <atomic>
<<<<<<< HEAD
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "perfetto/base/status.h"
#include "perfetto/trace_processor/basic_types.h"
#include "perfetto/trace_processor/trace_blob_view.h"
#include "perfetto/trace_processor/trace_processor.h"
#include "src/trace_processor/iterator_impl.h"
#include "src/trace_processor/metrics/metrics.h"
#include "src/trace_processor/perfetto_sql/engine/dataframe_shared_storage.h"
#include "src/trace_processor/perfetto_sql/engine/perfetto_sql_engine.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/create_function.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/create_view_function.h"
#include "src/trace_processor/trace_processor_storage_impl.h"
#include "src/trace_processor/util/descriptors.h"

namespace perfetto::trace_processor {
=======
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "perfetto/ext/base/flat_hash_map.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/trace_processor/basic_types.h"
#include "perfetto/trace_processor/status.h"
#include "perfetto/trace_processor/trace_processor.h"
#include "src/trace_processor/prelude/functions/create_function.h"
#include "src/trace_processor/prelude/functions/create_view_function.h"
#include "src/trace_processor/prelude/functions/import.h"
#include "src/trace_processor/sqlite/db_sqlite_table.h"
#include "src/trace_processor/sqlite/query_cache.h"
#include "src/trace_processor/sqlite/scoped_db.h"
#include "src/trace_processor/trace_processor_storage_impl.h"
#include "src/trace_processor/util/sql_modules.h"

#include "src/trace_processor/metrics/metrics.h"
#include "src/trace_processor/util/descriptors.h"

namespace perfetto {
namespace trace_processor {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

// Coordinates the loading of traces from an arbitrary source and allows
// execution of SQL queries on the events in these traces.
class TraceProcessorImpl : public TraceProcessor,
                           public TraceProcessorStorageImpl {
 public:
  explicit TraceProcessorImpl(const Config&);

  TraceProcessorImpl(const TraceProcessorImpl&) = delete;
  TraceProcessorImpl& operator=(const TraceProcessorImpl&) = delete;

  TraceProcessorImpl(TraceProcessorImpl&&) = delete;
  TraceProcessorImpl& operator=(TraceProcessorImpl&&) = delete;

  ~TraceProcessorImpl() override;

<<<<<<< HEAD
  // =================================================================
  // |        TraceProcessorStorage implementation starts here       |
  // =================================================================

  base::Status Parse(TraceBlobView) override;
  void Flush() override;
  base::Status NotifyEndOfFile() override;

  // =================================================================
  // |        PerfettoSQL related functionality starts here          |
  // =================================================================

  Iterator ExecuteQuery(const std::string& sql) override;

  base::Status RegisterSqlPackage(SqlPackage) override;

  base::Status RegisterSqlModule(SqlModule module) override;

  // =================================================================
  // |  Trace-based metrics (v2) related functionality starts here   |
  // =================================================================

  base::Status Summarize(const TraceSummaryComputationSpec& computation,
                         const std::vector<TraceSummarySpecBytes>& specs,
                         std::vector<uint8_t>* output,
                         const TraceSummaryOutputSpec& output_spec) override;

  // =================================================================
  // |        Metatracing related functionality starts here          |
  // =================================================================

  void EnableMetatrace(MetatraceConfig config) override;

  base::Status DisableAndReadMetatrace(
      std::vector<uint8_t>* trace_proto) override;

  // =================================================================
  // |              Advanced functionality starts here               |
  // =================================================================

  std::string GetCurrentTraceName() override;
  void SetCurrentTraceName(const std::string&) override;

  base::Status RegisterFileContent(const std::string& path,
                                   TraceBlobView content) override;

  void InterruptQuery() override;

  size_t RestoreInitialTables() override;

  // =================================================================
  // |  Trace-based metrics (v1) related functionality starts here   |
  // =================================================================

  base::Status RegisterMetric(const std::string& path,
                              const std::string& sql) override;

  base::Status ExtendMetricsProto(const uint8_t* data, size_t size) override;
=======
  // TraceProcessorStorage implementation:
  base::Status Parse(TraceBlobView) override;
  void Flush() override;
  void NotifyEndOfFile() override;

  // TraceProcessor implementation:
  Iterator ExecuteQuery(const std::string& sql) override;

  base::Status RegisterMetric(const std::string& path,
                              const std::string& sql) override;

  base::Status RegisterSqlModule(SqlModule sql_module) override;

  base::Status ExtendMetricsProto(const uint8_t* data, size_t size) override;

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  base::Status ExtendMetricsProto(
      const uint8_t* data,
      size_t size,
      const std::vector<std::string>& skip_prefixes) override;

  base::Status ComputeMetric(const std::vector<std::string>& metric_names,
                             std::vector<uint8_t>* metrics) override;
<<<<<<< HEAD
=======

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  base::Status ComputeMetricText(const std::vector<std::string>& metric_names,
                                 TraceProcessor::MetricResultFormat format,
                                 std::string* metrics_string) override;

  std::vector<uint8_t> GetMetricDescriptors() override;

<<<<<<< HEAD
  // ===================
  // |  Experimental   |
  // ===================

  base::Status AnalyzeStructuredQueries(
      const std::vector<StructuredQueryBytes>&,
      std::vector<AnalyzedStructuredQuery>*) override;
=======
  void InterruptQuery() override;

  size_t RestoreInitialTables() override;

  std::string GetCurrentTraceName() override;
  void SetCurrentTraceName(const std::string&) override;

  void EnableMetatrace(MetatraceConfig config) override;

  base::Status DisableAndReadMetatrace(
      std::vector<uint8_t>* trace_proto) override;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

 private:
  // Needed for iterators to be able to access the context.
  friend class IteratorImpl;

  template <typename Table>
<<<<<<< HEAD
  void RegisterStaticTable(Table* table) {
    engine_->RegisterStaticTable(table, Table::Name(),
                                 Table::ComputeStaticSchema());
  }

  bool IsRootMetricField(const std::string& metric_name);

  void InitPerfettoSqlEngine();
  void IncludeBeforeEofPrelude();
  void IncludeAfterEofPrelude();

  const Config config_;

  DataframeSharedStorage dataframe_shared_storage_;
  std::unique_ptr<PerfettoSqlEngine> engine_;

  DescriptorPool metrics_descriptor_pool_;

  std::vector<metrics::SqlMetricFile> sql_metrics_;

  // Manually registers SQL packages are stored here, to be able to restore
  // them when running |RestoreInitialTables()|.
  std::vector<SqlPackage> manually_registered_sql_packages_;

  std::unordered_map<std::string, std::string> proto_field_to_sql_metric_path_;
  std::unordered_map<std::string, std::string> proto_fn_name_to_path_;
=======
  void RegisterDbTable(const Table& table) {
    DbSqliteTable::RegisterTable(*db_, query_cache_.get(), &table,
                                 Table::Name());
  }

  void RegisterTableFunction(std::unique_ptr<TableFunction> generator) {
    DbSqliteTable::RegisterTable(*db_, query_cache_.get(),
                                 std::move(generator));
  }

  template <typename View>
  void RegisterView(const View& view);

  bool IsRootMetricField(const std::string& metric_name);

  // Keep this first: we need this to be destroyed after we clean up
  // everything else.
  ScopedDb db_;

  // State necessary for CREATE_FUNCTION invocations. We store this here as we
  // need to finalize any prepared statements *before* we destroy the database.
  CreateFunction::State create_function_state_;

  std::unique_ptr<QueryCache> query_cache_;

  DescriptorPool pool_;

  // Map from module name to module contents. Used for IMPORT function.
  base::FlatHashMap<std::string, sql_modules::RegisteredModule> sql_modules_;
  std::vector<metrics::SqlMetricFile> sql_metrics_;
  std::unordered_map<std::string, std::string> proto_field_to_sql_metric_path_;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  // This is atomic because it is set by the CTRL-C signal handler and we need
  // to prevent single-flow compiler optimizations in ExecuteQuery().
  std::atomic<bool> query_interrupted_{false};

<<<<<<< HEAD
  // Track the number of objects registered with SQLite post prelude.
  uint64_t sqlite_objects_post_prelude_ = 0;
=======
  // Keeps track of the tables created by the ingestion process. This is used
  // by RestoreInitialTables() to delete all the tables/view that have been
  // created after that point.
  std::vector<std::string> initial_tables_;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  std::string current_trace_name_;
  uint64_t bytes_parsed_ = 0;

  // NotifyEndOfFile should only be called once. Set to true whenever it is
  // called.
  bool notify_eof_called_ = false;
};

<<<<<<< HEAD
}  // namespace perfetto::trace_processor
=======
}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

#endif  // SRC_TRACE_PROCESSOR_TRACE_PROCESSOR_IMPL_H_
