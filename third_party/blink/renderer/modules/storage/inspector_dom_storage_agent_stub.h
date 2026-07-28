
#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_INSPECTOR_DOM_STORAGE_AGENT_STUB_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_INSPECTOR_DOM_STORAGE_AGENT_STUB_H_

#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/storage/storage_area.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class InspectedFrames;

class MODULES_EXPORT InspectorDOMStorageAgent final
    : public InspectorAgent {
 public:
  explicit InspectorDOMStorageAgent(InspectedFrames*) {}
  void Init(CoreProbeSink*, protocol::UberDispatcher*, InspectorSessionState*) override {}
  void Dispose() override {}

  void DidDispatchDOMStorageEvent(const String&,
                                  const String&,
                                  const String&,
                                  StorageArea::StorageType,
                                  const BlinkStorageKey&) {}
};

}  // namespace blink


#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_INSPECTOR_DOM_STORAGE_AGENT_STUB_H_
