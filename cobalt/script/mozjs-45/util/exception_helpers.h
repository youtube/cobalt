// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
#ifndef COBALT_SCRIPT_MOZJS_45_UTIL_EXCEPTION_HELPERS_H_
#define COBALT_SCRIPT_MOZJS_45_UTIL_EXCEPTION_HELPERS_H_

#include <string>
#include <vector>

#include "cobalt/script/stack_frame.h"
#include "nb/rewindable_vector.h"
#include "third_party/mozjs-45/js/public/UbiNode.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {
namespace util {
std::string GetExceptionString(JSContext* context);

std::string GetExceptionString(JSContext* context, JS::HandleValue exception);

// Retrieves the current stack frame. Values are stored in the output vector.
// RewindableVector<> will be unconditionally rewound and after this call will
// contain the number of frames retrieved. The output size will be less than
// or equal to max_frames.
void GetStackTrace(JSContext* context, size_t max_frames,
                   nb::RewindableVector<StackFrame>* output);

// The same as |GetStackTrace|, only using internal SpiderMonkey APIs instead.
// This may be required if attempting to obtain a call stack while SpiderMonkey
// is deep inside its internals, such as during an allocation.  If you decide to
// call this, make sure you know what you're doing.
void GetStackTraceUsingInternalApi(JSContext* context, size_t max_frames,
                                   nb::RewindableVector<StackFrame>* output);

}  // namespace util
}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_UTIL_EXCEPTION_HELPERS_H_
