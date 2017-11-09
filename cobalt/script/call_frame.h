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

#ifndef COBALT_SCRIPT_CALL_FRAME_H_
#define COBALT_SCRIPT_CALL_FRAME_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "cobalt/script/opaque_handle.h"
#include "cobalt/script/scope.h"

namespace cobalt {
namespace script {

// Opaque type that encapsulates a call frame, as used by the JavaScript
// debugger and devtools. When the script debugger pauses, an instance of a
// derived class is created and passed to the handler, representing the top of
// the call stack. The instance is only valid while the script debugger is
// paused.
// https://developer.chrome.com/devtools/docs/protocol/1.1/debugger#type-CallFrame
// https://developer.chrome.com/devtools/docs/protocol/1.1/debugger#event-paused
class CallFrame {
 public:
  virtual ~CallFrame() {}

  // A unique string identifying this call frame.
  // https://developer.chrome.com/devtools/docs/protocol/1.1/debugger#type-CallFrameId
  virtual std::string GetCallFrameId() = 0;

  // The next call frame on the stack.
  virtual scoped_ptr<CallFrame> GetCaller() = 0;

  // The name of the function called at this stack frame. May be empty for
  // anonymous functions.
  virtual std::string GetFunctionName() = 0;

  // Current line number being executed.
  virtual int GetLineNumber() = 0;

  // Current column number of executed statement.
  virtual base::optional<int> GetColumnNumber() = 0;

  // The ID of the script being executed. Corresponds to the scriptId parameter
  // in the Debugger.scriptParsed event notification when the script was first
  // parsed.
  // https://developer.chrome.com/devtools/docs/protocol/1.1/debugger#event-scriptParsed
  virtual std::string GetScriptId() = 0;

  // The first entry in the scope chain for this call frame.
  // https://developer.chrome.com/devtools/docs/protocol/1.1/debugger#type-Scope
  virtual scoped_ptr<Scope> GetScopeChain() = 0;

  // An opaque handle to the current JavaScript |this| object.
  // May be NULL if the value of |this| in the call frame is not an object.
  virtual const OpaqueHandleHolder* GetThis() = 0;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_CALL_FRAME_H_
