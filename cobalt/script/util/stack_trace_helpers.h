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

#ifndef COBALT_SCRIPT_UTIL_STACK_TRACE_HELPERS_H_
#define COBALT_SCRIPT_UTIL_STACK_TRACE_HELPERS_H_

#include <string>

#include "cobalt/script/stack_frame.h"
#include "nb/rewindable_vector.h"

namespace cobalt {
namespace script {
namespace util {

class StackTraceGenerator {
 public:
  virtual ~StackTraceGenerator() {}

  // Returns |true| if the current StackTraceGenerator can generate information
  // about the stack.
  virtual bool Valid() = 0;

  // Generates stack traces in the raw form. Returns true if any stack
  // frames were generated. False otherwise. Output vector will be
  // unconditionally rewound to being empty.
  virtual bool GenerateStackTrace(int depth,
                                  nb::RewindableVector<StackFrame>* out) = 0;

  // Returns true if any stack traces were written. The output vector will be
  // re-wound to being empty.
  // The first position is the most immediate stack frame.
  virtual bool GenerateStackTraceLines(
      int depth, nb::RewindableVector<std::string>* out) = 0;

  // Prints stack trace. Returns true on success.
  virtual bool GenerateStackTraceString(int depth, std::string* out) = 0;

  virtual bool GenerateStackTraceString(int depth, char* buff,
                                        size_t buff_size) = 0;
};

// Get's the thread local StackTraceGenerator.
// Defined in engine specific implementation.
StackTraceGenerator* GetThreadLocalStackTraceGenerator();

}  // namespace util
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_UTIL_STACK_TRACE_HELPERS_H_
