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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_COBALT_INSPECTOR_DOM_STORAGE_AGENT_STUB_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_COBALT_INSPECTOR_DOM_STORAGE_AGENT_STUB_H_

#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/storage/storage_area.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class InspectedFrames;

class MODULES_EXPORT InspectorDOMStorageAgent final : public InspectorAgent {
 public:
  explicit InspectorDOMStorageAgent(InspectedFrames*) {}
  void Init(CoreProbeSink*,
            protocol::UberDispatcher*,
            InspectorSessionState*) override {}
  void Dispose() override {}

  void DidDispatchDOMStorageEvent(const String&,
                                  const String&,
                                  const String&,
                                  StorageArea::StorageType,
                                  const BlinkStorageKey&) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_COBALT_INSPECTOR_DOM_STORAGE_AGENT_STUB_H_
