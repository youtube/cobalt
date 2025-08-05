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

#ifndef SRC_TRACE_PROCESSOR_PRELUDE_TABLE_FUNCTIONS_DESCENDANT_H_
#define SRC_TRACE_PROCESSOR_PRELUDE_TABLE_FUNCTIONS_DESCENDANT_H_

#include <optional>

#include "src/trace_processor/prelude/table_functions/table_function.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {
namespace tables {

#define PERFETTO_TP_DESCENDANT_SLICE_TABLE_DEF(NAME, PARENT, C) \
  NAME(DescendantSliceTable, "descendant_slice")                \
  PARENT(PERFETTO_TP_SLICE_TABLE_DEF, C)                        \
  C(uint32_t, start_id, Column::Flag::kHidden)

PERFETTO_TP_TABLE(PERFETTO_TP_DESCENDANT_SLICE_TABLE_DEF);

#define PERFETTO_TP_DESCENDANT_SLICE_BY_STACK_TABLE_DEF(NAME, PARENT, C) \
  NAME(DescendantSliceByStackTable, "descendant_slice_by_stack")         \
  PARENT(PERFETTO_TP_SLICE_TABLE_DEF, C)                                 \
  C(int64_t, start_stack_id, Column::Flag::kHidden)

PERFETTO_TP_TABLE(PERFETTO_TP_DESCENDANT_SLICE_BY_STACK_TABLE_DEF);

}  // namespace tables

class TraceProcessorContext;

// Implements the following dynamic tables:
// * descendant_slice
// * descendant_slice_by_stack
//
// See docs/analysis/trace-processor for usage.
class Descendant : public TableFunction {
 public:
  enum class Type { kSlice = 1, kSliceByStack = 2 };

  Descendant(Type type, const TraceStorage*);

  Table::Schema CreateSchema() override;
  std::string TableName() override;
  uint32_t EstimateRowCount() override;
  base::Status ValidateConstraints(const QueryConstraints&) override;
  base::Status ComputeTable(const std::vector<Constraint>& cs,
                            const std::vector<Order>& ob,
                            const BitVector& cols_used,
                            std::unique_ptr<Table>& table_return) override;

  // Returns a vector of slice rows which are descendants of |slice_id|. Returns
  // std::nullopt if an invalid |slice_id| is given. This is used by
  // ConnectedFlow to traverse flow indirectly connected flow events.
  static std::optional<std::vector<tables::SliceTable::RowNumber>>
  GetDescendantSlices(const tables::SliceTable& slices, SliceId slice_id);

 private:
  Type type_;
  const TraceStorage* storage_ = nullptr;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_PRELUDE_TABLE_FUNCTIONS_DESCENDANT_H_
