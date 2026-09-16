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

#include "third_party/blink/renderer/core/inspector/inspector_media_agent.h"

namespace blink {

void InspectorMediaAgent::PlayerErrorsRaised(
    const WebString&,
    const Vector<InspectorPlayerError>&) {}
void InspectorMediaAgent::PlayerEventsAdded(
    const WebString&,
    const Vector<InspectorPlayerEvent>&) {}
void InspectorMediaAgent::PlayerMessagesLogged(
    const WebString&,
    const Vector<InspectorPlayerMessage>&) {}
void InspectorMediaAgent::PlayerPropertiesChanged(
    const WebString&,
    const Vector<InspectorPlayerProperty>&) {}
void InspectorMediaAgent::PlayersCreated(const Vector<WebString>&) {}
void InspectorMediaAgent::Trace(Visitor*) const {}

}  // namespace blink
