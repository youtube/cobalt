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
#ifndef COBALT_SCRIPT_SCRIPT_DEBUGGER_H_
#define COBALT_SCRIPT_SCRIPT_DEBUGGER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "cobalt/script/call_frame.h"
#include "cobalt/script/opaque_handle.h"
#include "cobalt/script/script_object.h"
#include "cobalt/script/source_provider.h"

namespace cobalt {
namespace script {

class GlobalObjectProxy;

// Engine-independent pure virtual interface to a JavaScript debugger.
// Used as an opaque interface to the specific debugger implementation,
// e.g. JSCDebugger.
// Only pure virtual or static methods should be added to this class.
// No data members should be added to this class.
// https://engdoc.***REMOVED***/eng/doc/devguide/cpp/styleguide.shtml?cl=head#Interfaces
class ScriptDebugger {
 public:
  // Ideally, we want the delegate to do as much as possible, as its
  // implementation can be independent of the specific JS engine.
  class Delegate {
   public:
    // Called when the script debugger wants to detach.
    virtual void OnScriptDebuggerDetach(const std::string& reason) = 0;

    // Called when the script debugger wants to pause script execution.
    virtual void OnScriptDebuggerPause(scoped_ptr<CallFrame> call_frame) = 0;

    // Called when a script fails to parse.
    virtual void OnScriptFailedToParse(
        scoped_ptr<SourceProvider> source_provider) = 0;

    // Called when a script is successfully parsed.
    virtual void OnScriptParsed(scoped_ptr<SourceProvider> source_provider) = 0;
  };

  // Possible pause on exception states.
  enum PauseOnExceptionsState { kAll, kNone, kUncaught };

  // Factory method to create an engine-specific instance. Implementation to be
  // provided by derived class.
  static scoped_ptr<ScriptDebugger> CreateDebugger(
      GlobalObjectProxy* global_object_proxy, Delegate* delegate);

  // Code execution control. Implementations may use
  // |Delegate::OnScriptDebuggerPause| to have the delegate handle the
  // actual blocking of the thread in an engine-independent way.
  virtual void Pause() = 0;
  virtual void Resume() = 0;
  virtual void SetPauseOnExceptions(PauseOnExceptionsState state) = 0;
  virtual void StepInto() = 0;
  virtual void StepOut() = 0;
  virtual void StepOver() = 0;

 protected:
  virtual ~ScriptDebugger() {}
  friend class scoped_ptr<ScriptDebugger>;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_SCRIPT_DEBUGGER_H_
