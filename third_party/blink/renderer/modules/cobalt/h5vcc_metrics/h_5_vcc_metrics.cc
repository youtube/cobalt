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
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_metrics/metrics_event.h"

namespace blink {

H5vccMetrics::H5vccMetrics(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_metrics_(window.GetExecutionContext()),
      receiver_(this, window.GetExecutionContext()) {}

void H5vccMetrics::ContextDestroyed() {
  receiver_.reset();
}

EventListener* H5vccMetrics::onmetrics() {
  return GetAttributeEventListener(event_type_names::kMetrics);
}

void H5vccMetrics::setOnmetrics(EventListener* listener) {
  SetAttributeEventListener(event_type_names::kMetrics, listener);
  EnsureReceiverIsBound();
  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  remote_h5vcc_metrics_->AddListener(
      receiver_.BindNewPipeAndPassRemote(task_runner));
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

void H5vccMetrics::OnMetrics(const WTF::String& tbd) {
  DispatchEvent(
      *MakeGarbageCollected<MetricsEvent>(event_type_names::kMetrics, tbd));
}

void H5vccMetrics::OnEnable(ScriptPromiseResolver* resolver) {
  is_reporting_enabled_ = true;
  resolver->Resolve();
}

void H5vccMetrics::OnDisable(ScriptPromiseResolver* resolver) {
  is_reporting_enabled_ = false;
  resolver->Resolve();
}

void H5vccMetrics::OnIsEnabled(ScriptPromiseResolver* resolver, bool result) {
  resolver->Resolve(result);
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
}

void H5vccMetrics::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(remote_h5vcc_metrics_);
  visitor->Trace(receiver_);
}

}  // namespace blink
