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

#include "src/trace_processor/prelude/table_functions/experimental_annotated_stack.h"

#include <optional>

#include "perfetto/ext/base/string_utils.h"
#include "src/trace_processor/sqlite/sqlite_utils.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/tables/profiler_tables.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace perfetto {
namespace trace_processor {
namespace tables {

#define PERFETTO_TP_ANNOTATED_CALLSTACK_TABLE_DEF(NAME, PARENT, C) \
  NAME(ExperimentalAnnotatedCallstackTable,                        \
       "experimental_annotated_callstack")                         \
  PARENT(PERFETTO_TP_STACK_PROFILE_CALLSITE_DEF, C)                \
  C(StringId, annotation)                                          \
  C(tables::StackProfileCallsiteTable::Id, start_id, Column::Flag::kHidden)

PERFETTO_TP_TABLE(PERFETTO_TP_ANNOTATED_CALLSTACK_TABLE_DEF);

ExperimentalAnnotatedCallstackTable::~ExperimentalAnnotatedCallstackTable() =
    default;

}  // namespace tables

namespace {

enum class MapType {
  kArtInterp,
  kArtJit,
  kArtAot,
  kNativeLibart,
  kNativeOther,
  kOther
};

// Mapping examples:
//   /system/lib64/libc.so
//   /system/framework/framework.jar
//   /memfd:jit-cache (deleted)
//   /data/dalvik-cache/arm64/<snip>.apk@classes.dex
//   /data/app/<snip>/base.apk!libmonochrome_64.so
//   [vdso]
// TODO(rsavitski): consider moving this to a hidden column on
// stack_profile_mapping.
MapType ClassifyMap(NullTermStringView map) {
  if (map.empty())
    return MapType::kOther;

  // Primary mapping where modern ART puts jitted code.
  // The Zygote's JIT region is inherited by all descendant apps, so it can
  // still appear in their callstacks.
  if (map.StartsWith("/memfd:jit-cache") ||
      map.StartsWith("/memfd:jit-zygote-cache")) {
    return MapType::kArtJit;
  }

  size_t last_slash_pos = map.rfind('/');
  if (last_slash_pos != NullTermStringView::npos) {
    base::StringView suffix = map.substr(last_slash_pos);
    if (suffix.StartsWith("/libart.so") || suffix.StartsWith("/libartd.so"))
      return MapType::kNativeLibart;
  }
  size_t extension_pos = map.rfind('.');
  if (extension_pos != NullTermStringView::npos) {
    base::StringView suffix = map.substr(extension_pos);
    if (suffix.StartsWith(".so"))
      return MapType::kNativeOther;
    // unqualified dex
    if (suffix.StartsWith(".dex"))
      return MapType::kArtInterp;
    // dex with verification speedup info, produced by dex2oat
    if (suffix.StartsWith(".vdex"))
      return MapType::kArtInterp;
    // possibly uncompressed dex in a jar archive
    if (suffix.StartsWith(".jar"))
      return MapType::kArtInterp;
    // android package (zip file), this can contain uncompressed dexes or
    // native libraries that are mmap'd directly into the process. We rely on
    // libunwindstack's MapInfo::GetFullName, which suffixes the mapping with
    // "!lib.so" if it knows that the referenced piece of the archive is an
    // uncompressed ELF file. So an unadorned ".apk" is assumed to be a dex
    // file.
    if (suffix.StartsWith(".apk"))
      return MapType::kArtInterp;
    // ahead of time compiled ELFs
    if (suffix.StartsWith(".oat"))
      return MapType::kArtAot;
    // older/alternative name for .oat
    if (suffix.StartsWith(".odex"))
      return MapType::kArtAot;
  }
  return MapType::kOther;
}

}  // namespace

std::string ExperimentalAnnotatedStack::TableName() {
  return tables::ExperimentalAnnotatedCallstackTable::Name();
}

Table::Schema ExperimentalAnnotatedStack::CreateSchema() {
  return tables::ExperimentalAnnotatedCallstackTable::ComputeStaticSchema();
}

base::Status ExperimentalAnnotatedStack::ValidateConstraints(
    const QueryConstraints& qc) {
  const auto& cs = qc.constraints();
  int column = static_cast<int>(
      tables::ExperimentalAnnotatedCallstackTable::ColumnIndex::start_id);

  auto id_fn = [column](const QueryConstraints::Constraint& c) {
    return c.column == column && sqlite_utils::IsOpEq(c.op);
  };
  bool has_id_cs = std::find_if(cs.begin(), cs.end(), id_fn) != cs.end();
  return has_id_cs ? base::OkStatus()
                   : base::ErrStatus("Failed to find required constraints");
}

// TODO(carlscab): Replace annotation logic with
// src/trace_processor/util/annotated_callsites.h
base::Status ExperimentalAnnotatedStack::ComputeTable(
    const std::vector<Constraint>& cs,
    const std::vector<Order>&,
    const BitVector&,
    std::unique_ptr<Table>& table_return) {
  using CallsiteTable = tables::StackProfileCallsiteTable;

  const auto& cs_table = context_->storage->stack_profile_callsite_table();
  const auto& f_table = context_->storage->stack_profile_frame_table();
  const auto& m_table = context_->storage->stack_profile_mapping_table();

  // Input (id of the callsite leaf) is the constraint on the hidden |start_id|
  // column.
  using ColumnIndex = tables::ExperimentalAnnotatedCallstackTable::ColumnIndex;
  auto constraint_it =
      std::find_if(cs.begin(), cs.end(), [](const Constraint& c) {
        return c.col_idx == ColumnIndex::start_id && c.op == FilterOp::kEq;
      });
  PERFETTO_DCHECK(constraint_it != cs.end());
  if (constraint_it == cs.end() ||
      constraint_it->value.type != SqlValue::Type::kLong) {
    return base::ErrStatus("invalid input callsite id");
  }

  CallsiteId start_id =
      CallsiteId(static_cast<uint32_t>(constraint_it->value.AsLong()));
  auto opt_start_ref = cs_table.FindById(start_id);
  if (!opt_start_ref) {
    return base::ErrStatus("callsite with id %" PRIu32 " not found",
                           start_id.value);
  }

  // Iteratively walk the parent_id chain to construct the list of callstack
  // entries, each pointing at a frame.
  std::vector<CallsiteTable::RowNumber> cs_rows;
  cs_rows.push_back(opt_start_ref->ToRowNumber());
  std::optional<CallsiteId> maybe_parent_id = opt_start_ref->parent_id();
  while (maybe_parent_id) {
    auto parent_ref = *cs_table.FindById(*maybe_parent_id);
    cs_rows.push_back(parent_ref.ToRowNumber());
    maybe_parent_id = parent_ref.parent_id();
  }

  // Walk the callsites root-to-leaf, annotating:
  // * managed frames with their execution state (interpreted/jit/aot)
  // * common ART frames, which are usually not relevant to
  //   visualisation/inspection
  //
  // This is not a per-frame decision, because we do not want to filter out ART
  // frames immediately after a JNI transition (such frames are often relevant).
  //
  // As a consequence of the logic being based on a root-to-leaf walk, a given
  // callsite will always have the same annotation, as the parent path is always
  // the same, and children callsites do not affect their parents' annotations.
  StringId art_jni_trampoline =
      context_->storage->InternString("art_jni_trampoline");

  StringId common_frame = context_->storage->InternString("common-frame");
  StringId common_frame_interp =
      context_->storage->InternString("common-frame-interp");
  StringId art_interp = context_->storage->InternString("interp");
  StringId art_jit = context_->storage->InternString("jit");
  StringId art_aot = context_->storage->InternString("aot");

  // Annotation FSM states:
  // * kInitial: default, native-only callstacks never leave this state.
  // * kEraseLibart: we've seen a managed frame, and will now "erase" (i.e. tag
  //                 as a common-frame) frames belonging to the ART runtime.
  // * kKeepNext: we've seen a special JNI trampoline for managed->native
  //              transition, keep the immediate child (even if it is in ART),
  //              and then go back to kEraseLibart.
  // Regardless of the state, managed frames get annotated with their execution
  // mode, based on the mapping.
  enum class State { kInitial, kEraseLibart, kKeepNext };
  State annotation_state = State::kInitial;

  std::vector<StringPool::Id> annotations_reversed;
  for (auto it = cs_rows.rbegin(); it != cs_rows.rend(); ++it) {
    auto cs_ref = it->ToRowReference(cs_table);
    auto frame_ref = *f_table.FindById(cs_ref.frame_id());
    auto map_ref = *m_table.FindById(frame_ref.mapping());

    // Keep immediate callee of a JNI trampoline, but keep tagging all
    // successive libart frames as common.
    if (annotation_state == State::kKeepNext) {
      annotations_reversed.push_back(kNullStringId);
      annotation_state = State::kEraseLibart;
      continue;
    }

    // Special-case "art_jni_trampoline" frames, keeping their immediate callee
    // even if it is in libart, as it could be a native implementation of a
    // managed method. Example for "java.lang.reflect.Method.Invoke":
    //   art_jni_trampoline
    //   art::Method_invoke(_JNIEnv*, _jobject*, _jobject*, _jobjectArray*)
    //
    // Simpleperf also relies on this frame name, so it should be fairly stable.
    // TODO(rsavitski): consider detecting standard JNI upcall entrypoints -
    // _JNIEnv::Call*. These are sometimes inlined into other DSOs, so erasing
    // only the libart frames does not clean up all of the JNI-related frames.
    StringId fname_id = frame_ref.name();
    if (fname_id == art_jni_trampoline) {
      annotations_reversed.push_back(common_frame);
      annotation_state = State::kKeepNext;
      continue;
    }

    NullTermStringView map_view = context_->storage->GetString(map_ref.name());
    MapType map_type = ClassifyMap(map_view);

    // Annotate managed frames.
    if (map_type == MapType::kArtInterp ||  //
        map_type == MapType::kArtJit ||     //
        map_type == MapType::kArtAot) {
      if (map_type == MapType::kArtInterp)
        annotations_reversed.push_back(art_interp);
      else if (map_type == MapType::kArtJit)
        annotations_reversed.push_back(art_jit);
      else if (map_type == MapType::kArtAot)
        annotations_reversed.push_back(art_aot);

      // Now know to be in a managed callstack - erase subsequent ART frames.
      if (annotation_state == State::kInitial)
        annotation_state = State::kEraseLibart;
      continue;
    }

    // Mixed callstack, tag libart frames as uninteresting (common-frame).
    // Special case a subset of interpreter implementation frames as
    // "common-frame-interp" using frame name prefixes. Those functions are
    // actually executed, whereas the managed "interp" frames are synthesised as
    // their caller by the unwinding library (based on the dex_pc virtual
    // register restored using the libart's DWARF info). The heuristic covers
    // the "nterp" and "switch" interpreter implementations.
    //
    // Example:
    //  <towards root>
    //  android.view.WindowLayout.computeFrames [interp]
    //  nterp_op_iget_object_slow_path [common-frame-interp]
    //
    // This annotation is helpful when trying to answer "what mode was the
    // process in?" based on the leaf frame of the callstack. As we want to
    // classify such cases as interpreted, even though the leaf frame is
    // libart.so.
    //
    // For "switch" interpreter, we match any frame starting with
    // "art::interpreter::" according to itanium mangling.
    if (annotation_state == State::kEraseLibart &&
        map_type == MapType::kNativeLibart) {
      NullTermStringView fname = context_->storage->GetString(fname_id);
      if (fname.StartsWith("nterp_") || fname.StartsWith("Nterp") ||
          fname.StartsWith("ExecuteNterp") ||
          fname.StartsWith("ExecuteSwitchImpl") ||
          fname.StartsWith("_ZN3art11interpreter")) {
        annotations_reversed.push_back(common_frame_interp);
        continue;
      }
      annotations_reversed.push_back(common_frame);
      continue;
    }

    // default - no special annotation
    annotations_reversed.push_back(kNullStringId);
  }

  // Build the dynamic table.
  PERFETTO_DCHECK(cs_rows.size() == annotations_reversed.size());
  ColumnStorage<StringPool::Id> annotation_vals;
  for (auto it = annotations_reversed.rbegin();
       it != annotations_reversed.rend(); ++it) {
    annotation_vals.Append(*it);
  }

  // Hidden column - always the input, i.e. the callsite leaf.
  ColumnStorage<uint32_t> start_id_vals;
  for (uint32_t i = 0; i < cs_rows.size(); i++)
    start_id_vals.Append(start_id.value);

  table_return =
      tables::ExperimentalAnnotatedCallstackTable::SelectAndExtendParent(
          cs_table, std::move(cs_rows), std::move(annotation_vals),
          std::move(start_id_vals));
  return base::OkStatus();
}

uint32_t ExperimentalAnnotatedStack::EstimateRowCount() {
  return 1;
}

}  // namespace trace_processor
}  // namespace perfetto
