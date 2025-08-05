/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "src/trace_processor/prelude/table_functions/connected_flow.h"

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

TEST(ConnectedFlow, SliceTableNullConstraint) {
  // Insert a row to make sure that we are not returning an empty table just
  // because the source is empty.
  TraceStorage storage;
  storage.mutable_slice_table()->Insert({});

  ConnectedFlow generator{ConnectedFlow::Mode::kDirectlyConnectedFlow,
                          &storage};

  // Check that if we pass start_id = NULL as a constraint, we correctly return
  // an empty table.
  std::unique_ptr<Table> res;
  Constraint c{tables::ConnectedFlowTable::ColumnIndex::start_id, FilterOp::kEq,
               SqlValue()};
  base::Status status = generator.ComputeTable({c}, {}, BitVector(), res);
  ASSERT_TRUE(status.ok());
  ASSERT_EQ(res->row_count(), 0u);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
