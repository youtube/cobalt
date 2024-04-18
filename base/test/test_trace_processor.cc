// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_trace_processor.h"
#include <sstream>
#include "third_party/perfetto/include/perfetto/trace_processor/trace_processor.h"

namespace base::test {

TestTraceProcessor::TestTraceProcessor() {
  config_ = std::make_unique<perfetto::trace_processor::Config>();
  trace_processor_ =
      perfetto::trace_processor::TraceProcessor::CreateInstance(*config_);
}

TestTraceProcessor::~TestTraceProcessor() = default;

std::vector<std::vector<std::string>> TestTraceProcessor::ExecuteQuery(
    const std::string& sql) {
  std::vector<std::vector<std::string>> result;
  auto it = trace_processor_->ExecuteQuery(sql);
  // Write column names.
  std::vector<std::string> column_names;
  for (uint32_t c = 0; c < it.ColumnCount(); ++c) {
    column_names.push_back(it.GetColumnName(c));
  }
  result.push_back(column_names);
  // Write rows.
  while (it.Next()) {
    std::vector<std::string> row;
    for (uint32_t c = 0; c < it.ColumnCount(); ++c) {
      perfetto::trace_processor::SqlValue sql_value = it.Get(c);
      std::ostringstream ss;
      switch (sql_value.type) {
        case perfetto::trace_processor::SqlValue::Type::kLong:
          ss << sql_value.AsLong();
          row.push_back(ss.str());
          break;
        case perfetto::trace_processor::SqlValue::Type::kDouble:
          ss << sql_value.AsDouble();
          row.push_back(ss.str());
          break;
        case perfetto::trace_processor::SqlValue::Type::kString:
          row.push_back(sql_value.AsString());
          break;
        case perfetto::trace_processor::SqlValue::Type::kBytes:
          row.push_back("<raw bytes>");
          break;
        case perfetto::trace_processor::SqlValue::Type::kNull:
          row.push_back("[NULL]");
          break;
        default:
          row.push_back("unknown");
      }
    }
    result.push_back(row);
  }
  return result;
}

absl::Status TestTraceProcessor::ParseTrace(std::unique_ptr<uint8_t[]> buf,
                                            size_t size) {
  auto status =
      trace_processor_->Parse(perfetto::trace_processor::TraceBlobView(
          perfetto::trace_processor::TraceBlob::TakeOwnership(std::move(buf),
                                                              size)));
  // TODO(rasikan): Add DCHECK that the trace is well-formed and parsing doesn't
  // have any errors (e.g. to catch the cases when someone emits overlapping
  // trace events on the same track).
  trace_processor_->NotifyEndOfFile();
  return status.ok() ? absl::OkStatus() : absl::UnknownError(status.message());
}

absl::Status TestTraceProcessor::ParseTrace(
    const std::vector<char>& raw_trace) {
  auto size = raw_trace.size();
  std::unique_ptr<uint8_t[]> data_copy(new uint8_t[size]);
  std::copy(raw_trace.begin(), raw_trace.end(), data_copy.get());
  return ParseTrace(std::move(data_copy), size);
}

}  // namespace base::test
