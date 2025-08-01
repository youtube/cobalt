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

#ifndef SRC_TRACE_PROCESSOR_PRELUDE_TABLE_FUNCTIONS_EXPERIMENTAL_FLAMEGRAPH_H_
#define SRC_TRACE_PROCESSOR_PRELUDE_TABLE_FUNCTIONS_EXPERIMENTAL_FLAMEGRAPH_H_

#include "src/trace_processor/prelude/table_functions/flamegraph_construction_algorithms.h"
#include "src/trace_processor/prelude/table_functions/table_function.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

class ExperimentalFlamegraph : public TableFunction {
 public:
  enum class ProfileType { kGraph, kHeapProfile, kPerf };

  struct InputValues {
    ProfileType profile_type;
    int64_t ts;
    std::vector<TimeConstraints> time_constraints;
    std::optional<UniquePid> upid;
    std::optional<std::string> upid_group;
    std::string focus_str;
  };

  explicit ExperimentalFlamegraph(TraceProcessorContext* context);
  virtual ~ExperimentalFlamegraph() override;

  Table::Schema CreateSchema() override;
  std::string TableName() override;
  uint32_t EstimateRowCount() override;
  base::Status ValidateConstraints(const QueryConstraints&) override;
  base::Status ComputeTable(const std::vector<Constraint>& cs,
                            const std::vector<Order>& ob,
                            const BitVector& cols_used,
                            std::unique_ptr<Table>& table_return) override;

 private:
  TraceProcessorContext* context_ = nullptr;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_PRELUDE_TABLE_FUNCTIONS_EXPERIMENTAL_FLAMEGRAPH_H_
