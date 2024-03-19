/*
 * Copyright (C) 2021 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_PRELUDE_TABLE_FUNCTIONS_FLAMEGRAPH_CONSTRUCTION_ALGORITHMS_H_
#define SRC_TRACE_PROCESSOR_PRELUDE_TABLE_FUNCTIONS_FLAMEGRAPH_CONSTRUCTION_ALGORITHMS_H_

#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {

// Represents a time boundary for a column.
struct TimeConstraints {
  FilterOp op;
  int64_t value;
};

std::unique_ptr<tables::ExperimentalFlamegraphNodesTable>
BuildHeapProfileFlamegraph(TraceStorage* storage,
                           UniquePid upid,
                           int64_t timestamp);

std::unique_ptr<tables::ExperimentalFlamegraphNodesTable>
BuildNativeCallStackSamplingFlamegraph(
    TraceStorage* storage,
    std::optional<UniquePid> upid,
    std::optional<std::string> upid_group,
    const std::vector<TimeConstraints>& time_constraints);
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_PRELUDE_TABLE_FUNCTIONS_FLAMEGRAPH_CONSTRUCTION_ALGORITHMS_H_
