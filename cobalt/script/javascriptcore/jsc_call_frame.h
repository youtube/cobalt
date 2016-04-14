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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JSC_CALL_FRAME_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JSC_CALL_FRAME_H_

#include <string>

#include "base/optional.h"
#include "cobalt/script/call_frame.h"
#include "cobalt/script/javascriptcore/jsc_global_object.h"
#include "cobalt/script/javascriptcore/jsc_object_handle_holder.h"
#include "cobalt/script/scope.h"

#include "third_party/WebKit/Source/JavaScriptCore/config.h"
#include "third_party/WebKit/Source/JavaScriptCore/debugger/DebuggerCallFrame.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

// JavaScriptCore-specific implementation of a JavaScript call frame.
class JSCCallFrame : public CallFrame {
 public:
  JSCCallFrame(const JSC::DebuggerCallFrame& call_frame, intptr_t script_id,
               int line_number);
  JSCCallFrame(const JSC::DebuggerCallFrame& call_frame, intptr_t script_id,
               int line_number, int column_number);
  ~JSCCallFrame() OVERRIDE;

  std::string GetCallFrameId() OVERRIDE;
  scoped_ptr<CallFrame> GetCaller() OVERRIDE;
  std::string GetFunctionName() OVERRIDE;
  int GetLineNumber() OVERRIDE;
  base::optional<int> GetColumnNumber() OVERRIDE;
  std::string GetScriptId() OVERRIDE;
  scoped_ptr<Scope> GetScopeChain() OVERRIDE;
  const OpaqueHandleHolder* GetThis() OVERRIDE;

 private:
  // Called by both constructors to perform common initialization.
  void Initialize();

  JSC::DebuggerCallFrame call_frame_;
  std::string script_id_;
  int line_number_;
  base::optional<int> column_number_;
  JSCGlobalObject* global_object_;
  scoped_ptr<JSCObjectHandleHolder> this_holder_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JSC_CALL_FRAME_H_
