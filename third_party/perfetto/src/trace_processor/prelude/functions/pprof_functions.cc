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

#include "src/trace_processor/prelude/functions/pprof_functions.h"

#include <stdlib.h>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/base/status.h"
#include "perfetto/ext/base/status_or.h"
#include "perfetto/protozero/packed_repeated_fields.h"
#include "perfetto/trace_processor/basic_types.h"
#include "perfetto/trace_processor/status.h"
#include "protos/perfetto/trace_processor/stack.pbzero.h"
#include "src/trace_processor/sqlite/sqlite_utils.h"
#include "src/trace_processor/types/trace_processor_context.h"
#include "src/trace_processor/util/profile_builder.h"
#include "src/trace_processor/util/status_macros.h"

// TODO(carlscab): We currently recreate the GProfileBuilder for every group. We
// should cache this somewhere maybe even have a helper table that stores all
// this data.

namespace perfetto {
namespace trace_processor {
namespace {

using protos::pbzero::Stack;

constexpr char kFunctionName[] = "EXPERIMENTAL_PROFILE";

template <typename T>
std::unique_ptr<T> WrapUnique(T* ptr) {
  return std::unique_ptr<T>(ptr);
}

class AggregateContext {
 public:
  static base::StatusOr<std::unique_ptr<AggregateContext>>
  Create(TraceProcessorContext* tp_context, size_t argc, sqlite3_value** argv) {
    base::StatusOr<std::vector<GProfileBuilder::ValueType>> sample_types =
        GetSampleTypes(argc, argv);
    if (!sample_types.ok()) {
      return sample_types.status();
    }
    return WrapUnique(new AggregateContext(tp_context, sample_types.value()));
  }

  base::Status Step(size_t argc, sqlite3_value** argv) {
    RETURN_IF_ERROR(UpdateSampleValue(argc, argv));

    base::StatusOr<SqlValue> value =
        sqlite_utils::ExtractArgument(argc, argv, "stack", 0, SqlValue::kBytes);
    if (!value.ok()) {
      return value.status();
    }

    Stack::Decoder stack(static_cast<const uint8_t*>(value->bytes_value),
                         value->bytes_count);
    if (stack.bytes_left() != 0) {
      return sqlite_utils::ToInvalidArgumentError(
          "stack", 0, base::ErrStatus("failed to deserialize Stack proto"));
    }
    if (!builder_.AddSample(stack, sample_value_)) {
      return base::ErrStatus("Failed to add callstack");
    }
    return util::OkStatus();
  }

  void Final(sqlite3_context* ctx) {
    // TODO(carlscab): A lot of copies are happening here.
    std::string profile_proto = builder_.Build();

    std::unique_ptr<uint8_t[], base::FreeDeleter> data(
        static_cast<uint8_t*>(malloc(profile_proto.size())));
    memcpy(data.get(), profile_proto.data(), profile_proto.size());
    sqlite3_result_blob(ctx, data.release(),
                        static_cast<int>(profile_proto.size()), free);
  }

 private:
  static base::StatusOr<std::vector<GProfileBuilder::ValueType>> GetSampleTypes(
      size_t argc,
      sqlite3_value** argv) {
    std::vector<GProfileBuilder::ValueType> sample_types;

    if (argc == 1) {
      sample_types.push_back({"samples", "count"});
    }

    for (size_t i = 1; i < argc; i += 3) {
      base::StatusOr<SqlValue> type = sqlite_utils::ExtractArgument(
          argc, argv, "sample_type", i, SqlValue::kString);
      if (!type.ok()) {
        return type.status();
      }

      base::StatusOr<SqlValue> units = sqlite_utils::ExtractArgument(
          argc, argv, "sample_units", i + 1, SqlValue::kString);
      if (!units.ok()) {
        return units.status();
      }

      sample_types.push_back({type->AsString(), units->AsString()});
    }
    return std::move(sample_types);
  }

  AggregateContext(TraceProcessorContext* tp_context,
                   const std::vector<GProfileBuilder::ValueType>& sample_types)
      : builder_(tp_context, sample_types) {
    sample_value_.Append(1);
  }

  base::Status UpdateSampleValue(size_t argc, sqlite3_value** argv) {
    if (argc == 1) {
      return base::OkStatus();
    }

    sample_value_.Reset();
    for (size_t i = 3; i < argc; i += 3) {
      base::StatusOr<SqlValue> value = sqlite_utils::ExtractArgument(
          argc, argv, "sample_value", i, SqlValue::kLong);
      if (!value.ok()) {
        return value.status();
      }
      sample_value_.Append(value->AsLong());
    }

    return base::OkStatus();
  }

  GProfileBuilder builder_;
  protozero::PackedVarInt sample_value_;
};

static base::Status Step(sqlite3_context* ctx,
                         size_t argc,
                         sqlite3_value** argv) {
  AggregateContext** agg_context_ptr = static_cast<AggregateContext**>(
      sqlite3_aggregate_context(ctx, sizeof(AggregateContext*)));
  if (!agg_context_ptr) {
    return base::ErrStatus("Failed to allocate aggregate context");
  }

  if (!*agg_context_ptr) {
    TraceProcessorContext* tp_context =
        static_cast<TraceProcessorContext*>(sqlite3_user_data(ctx));
    base::StatusOr<std::unique_ptr<AggregateContext>> agg_context =
        AggregateContext::Create(tp_context, argc, argv);
    if (!agg_context.ok()) {
      return agg_context.status();
    }

    *agg_context_ptr = agg_context->release();
  }

  return (*agg_context_ptr)->Step(argc, argv);
}

static void StepWrapper(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
  PERFETTO_CHECK(argc >= 0);

  base::Status status = Step(ctx, static_cast<size_t>(argc), argv);

  if (!status.ok()) {
    sqlite_utils::SetSqliteError(ctx, kFunctionName, status);
  }
}

static void FinalWrapper(sqlite3_context* ctx) {
  AggregateContext** agg_context_ptr =
      static_cast<AggregateContext**>(sqlite3_aggregate_context(ctx, 0));

  if (!agg_context_ptr) {
    return;
  }

  (*agg_context_ptr)->Final(ctx);

  delete (*agg_context_ptr);
}

}  // namespace

base::Status PprofFunctions::Register(sqlite3* db,
                                      TraceProcessorContext* context) {
  int flags = SQLITE_UTF8 | SQLITE_DETERMINISTIC;
  int ret =
      sqlite3_create_function_v2(db, kFunctionName, -1, flags, context, nullptr,
                                 StepWrapper, FinalWrapper, nullptr);
  if (ret != SQLITE_OK) {
    return base::ErrStatus("Unable to register function with name %s",
                           kFunctionName);
  }
  return base::OkStatus();
}

}  // namespace trace_processor
}  // namespace perfetto
