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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_PLATFORM_SERVICE_H_5_VCC_PLATFORM_SERVICE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_PLATFORM_SERVICE_H_5_VCC_PLATFORM_SERVICE_H_

#include "cobalt/browser/h5vcc_platform_service/public/mojom/h5vcc_platform_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_receive_callback.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DOMArrayBuffer;
class ExceptionState;
class ScriptState;

class MODULES_EXPORT H5vccPlatformService final
    : public ScriptWrappable,
      public ExecutionContextLifecycleObserver,
      public h5vcc_platform_service::mojom::blink::PlatformServiceObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Web-exposed static methods:
  static bool has(ScriptState* script_state, const WTF::String& service_name);
  static H5vccPlatformService* open(ScriptState* script_state,
                                    const WTF::String& service_name,
                                    V8ReceiveCallback* receive_callback);

  H5vccPlatformService(LocalDOMWindow& window,
                       const WTF::String& service_name,
                       V8ReceiveCallback* receive_callback);
  ~H5vccPlatformService() override;

  // Web-exposed instance methods:
  DOMArrayBuffer* send(DOMArrayBuffer* data, ExceptionState& exception_state);
  void close();

  void ContextDestroyed() override;

  // Renderer implementation for cobalt::mojom::blink::PlatformServiceObserver
  void OnDataReceived(const WTF::Vector<uint8_t>& data) override;

  void Trace(Visitor* visitor) const override;

 private:
  void OnManagerConnectionError();
  void OnServiceConnectionError();

  WTF::String service_name_;
  Member<V8ReceiveCallback> receive_callback_;

  // Remote to the specific PlatformService instance in the browser
  HeapMojoRemote<h5vcc_platform_service::mojom::blink::PlatformService>
      platform_service_remote_{GetExecutionContext()};

  // Receiver for messages from the browser for this instance
  HeapMojoReceiver<
      h5vcc_platform_service::mojom::blink::PlatformServiceObserver,
      H5vccPlatformService>
      observer_receiver_{this, GetExecutionContext()};

  bool service_opened_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_PLATFORM_SERVICE_H_5_VCC_PLATFORM_SERVICE_H_
