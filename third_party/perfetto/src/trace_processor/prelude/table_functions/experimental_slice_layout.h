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

#ifndef SRC_TRACE_PROCESSOR_PRELUDE_TABLE_FUNCTIONS_EXPERIMENTAL_SLICE_LAYOUT_H_
#define SRC_TRACE_PROCESSOR_PRELUDE_TABLE_FUNCTIONS_EXPERIMENTAL_SLICE_LAYOUT_H_

#include <set>

#include "src/trace_processor/prelude/table_functions/table_function.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {

namespace tables {

#define PERFETTO_TP_SLICE_LAYOUT_TABLE_DEF(NAME, PARENT, C)       \
  NAME(ExperimentalSliceLayoutTable, "experimental_slice_layout") \
  PARENT(PERFETTO_TP_SLICE_TABLE_DEF, C)                          \
  C(uint32_t, layout_depth)                                       \
  C(StringPool::Id, filter_track_ids, Column::kHidden)

PERFETTO_TP_TABLE(PERFETTO_TP_SLICE_LAYOUT_TABLE_DEF);

}  // namespace tables

class ExperimentalSliceLayout : public TableFunction {
 public:
  ExperimentalSliceLayout(StringPool* string_pool,
                          const tables::SliceTable* table);
  virtual ~ExperimentalSliceLayout() override;

  Table::Schema CreateSchema() override;
  std::string TableName() override;
  uint32_t EstimateRowCount() override;
  base::Status ValidateConstraints(const QueryConstraints&) override;
  base::Status ComputeTable(const std::vector<Constraint>& cs,
                            const std::vector<Order>& ob,
                            const BitVector& cols_used,
                            std::unique_ptr<Table>& table_return) override;

 private:
  std::unique_ptr<Table> ComputeLayoutTable(
      std::vector<tables::SliceTable::RowNumber> rows,
      StringPool::Id filter_id);
  tables::SliceTable::Id InsertSlice(
      std::map<tables::SliceTable::Id, tables::SliceTable::Id>& id_map,
      tables::SliceTable::Id id,
      std::optional<tables::SliceTable::Id> parent_id);

  // TODO(lalitm): remove this cache and move to having explicitly scoped
  // lifetimes of dynamic tables.
  std::unordered_map<StringId, std::unique_ptr<Table>> layout_table_cache_;

  StringPool* string_pool_;
  const tables::SliceTable* slice_table_;
  const StringPool::Id empty_string_id_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_PRELUDE_TABLE_FUNCTIONS_EXPERIMENTAL_SLICE_LAYOUT_H_
