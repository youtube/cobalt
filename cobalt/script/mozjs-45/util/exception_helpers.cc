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

#include "cobalt/script/mozjs-45/util/exception_helpers.h"

#include <algorithm>
#include <string>

#include "cobalt/script/mozjs-45/conversion_helpers.h"
#include "cobalt/script/mozjs-45/mozjs_exception_state.h"
#include "third_party/mozjs-45/js/public/UbiNode.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {
namespace util {

std::string GetExceptionString(JSContext* context) {
  if (!JS_IsExceptionPending(context)) {
    return std::string("No exception pending.");
  }
  JS::RootedValue exception(context);
  JS_GetPendingException(context, &exception);
  JS_ReportPendingException(context);
  return GetExceptionString(context, exception);
}

std::string GetExceptionString(JSContext* context, JS::HandleValue exception) {
  std::string exception_string;
  MozjsExceptionState exception_state(context);
  FromJSValue(context, exception, kNoConversionFlags, &exception_state,
              &exception_string);
  return exception_string;
}

std::vector<StackFrame> GetStackTrace(JSContext* context, int max_frames) {
  JS::RootedObject stack(context);
  std::vector<StackFrame> stack_frames;
  if (!js::GetContextCompartment(context)) {
    LOG(WARNING) << "Tried to get stack trace without access to compartment.";
    return stack_frames;
  }
  if (!JS::CaptureCurrentStack(context, &stack, max_frames)) {
    return stack_frames;
  }

  while (stack != NULL) {
    StackFrame sf;

    JS::RootedString source(context);
    uint32_t line;
    uint32_t column;
    JS::RootedString name(context);

    JS::GetSavedFrameSource(context, stack, &source);
    JS::GetSavedFrameLine(context, stack, &line);
    JS::GetSavedFrameColumn(context, stack, &column);
    JS::GetSavedFrameFunctionDisplayName(context, stack, &name);

    sf.line_number = static_cast<int>(line);
    sf.column_number = static_cast<int>(column);
    sf.function_name = "global code";
    if (name) {
      JS::RootedValue rooted_value(context);
      rooted_value.setString(name);
      MozjsExceptionState exception_state(context);
      FromJSValue(context, rooted_value, kNoConversionFlags, &exception_state,
                  &sf.function_name);
    }
    if (source) {
      JS::RootedValue rooted_value(context);
      rooted_value.setString(source);
      MozjsExceptionState exception_state(context);
      FromJSValue(context, rooted_value, kNoConversionFlags, &exception_state,
                  &sf.source_url);
    }

    stack_frames.push_back(sf);
    JS::GetSavedFrameParent(context, stack, &stack);
  }

  return stack_frames;
}

}  // namespace util
}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
