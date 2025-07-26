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

#include "src/trace_processor/importers/common/args_tracker.h"

#include <algorithm>

#include "src/trace_processor/importers/common/args_translation_table.h"

namespace perfetto {
namespace trace_processor {

ArgsTracker::ArgsTracker(TraceProcessorContext* context) : context_(context) {}

ArgsTracker::~ArgsTracker() {
  Flush();
}

void ArgsTracker::AddArg(Column* arg_set_id,
                         uint32_t row,
                         StringId flat_key,
                         StringId key,
                         Variadic value,
                         UpdatePolicy update_policy) {
  args_.emplace_back();

  auto* rid_arg = &args_.back();
  rid_arg->column = arg_set_id;
  rid_arg->row = row;
  rid_arg->flat_key = flat_key;
  rid_arg->key = key;
  rid_arg->value = value;
  rid_arg->update_policy = update_policy;
}

void ArgsTracker::Flush() {
  using Arg = GlobalArgsTracker::Arg;

  if (args_.empty())
    return;

  // We sort here because a single packet may add multiple args with different
  // rowids.
  auto comparator = [](const Arg& f, const Arg& s) {
    // We only care that all args for a specific arg set appear in a contiguous
    // block and that args within one arg set are sorted by key, but not about
    // the relative order of one block to another. The simplest way to achieve
    // that is to sort by table column pointer & row, which identify the arg
    // set, and then by key.
    if (f.column == s.column && f.row == s.row)
      return f.key < s.key;
    if (f.column == s.column)
      return f.row < s.row;
    return f.column < s.column;
  };
  std::stable_sort(args_.begin(), args_.end(), comparator);

  for (uint32_t i = 0; i < args_.size();) {
    const GlobalArgsTracker::Arg& arg = args_[i];
    auto* col = arg.column;
    uint32_t row = arg.row;

    uint32_t next_rid_idx = i + 1;
    while (next_rid_idx < args_.size() && col == args_[next_rid_idx].column &&
           row == args_[next_rid_idx].row) {
      next_rid_idx++;
    }

    ArgSetId set_id =
        context_->global_args_tracker->AddArgSet(&args_[0], i, next_rid_idx);
    if (col->IsNullable()) {
      TypedColumn<std::optional<uint32_t>>::FromColumn(col)->Set(row, set_id);
    } else {
      TypedColumn<uint32_t>::FromColumn(col)->Set(row, set_id);
    }

    i = next_rid_idx;
  }
  args_.clear();
}

ArgsTracker::CompactArgSet ArgsTracker::ToCompactArgSet(
    const Column& column,
    uint32_t row_number) && {
  CompactArgSet compact_args;
  for (const auto& arg : args_) {
    PERFETTO_DCHECK(arg.column == &column);
    PERFETTO_DCHECK(arg.row == row_number);
    compact_args.emplace_back(arg.ToCompactArg());
  }
  args_.clear();
  return compact_args;
}

bool ArgsTracker::NeedsTranslation(const ArgsTranslationTable& table) const {
  return std::any_of(
      args_.begin(), args_.end(), [&table](const GlobalArgsTracker::Arg& arg) {
        return table.NeedsTranslation(arg.flat_key, arg.key, arg.value.type);
      });
}

ArgsTracker::BoundInserter::BoundInserter(ArgsTracker* args_tracker,
                                          Column* arg_set_id_column,
                                          uint32_t row)
    : args_tracker_(args_tracker),
      arg_set_id_column_(arg_set_id_column),
      row_(row) {}

ArgsTracker::BoundInserter::~BoundInserter() = default;

}  // namespace trace_processor
}  // namespace perfetto
