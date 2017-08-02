// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/script/mozjs/util/exception_helpers.h"

#include <algorithm>

#include "cobalt/script/mozjs/conversion_helpers.h"
#include "cobalt/script/mozjs/mozjs_exception_state.h"
#include "third_party/mozjs/js/public/RootingAPI.h"
#include "third_party/mozjs/js/src/jsapi.h"
#include "third_party/mozjs/js/src/jsdbgapi.h"
#include "third_party/mozjs/js/src/jsscript.h"

namespace cobalt {
namespace script {
namespace mozjs {
namespace util {

std::string GetExceptionString(JSContext* context) {
  if (!JS_IsExceptionPending(context)) {
    return std::string("No exception pending.");
  }
  JS::RootedValue exception(context);
  JS_GetPendingException(context, exception.address());
  JS_ReportPendingException(context);
  return GetExceptionString(context, exception);
}

std::string GetExceptionString(JSContext* context,
                               JS::HandleValue exception) {
  std::string exception_string;
  MozjsExceptionState exception_state(context);
  FromJSValue(context, exception, kNoConversionFlags, &exception_state,
              &exception_string);
  return exception_string;
}

void GetStackTrace(JSContext* context, size_t max_frames,
                   nb::RewindableVector<StackFrame>* output) {
  DCHECK(output);
  output->rewindAll();
  JSAutoRequest auto_request(context);
  JS::StackDescription* stack_description =
      JS::DescribeStack(context, max_frames);
  if (max_frames == 0) {
    max_frames = static_cast<size_t>(stack_description->nframes);
  } else {
    max_frames =
        std::min(max_frames, static_cast<size_t>(stack_description->nframes));
  }
  JS::FrameDescription* stack_trace = stack_description->frames;
  for (size_t i = 0; i < max_frames; ++i) {
    output->grow(1);
    StackFrame& sf = output->back();

    sf.line_number = stack_trace[i].lineno;
    sf.column_number = stack_trace[i].columnno;
    sf.function_name = "global code";
    if (stack_trace[i].fun) {
      JS::RootedString rooted_string(context,
                                     JS_GetFunctionId(stack_trace[i].fun));
      if (rooted_string) {
        const jschar* jstring_raw = rooted_string->getChars(context);

        if (!jstring_raw) {
          sf.function_name = "";
        } else {
          // Note, this is a wide-string conversion.
          sf.function_name.assign(jstring_raw,
                                  jstring_raw + rooted_string->length());
        }
      } else {
        // anonymous function
        sf.function_name = "(anonymous function)";
      }
    }
    if (stack_trace[i].script) {
      sf.source_url = stack_trace[i].script->filename();
    }
  }
  JS::FreeStackDescription(context, stack_description);
}

}  // namespace util
}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
