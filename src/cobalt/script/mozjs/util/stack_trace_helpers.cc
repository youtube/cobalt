// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef NB_SCRIPT_STACKTRACE_H_
#define NB_SCRIPT_STACKTRACE_H_

#include "cobalt/script/mozjs/util/stack_trace_helpers.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include "cobalt/script/mozjs/util/exception_helpers.h"
#include "cobalt/script/stack_frame.h"
#include "nb/thread_local_object.h"
#include "starboard/once.h"
#include "starboard/types.h"
#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {
namespace util {
namespace {

typedef nb::ThreadLocalObject<StackTraceGenerator> ThreadLocalJsStackTracer;

SB_ONCE_INITIALIZE_FUNCTION(ThreadLocalJsStackTracer,
                            s_thread_local_js_stack_tracer_singelton);

}  // namespace.

void SetThreadLocalJSContext(JSContext* context) {
  GetThreadLocalStackTraceGenerator()->set_js_context(context);
}

JSContext* GetThreadLocalJSContext() {
  return GetThreadLocalStackTraceGenerator()->js_context();
}

StackTraceGenerator* GetThreadLocalStackTraceGenerator() {
  return s_thread_local_js_stack_tracer_singelton()->GetOrCreate();
}

//////////////////////////////////// IMPL /////////////////////////////////////

StackTraceGenerator::StackTraceGenerator() : js_context_(NULL) {}
StackTraceGenerator::~StackTraceGenerator() {}

bool StackTraceGenerator::Valid() { return js_context_ != NULL; }

size_t StackTraceGenerator::GenerateStackTrace(int depth,
                                               std::vector<StackFrame>* out) {
  if (!Valid()) {
    return 0;
  }

  *out = GetStackTrace(js_context_, depth);
  return out->size();
}

size_t StackTraceGenerator::GenerateStackTraceLines(
    int depth, std::vector<std::string>* out) {
  if (!Valid()) {
    return 0;
  }

  std::vector<StackFrame> stack_frame = GetStackTrace(js_context_, depth);
  const size_t n = stack_frame.size();

  if (n > out->size()) {
    out->resize(n);
  }

  for (size_t i = 0; i < n; ++i) {
    std::stringstream ss;

    StackFrame& sf = stack_frame[i];
    ss << sf.source_url << "(" << sf.line_number << "," << sf.column_number
       << "):" << sf.function_name;
    // Write the string out.
    (*out)[i] = ss.str();
  }
  return n;
}

bool StackTraceGenerator::GenerateStackTraceString(int depth,
                                                   std::string* out) {
  if (!Valid()) {
    return 0;
  }

  std::vector<StackFrame> stack_frame = GetStackTrace(js_context_, depth);
  const size_t n = stack_frame.size();

  if (n == 0) {
    return false;
  }

  std::stringstream ss;
  for (int i = static_cast<int>(n - 1); i >= 0; --i) {
    cobalt::script::StackFrame& sf = stack_frame[static_cast<size_t>(i)];
    for (size_t j = 0; j < i; ++j) {
      ss << "  ";
    }
    ss << sf.source_url << "(" << sf.line_number << "," << sf.column_number
       << "):" << sf.function_name << "\n";
  }
  *out = ss.str();
  return true;
}

JSContext* StackTraceGenerator::js_context() { return js_context_; }

void StackTraceGenerator::set_js_context(JSContext* js_ctx) {
  js_context_ = js_ctx;
}

}  // namespace util
}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // NB_SCRIPT_STACKTRACE_H_
