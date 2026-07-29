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

#include "third_party/blink/renderer/core/inspector/inspector_animation_agent.h"

namespace blink {

// Agent stubs used by inspector_trace_events.cc and probes
String InspectorAnimationAgent::AnimationDisplayName(const Animation&) {
  return String();
}
void InspectorAnimationAgent::DidClearDocumentOfWindowObject(LocalFrame*) {}
void InspectorAnimationAgent::DidCreateAnimation(unsigned int) {}
void InspectorAnimationAgent::AnimationUpdated(Animation*) {}
void InspectorAnimationAgent::Trace(Visitor*) const {}

}  // namespace blink
