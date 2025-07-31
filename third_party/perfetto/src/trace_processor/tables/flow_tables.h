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

#ifndef SRC_TRACE_PROCESSOR_TABLES_FLOW_TABLES_H_
#define SRC_TRACE_PROCESSOR_TABLES_FLOW_TABLES_H_

#include "src/trace_processor/tables/macros.h"
#include "src/trace_processor/tables/slice_tables.h"

namespace perfetto {
namespace trace_processor {
namespace tables {

// @param arg_set_id {@joinable args.arg_set_id}
#define PERFETTO_TP_FLOW_TABLE_DEF(NAME, PARENT, C) \
  NAME(FlowTable, "flow")                           \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                 \
  C(SliceTable::Id, slice_out)                      \
  C(SliceTable::Id, slice_in)                       \
  C(uint32_t, arg_set_id)

}  // namespace tables
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_FLOW_TABLES_H_
