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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_H5VCC_METRICS_H_5_VCC_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_H5VCC_METRICS_H_5_VCC_METRICS_H_

#include "cobalt/browser/h5vcc_metrics/public/mojom/h5vcc_metrics.mojom-blink.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
class LocalDOMWindow;
class ScriptState;
class ScriptPromiseResolver;

class MODULES_EXPORT H5vccMetrics final
    : public EventTargetWithInlineData,
      public ExecutionContextLifecycleObserver,
      public h5vcc_metrics::mojom::blink::MetricsListener {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit H5vccMetrics(LocalDOMWindow&);

  void ContextDestroyed() override;

  // Web-exposed interface:
  EventListener* onmetrics();
  void setOnmetrics(EventListener* listener);

  ScriptPromise enable(ScriptState*, ExceptionState&);
  ScriptPromise disable(ScriptState*, ExceptionState&);
  bool isEnabled();
  ScriptPromise setMetricEventInterval(ScriptState*, uint64_t, ExceptionState&);

  // EventTargetWithInlineData impl.
  ExecutionContext* GetExecutionContext() const override {
    return ExecutionContextLifecycleObserver::GetExecutionContext();
  }
  const AtomicString& InterfaceName() const override {
    return event_target_names::kH5VccMetrics;
  }

  // MetricsListener impl.
  void OnMetrics(const WTF::String& tbd) override;

  void Trace(Visitor*) const override;

 private:
  void FireMetricsEvent(const String&);
  void OnEnable(ScriptPromiseResolver* resolver);
  void OnDisable(ScriptPromiseResolver* resolver);
  void OnIsEnabled(ScriptPromiseResolver* resolver, bool result);
  void OnSetMetricEventInterval(ScriptPromiseResolver* resolver);

  void EnsureReceiverIsBound();
  HeapMojoRemote<h5vcc_metrics::mojom::blink::H5vccMetrics>
      remote_h5vcc_metrics_;
  HeapMojoReceiver<h5vcc_metrics::mojom::blink::MetricsListener, H5vccMetrics>
      receiver_;

  bool is_reporting_enabled_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_H5VCC_METRICS_H_5_VCC_METRICS_H_
