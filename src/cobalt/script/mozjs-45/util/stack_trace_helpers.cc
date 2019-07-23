// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/script/mozjs-45/util/stack_trace_helpers.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "cobalt/script/mozjs-45/util/exception_helpers.h"
#include "cobalt/script/stack_frame.h"
#include "nb/thread_local_object.h"
#include "starboard/common/string.h"
#include "starboard/memory.h"
#include "starboard/once.h"
#include "starboard/types.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {
namespace util {

namespace {

typedef nb::ThreadLocalObject<MozjsStackTraceGenerator>
    ThreadLocalJsStackTracer;

SB_ONCE_INITIALIZE_FUNCTION(ThreadLocalJsStackTracer,
                            s_thread_local_js_stack_tracer_singelton);

void ToStringAppend(const StackFrame& sf, std::string* out) {
  base::SStringPrintf(out, "%s(%d,%d):%s", sf.source_url.c_str(),
                      sf.line_number, sf.column_number,
                      sf.function_name.c_str());
}

}  // namespace

void SetThreadLocalJSContext(JSContext* context) {
  static_cast<MozjsStackTraceGenerator*>(
      ::cobalt::script::util::GetThreadLocalStackTraceGenerator())
      ->set_context(context);
}

JSContext* GetThreadLocalJSContext() {
  return static_cast<MozjsStackTraceGenerator*>(
             ::cobalt::script::util::GetThreadLocalStackTraceGenerator())
      ->context();
}

bool MozjsStackTraceGenerator::GenerateStackTrace(
    int depth, nb::RewindableVector<StackFrame>* out) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  out->rewindAll();
  if (!Valid()) {
    return false;
  }
  GetStackTraceUsingInternalApi(context_, depth, out);
  return !out->empty();
}

bool MozjsStackTraceGenerator::GenerateStackTraceLines(
    int depth, nb::RewindableVector<std::string>* out) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  out->rewindAll();
  nb::RewindableVector<StackFrame>& stack_frames = scratch_data_.stack_frames_;
  if (!GenerateStackTrace(depth, &stack_frames)) {
    return false;
  }

  for (size_t i = 0; i < stack_frames.size(); ++i) {
    std::string& current_string = out->grow(1);
    current_string.assign("");  // Should not deallocate memory.
    StackFrame& sf = stack_frames[i];
    ToStringAppend(sf, &current_string);
  }
  return true;
}

bool MozjsStackTraceGenerator::GenerateStackTraceString(int depth,
                                                        std::string* out) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  out->assign("");  // Should not deallocate memory.

  nb::RewindableVector<StackFrame>& stack_frames = scratch_data_.stack_frames_;
  if (!GenerateStackTrace(depth, &stack_frames)) {
    return false;
  }

  for (size_t i = 0; i < stack_frames.size(); ++i) {
    cobalt::script::StackFrame& sf = stack_frames[i];
    ToStringAppend(sf, out);
    if (i < stack_frames.size() - 1) {
      base::SStringPrintf(out, "\n");
    }
  }
  return true;
}

bool MozjsStackTraceGenerator::GenerateStackTraceString(int depth, char* buff,
                                                        size_t buff_size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SbMemorySet(buff, 0, buff_size);
  std::string& scratch_symbol = scratch_data_.symbol_;

  if (!GenerateStackTraceString(depth, &scratch_symbol)) {
    return false;
  }

  SbStringCopy(buff, scratch_symbol.c_str(), buff_size);
  return true;
}

}  // namespace util
}  // namespace mozjs

namespace util {

// Declared in abstract cobalt script.
StackTraceGenerator* GetThreadLocalStackTraceGenerator() {
  return mozjs::util::s_thread_local_js_stack_tracer_singelton()->GetOrCreate();
}

}  // namespace util

}  // namespace script
}  // namespace cobalt
