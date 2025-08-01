/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_JSON_JSON_TRACE_PARSER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_JSON_JSON_TRACE_PARSER_H_

#include <stdint.h>

#include <memory>
#include <tuple>

#include "src/trace_processor/importers/common/trace_parser.h"
#include "src/trace_processor/importers/systrace/systrace_line.h"
#include "src/trace_processor/importers/systrace/systrace_line_parser.h"

namespace Json {
class Value;
}

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

// Parses legacy chrome JSON traces. The support for now is extremely rough
// and supports only explicit TRACE_EVENT_BEGIN/END events.
class JsonTraceParser : public TraceParser {
 public:
  explicit JsonTraceParser(TraceProcessorContext*);
  ~JsonTraceParser() override;

  // TraceParser implementation.
  void ParseJsonPacket(int64_t timestamp, std::string string_value) override;
  void ParseSystraceLine(int64_t timestamp, SystraceLine line) override;

 private:
  TraceProcessorContext* const context_;
  SystraceLineParser systrace_line_parser_;

  void MaybeAddFlow(TrackId track_id, const Json::Value& event);
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_JSON_JSON_TRACE_PARSER_H_
