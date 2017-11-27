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

#ifndef COBALT_SCRIPT_MOZJS_45_UTIL_STACK_TRACE_HELPERS_H_
#define COBALT_SCRIPT_MOZJS_45_UTIL_STACK_TRACE_HELPERS_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/stack_frame.h"
#include "cobalt/script/util/stack_trace_helpers.h"
#include "nb/rewindable_vector.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

#if defined(STARBOARD_ALLOWS_MEMORY_TRACKING)
#define ENABLE_JS_STACK_TRACE_IN_SCOPE(context)                            \
  ::cobalt::script::mozjs::util::StackTraceScope stack_trace_scope_object( \
      context)
#else
#define ENABLE_JS_STACK_TRACE_IN_SCOPE(context)
#endif

namespace cobalt {
namespace script {
namespace mozjs {
namespace util {

// Usage:
//   void Function(JSContext* js_ctx, ...) {
//     ENABLE_JS_STACK_TRACE_IN_SCOPE(js_ctx);
//     ...
//     InvokeOtherFunctions();
//   }
//
//  void InvokeOtherFunctions() {
//    ...
//    std::string stack_trace_str;
//    if (GetThreadLocalStackTraceGenerator()->GenerateStackTraceString(
//          2, &stack_trace_str)) {
//      // Prints the stack trace from javascript.
//      SbLogRaw(stack_trace_str.c_str());
//    }
//  }
class MozjsStackTraceGenerator
    : public ::cobalt::script::util::StackTraceGenerator {
 public:
  MozjsStackTraceGenerator() : context_(NULL) {}

  // Returns |true| if the current StackTraceGenerator can generate information
  // about the stack.
  bool Valid() override { return context_ != NULL; }

  // Generates stack traces in the raw form. Returns true if any stack
  // frames were generated. False otherwise. Output vector will be
  // unconditionally rewound to being empty.
  bool GenerateStackTrace(int depth,
                          nb::RewindableVector<StackFrame>* out) override;

  // Returns true if any stack traces were written. The output vector will be
  // re-wound to being empty.
  // The first position is the most immediate stack frame.
  bool GenerateStackTraceLines(int depth,
                               nb::RewindableVector<std::string>* out) override;

  // Prints stack trace. Returns true on success.
  bool GenerateStackTraceString(int depth, std::string* out) override;

  bool GenerateStackTraceString(int depth, char* buff,
                                size_t buff_size) override;

  // Gets the internal data structure used to generate stack traces.
  JSContext* context() { return context_; }

  // Internal only, do not set.
  void set_context(JSContext* context) { context_ = context; }

 private:
  JSContext* context_;

  // Recycles memory so that stack tracing is efficient.
  struct Scratch {
    nb::RewindableVector<StackFrame> stack_frames_;
    nb::RewindableVector<std::string> strings_stack_frames_;
    std::string symbol_;
  };
  Scratch scratch_data_;
  // Checks that each instance can only be used within the same thread.
  base::ThreadChecker thread_checker_;
};

// This should only be accessed by a scoped object.
void SetThreadLocalJSContext(JSContext* context);
JSContext* GetThreadLocalJSContext();

// Useful for updating the most recent stack trace.
struct StackTraceScope {
  explicit StackTraceScope(JSContext* js)
      : prev_context_(GetThreadLocalJSContext()) {
    SetThreadLocalJSContext(js);
  }
  ~StackTraceScope() { SetThreadLocalJSContext(prev_context_); }
  JSContext* prev_context_;
};

}  // namespace util
}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_UTIL_STACK_TRACE_HELPERS_H_
