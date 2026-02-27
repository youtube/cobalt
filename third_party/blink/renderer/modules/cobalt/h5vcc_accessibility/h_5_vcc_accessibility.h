// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_ACCESSIBILITY_H_5_VCC_ACCESSIBILITY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_ACCESSIBILITY_H_5_VCC_ACCESSIBILITY_H_

#include <optional>

#include "cobalt/browser/h5vcc_accessibility/public/mojom/h5vcc_accessibility.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

class LocalDOMWindow;
class ScriptState;
class ScriptPromiseResolver;

class MODULES_EXPORT H5vccAccessibility final
    // TODO: EventTargetWithInlineData should be replaced with EventTarget
    // after Chromium base version update, see
    // https://chromium-review.googlesource.com/c/chromium/src/+/4621887
    : public EventTargetWithInlineData,
      public ExecutionContextLifecycleObserver,
      public h5vcc_accessibility::mojom::blink::H5vccAccessibilityClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit H5vccAccessibility(LocalDOMWindow&);

  // This would auto-generate ontexttospeechchange and setOntexttospeechchange
  // for us.
  DEFINE_ATTRIBUTE_EVENT_LISTENER(texttospeechchange, kTexttospeechchange)

  // Web-exposed interface:
  bool textToSpeech();

  // Mojom interface:
  void NotifyTextToSpeechChange(bool enabled) override;

  void AddedEventListener(
      const AtomicString& event_type,
      RegisteredEventListener& registered_listener) override;
  void RemovedEventListener(
      const AtomicString& event_type,
      const RegisteredEventListener& registered_listener) override;
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;
  void ContextDestroyed() override;
  void Trace(Visitor* visitor) const override;

 private:
  void EnsureRemoteIsBound();
  void EnsureReceiverIsBound();

  // TODO(b/458102489): Remove this value caching when Kabuki uses the Event
  // exclusively.
  std::optional<bool> cached_text_to_speech_enabled_;

  // Proxy to the remote H5vccAccessibilityBrowser implementation.
  HeapMojoRemote<h5vcc_accessibility::mojom::blink::H5vccAccessibilityBrowser>
      remote_;
  // Pipe to receive notifications from the remote interface implementation.
  HeapMojoReceiver<h5vcc_accessibility::mojom::blink::H5vccAccessibilityClient,
                   H5vccAccessibility>
      notification_receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_ACCESSIBILITY_H_5_VCC_ACCESSIBILITY_H_
