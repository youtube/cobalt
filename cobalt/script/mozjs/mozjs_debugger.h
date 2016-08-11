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
#ifndef COBALT_SCRIPT_MOZJS_MOZJS_DEBUGGER_H_
#define COBALT_SCRIPT_MOZJS_MOZJS_DEBUGGER_H_

#include <string>

#include "cobalt/script/script_debugger.h"

namespace cobalt {
namespace script {
namespace mozjs {

class MozjsDebugger : public ScriptDebugger {
 public:
  MozjsDebugger(GlobalObjectProxy* global_object_proxy, Delegate* delegate);
  ~MozjsDebugger() OVERRIDE;

  // Implementation of ScriptDebugger.
  void Attach() OVERRIDE;
  void Detach() OVERRIDE;
  void Pause() OVERRIDE;
  void Resume() OVERRIDE;
  void SetBreakpoint(const std::string& script_id, int line_number,
                     int column_number) OVERRIDE;
  PauseOnExceptionsState SetPauseOnExceptions(
      PauseOnExceptionsState state) OVERRIDE;
  void StepInto() OVERRIDE;
  void StepOut() OVERRIDE;
  void StepOver() OVERRIDE;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_MOZJS_DEBUGGER_H_
