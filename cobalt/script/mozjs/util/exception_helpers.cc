/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/script/mozjs/util/exception_helpers.h"

#include <algorithm>

#include "cobalt/script/mozjs/conversion_helpers.h"
#include "cobalt/script/mozjs/mozjs_exception_state.h"
#include "third_party/mozjs/js/src/jsdbgapi.h"
#include "third_party/mozjs/js/src/jsscript.h"

namespace cobalt {
namespace script {
namespace mozjs {
namespace util {
std::vector<StackFrame> GetStackTrace(JSContext* context, int max_frames) {
  JS::StackDescription* stack_description =
      JS::DescribeStack(context, max_frames);
  if (max_frames == 0) {
    max_frames = static_cast<int>(stack_description->nframes);
  } else {
    max_frames =
        std::min(max_frames, static_cast<int>(stack_description->nframes));
  }
  JS::FrameDescription* stack_trace = stack_description->frames;
  std::vector<StackFrame> stack_frames(max_frames);
  for (int i = 0; i < max_frames; ++i) {
    StackFrame sf;
    sf.line_number = stack_trace[i].lineno;
    sf.column_number = stack_trace[i].columnno;
    sf.function_name = "global code";
    if (stack_trace[i].fun) {
      JS::RootedString rooted_string(context,
                                     JS_GetFunctionId(stack_trace[i].fun));
      JS::RootedValue rooted_value(context, STRING_TO_JSVAL(rooted_string));
      MozjsExceptionState exception_state(context);
      FromJSValue(context, rooted_value, kNoConversionFlags, &exception_state,
                  &sf.function_name);
    }
    if (stack_trace[i].script) {
      sf.source_url = stack_trace[i].script->filename();
    }
    stack_frames[i] = sf;
  }
  JS::FreeStackDescription(context, stack_description);
  return stack_frames;
}
}  // namespace util
}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
