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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_RUNTIME_H_5_VCC_RUNTIME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_RUNTIME_H_5_VCC_RUNTIME_H_

#include "cobalt/browser/h5vcc_runtime/public/mojom/h5vcc_runtime.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
class LocalDOMWindow;
class ScriptState;

class MODULES_EXPORT H5vccRuntime final
    : public EventTarget,
      public ExecutionContextLifecycleObserver,
      public h5vcc_runtime::mojom::blink::DeepLinkListener {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit H5vccRuntime(LocalDOMWindow&);

  void ContextDestroyed() override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(deeplink, kDeeplink)

  // EventTarget interface:
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;
  void RemovedEventListener(const AtomicString& event_type,
                            const RegisteredEventListener&) override;

  // Mojom interface:
  void NotifyDeepLink(const WTF::String& deep_link) override;

  // EventTarget impl.
  ExecutionContext* GetExecutionContext() const override {
    return ExecutionContextLifecycleObserver::GetExecutionContext();
  }
  const AtomicString& InterfaceName() const override {
    return event_target_names::kH5VccRuntime;
  }

  void Trace(Visitor*) const override;

 private:
  void MaybeFireDeepLinkEvent(const String&);
  void EnsureRemoteIsBound();

  void MaybeRegisterMojoListener();
  void MaybeUnregisterMojoListener();

  HeapMojoRemote<h5vcc_runtime::mojom::blink::H5vccRuntime>
      remote_h5vcc_runtime_;
  HeapMojoReceiver<h5vcc_runtime::mojom::blink::DeepLinkListener, H5vccRuntime>
      deep_link_receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_RUNTIME_H_5_VCC_RUNTIME_H_
