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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_METRICS_H_5_VCC_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_METRICS_H_5_VCC_METRICS_H_

#include "cobalt/browser/h5vcc_metrics/public/mojom/h5vcc_metrics.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_h_5_vcc_metric_type.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
class LocalDOMWindow;
class ScriptState;

class MODULES_EXPORT H5vccMetrics final
    : public EventTarget,
      public ExecutionContextLifecycleObserver,
      public h5vcc_metrics::mojom::blink::MetricsListener {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit H5vccMetrics(LocalDOMWindow&);
  ~H5vccMetrics() override;

  // ExecutionContextLifecycleObserver impl.
  void ContextDestroyed() override;

  // Web-exposed interface:
  DEFINE_ATTRIBUTE_EVENT_LISTENER(metrics, kMetrics)
  ScriptPromise<IDLUndefined> enable(ScriptState*, ExceptionState&);
  ScriptPromise<IDLUndefined> disable(ScriptState*, ExceptionState&);
  bool isEnabled();
  ScriptPromise<IDLUndefined> setMetricEventInterval(ScriptState*,
                                                     uint64_t,
                                                     ExceptionState&);
  ScriptPromise<IDLString> requestHistograms(ScriptState*, ExceptionState&);

  // EventTarget impl.
  ExecutionContext* GetExecutionContext() const override {
    return ExecutionContextLifecycleObserver::GetExecutionContext();
  }
  const AtomicString& InterfaceName() const override {
    return event_target_names::kH5VccMetrics;
  }

  // MetricsListener impl.
  void OnMetrics(h5vcc_metrics::mojom::H5vccMetricType metric_type,
                 const WTF::String& metric_payload) override;

  void Trace(Visitor*) const override;

 protected:
  // EventTarget:
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;
  void RemovedEventListener(
      const AtomicString& event_type,
      const RegisteredEventListener& registered_listener) override;

 private:
  void OnEnable(ScriptPromiseResolver<IDLUndefined>* resolver);
  void OnDisable(ScriptPromiseResolver<IDLUndefined>* resolver);
  void OnSetMetricEventInterval(ScriptPromiseResolver<IDLUndefined>* resolver);
  void OnRequestHistograms(ScriptPromiseResolver<IDLString>* resolver,
                           const WTF::String& histograms_base64);

  void EnsureRemoteIsBound();
  void OnCloseConnection();

  void MaybeRegisterMojoListener();
  void MaybeUnregisterMojoListener();

  void CleanupPromise(ScriptPromiseResolverBase* resolver);

  bool HasValidExecutionContext();

  HeapMojoRemote<h5vcc_metrics::mojom::blink::H5vccMetrics>
      remote_h5vcc_metrics_;
  HeapMojoReceiver<h5vcc_metrics::mojom::blink::MetricsListener, H5vccMetrics>
      receiver_;

  // If the Mojo connection closes/errors all promises are rejected.
  HeapHashSet<Member<ScriptPromiseResolverBase>> h5vcc_metrics_promises_;

  bool is_reporting_enabled_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_METRICS_H_5_VCC_METRICS_H_
