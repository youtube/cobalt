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
#include "third_party/mozjs-45/js/src/builtin/ModuleObject.h"
#include "third_party/mozjs-45/js/src/jsapi.h"
#include "third_party/mozjs-45/js/src/jscntxt.h"
#include "third_party/mozjs-45/js/src/jscntxtinlines.h"
#include "third_party/mozjs-45/js/src/jsfriendapi.h"
#include "third_party/mozjs-45/js/src/jsprf.h"

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

void GetStackTrace(JSContext* context, size_t max_frames,
                   nb::RewindableVector<StackFrame>* output) {
  DCHECK(output);
  output->rewindAll();

  JS::RootedObject stack(context);
  if (!js::GetContextCompartment(context)) {
    LOG(WARNING) << "Tried to get stack trace without access to compartment.";
    return;
  }
  if (!JS::CaptureCurrentStack(context, &stack, max_frames)) {
    return;
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
    sf.function_name = "";
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

    output->push_back(sf);
    JS::GetSavedFrameParent(context, stack, &stack);
  }
}

void GetStackTraceUsingInternalApi(JSContext* context, size_t max_frames,
                                   nb::RewindableVector<StackFrame>* output) {
  DCHECK(output);
  output->rewindAll();

  int frames_captured = 0;
  for (js::AllFramesIter it(context);
       !it.done() && (max_frames == 0 || frames_captured < max_frames);
       ++it, ++frames_captured) {
    const char* filename = JS_GetScriptFilename(it.script());
    unsigned column = 0;
    unsigned line = js::PCToLineNumber(it.script(), it.pc(), &column);

    StackFrame sf;
    sf.source_url = filename;
    sf.line_number = line;
    sf.column_number = column;
    if (it.isFunctionFrame()) {
      JSFunction* function = it.callee(context);
      if (function->displayAtom()) {
        auto* atom = function->displayAtom();
        JS::AutoCheckCannotGC nogc;
        auto* chars = atom->latin1Chars(nogc);
        sf.function_name = std::string(chars, chars + atom->length());
      } else {
        sf.function_name = "<anonymous>";
      }
    } else {
      sf.function_name = "";
    }

    output->push_back(sf);
  }
}

}  // namespace util
}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
