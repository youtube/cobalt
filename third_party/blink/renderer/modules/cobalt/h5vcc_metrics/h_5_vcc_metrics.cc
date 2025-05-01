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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_metrics/h_5_vcc_metrics.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_h_5_vcc.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_h_5_vcc_metric_type.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_metrics/metrics_event.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"

namespace blink {

H5vccMetrics::H5vccMetrics(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_metrics_(window.GetExecutionContext()),
      receiver_(this, window.GetExecutionContext()) {}

void H5vccMetrics::ContextDestroyed() {
  OnCloseConnection();
}

ScriptPromise H5vccMetrics::enable(ScriptState* script_state,
                                   ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  remote_h5vcc_metrics_->Enable(
      /*enable=*/true,
      WTF::BindOnce(&H5vccMetrics::OnEnable, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise H5vccMetrics::disable(ScriptState* script_state,
                                    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  remote_h5vcc_metrics_->Enable(
      /*enable=*/false,
      WTF::BindOnce(&H5vccMetrics::OnDisable, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

bool H5vccMetrics::isEnabled() {
  return is_reporting_enabled_;
}

ScriptPromise H5vccMetrics::setMetricEventInterval(
    ScriptState* script_state,
    uint64_t interval_seconds,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  remote_h5vcc_metrics_->SetMetricEventInterval(
      interval_seconds,
      WTF::BindOnce(&H5vccMetrics::OnSetMetricEventInterval,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
}

void H5vccMetrics::OnMetrics(h5vcc_metrics::mojom::H5vccMetricType metric_type,
                             const WTF::String& metric_payload) {
  // For now, only UMA is supported.
  if (metric_type == h5vcc_metrics::mojom::H5vccMetricType::kCobaltUma) {
    DispatchEvent(*MakeGarbageCollected<MetricsEvent>(
        event_type_names::kMetrics,
        V8H5vccMetricType(V8H5vccMetricType::Enum::kCobaltUma),
        metric_payload));
  }
}

void H5vccMetrics::AddedEventListener(const AtomicString& event_type,
                                      RegisteredEventListener& listener) {
  EventTarget::AddedEventListener(event_type, listener);

  if (event_type != event_type_names::kMetrics) {
    return;
  }
  EnsureReceiverIsBound();
}

void H5vccMetrics::OnEnable(ScriptPromiseResolver* resolver) {
  is_reporting_enabled_ = true;
  resolver->Resolve();
}

void H5vccMetrics::OnDisable(ScriptPromiseResolver* resolver) {
  is_reporting_enabled_ = false;
  resolver->Resolve();
}

void H5vccMetrics::OnSetMetricEventInterval(ScriptPromiseResolver* resolver) {
  resolver->Resolve();
}

void H5vccMetrics::EnsureReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_h5vcc_metrics_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_h5vcc_metrics_.BindNewPipeAndPassReceiver(task_runner));
  remote_h5vcc_metrics_.set_disconnect_handler(WTF::BindOnce(
      &H5vccMetrics::OnCloseConnection, WrapWeakPersistent(this)));
  remote_h5vcc_metrics_->AddListener(
      receiver_.BindNewPipeAndPassRemote(task_runner));
}

void H5vccMetrics::OnCloseConnection() {
  remote_h5vcc_metrics_.reset();
  receiver_.reset();

  // TODO: Keep track of in-flight Promises and reject them here.
}

void H5vccMetrics::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(remote_h5vcc_metrics_);
  visitor->Trace(receiver_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
