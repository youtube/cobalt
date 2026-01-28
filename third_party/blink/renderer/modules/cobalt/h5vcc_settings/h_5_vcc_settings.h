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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_SETTINGS_H_5_VCC_SETTINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_SETTINGS_H_5_VCC_SETTINGS_H_

#include <optional>

#include "cobalt/browser/h5vcc_settings/public/mojom/h5vcc_settings.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_long_string.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class LocalDOMWindow;

class MODULES_EXPORT H5vccSettings final
    : public ScriptWrappable,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit H5vccSettings(LocalDOMWindow&);

  void ContextDestroyed() override;

  // Web-exposed interface:

  ScriptPromise set(ScriptState* script_state,
                    const WTF::String& name,
                    const V8UnionLongOrString* value,
                    ExceptionState& exception_state);

  std::optional<bool> GetAppendFirstSegmentSynchronously() const {
    return append_first_segment_synchronously_;
  }

  void Trace(Visitor*) const override;

 private:
  void OnSetValueFinished(ScriptPromiseResolver*);
  void OnConnectionError();
  void EnsureReceiverIsBound();

  HeapMojoRemote<h5vcc_settings::mojom::blink::H5vccSettings>
      remote_h5vcc_settings_;
  // Holds promises associated with outstanding async remote_h5vcc_settings_
  // requests so that they can be rejected in the case of a Mojo connection
  // error.
  HeapHashSet<Member<ScriptPromiseResolver>> ongoing_requests_;

  std::optional<bool> append_first_segment_synchronously_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_SETTINGS_H_5_VCC_SETTINGS_H_
