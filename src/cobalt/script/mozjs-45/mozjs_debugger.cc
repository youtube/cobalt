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

#include "cobalt/script/mozjs/mozjs_debugger.h"

#include "base/logging.h"

namespace cobalt {
namespace script {

// Static factory method declared in public interface.
scoped_ptr<ScriptDebugger> ScriptDebugger::CreateDebugger(
    GlobalEnvironment* global_environment, Delegate* delegate) {
  return scoped_ptr<ScriptDebugger>(
      new mozjs::MozjsDebugger(global_environment, delegate));
}

namespace mozjs {

MozjsDebugger::MozjsDebugger(GlobalEnvironment* global_environment,
                             Delegate* delegate) {
  NOTIMPLEMENTED();
}

MozjsDebugger::~MozjsDebugger() { NOTIMPLEMENTED(); }

void MozjsDebugger::Attach() { NOTIMPLEMENTED(); }

void MozjsDebugger::Detach() { NOTIMPLEMENTED(); }

void MozjsDebugger::Pause() { NOTIMPLEMENTED(); }

void MozjsDebugger::Resume() { NOTIMPLEMENTED(); }

void MozjsDebugger::SetBreakpoint(const std::string& script_id, int line_number,
                                  int column_number) {
  NOTIMPLEMENTED();
}

script::ScriptDebugger::PauseOnExceptionsState
MozjsDebugger::SetPauseOnExceptions(PauseOnExceptionsState state) {
  NOTIMPLEMENTED();
  return kNone;
}

void MozjsDebugger::StepInto() { NOTIMPLEMENTED(); }

void MozjsDebugger::StepOut() { NOTIMPLEMENTED(); }

void MozjsDebugger::StepOver() { NOTIMPLEMENTED(); }

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
