// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/core/inspector/inspector_event_breakpoints_agent.h"


namespace blink {

void InspectorEventBreakpointsAgent::DidCreateCanvasContext() {}
void InspectorEventBreakpointsAgent::DidCreateOffscreenCanvasContext() {}
void InspectorEventBreakpointsAgent::DidFireWebGLError(const String&) {}
void InspectorEventBreakpointsAgent::DidFireWebGLWarning() {}
void InspectorEventBreakpointsAgent::DidFireWebGLErrorOrWarning(const String&) {}
void InspectorEventBreakpointsAgent::ScriptExecutionBlockedByCSP(const String&) {}
void InspectorEventBreakpointsAgent::BreakableLocation(const char*) {}
void InspectorEventBreakpointsAgent::Will(const probe::ExecuteScript&) {}
void InspectorEventBreakpointsAgent::Did(const probe::ExecuteScript&) {}
void InspectorEventBreakpointsAgent::Will(const probe::UserCallback&) {}
void InspectorEventBreakpointsAgent::Did(const probe::UserCallback&) {}
void InspectorEventBreakpointsAgent::DidCreateAudioContext() {}
void InspectorEventBreakpointsAgent::DidCloseAudioContext() {}
void InspectorEventBreakpointsAgent::DidResumeAudioContext() {}
void InspectorEventBreakpointsAgent::DidSuspendAudioContext() {}

} // namespace blink
