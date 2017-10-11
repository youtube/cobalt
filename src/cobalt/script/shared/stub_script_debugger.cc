// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/script/script_debugger.h"

#include "base/logging.h"

namespace cobalt {
namespace script {

class StubScriptDebugger : public ScriptDebugger {
 public:
  StubScriptDebugger(GlobalEnvironment* global_environment,
                     Delegate* delegate) {
    NOTIMPLEMENTED();
  }
  ~StubScriptDebugger() override { NOTIMPLEMENTED(); }

  void Attach() override { NOTIMPLEMENTED(); }
  void Detach() override { NOTIMPLEMENTED(); }
  void Pause() override { NOTIMPLEMENTED(); }
  void Resume() override { NOTIMPLEMENTED(); }
  void SetBreakpoint(const std::string& script_id, int line_number,
                     int column_number) override {
    NOTIMPLEMENTED();
  }
  PauseOnExceptionsState SetPauseOnExceptions(
      PauseOnExceptionsState state) override {
    NOTIMPLEMENTED();
    return kNone;
  }
  void StepInto() override { NOTIMPLEMENTED(); }
  void StepOut() override { NOTIMPLEMENTED(); }
  void StepOver() override { NOTIMPLEMENTED(); }
};

// Static factory method declared in public interface.
scoped_ptr<ScriptDebugger> ScriptDebugger::CreateDebugger(
    GlobalEnvironment* global_environment, Delegate* delegate) {
  return scoped_ptr<ScriptDebugger>(
      new StubScriptDebugger(global_environment, delegate));
}

}  // namespace script
}  // namespace cobalt
