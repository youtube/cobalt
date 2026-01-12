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
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_metrics/metrics_event.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"

namespace blink {

H5vccMetrics::H5vccMetrics(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_metrics_(window.GetExecutionContext()),
      receiver_(this, window.GetExecutionContext()) {}

H5vccMetrics::~H5vccMetrics() {
  DCHECK(h5vcc_metrics_promises_.empty());
}

void H5vccMetrics::ContextDestroyed() {
  OnCloseConnection();
}

ScriptPromise<IDLUndefined> H5vccMetrics::enable(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  h5vcc_metrics_promises_.insert(resolver);

  EnsureRemoteIsBound();

  remote_h5vcc_metrics_->Enable(
      /*enable=*/true,
      WTF::BindOnce(&H5vccMetrics::OnEnable, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise<IDLUndefined> H5vccMetrics::disable(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  h5vcc_metrics_promises_.insert(resolver);

  EnsureRemoteIsBound();

  remote_h5vcc_metrics_->Enable(
      /*enable=*/false,
      WTF::BindOnce(&H5vccMetrics::OnDisable, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

bool H5vccMetrics::isEnabled() {
  return is_reporting_enabled_;
}

ScriptPromise<IDLUndefined> H5vccMetrics::setMetricEventInterval(
    ScriptState* script_state,
    uint64_t interval_seconds,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  h5vcc_metrics_promises_.insert(resolver);

  EnsureRemoteIsBound();

  remote_h5vcc_metrics_->SetMetricEventInterval(
      interval_seconds,
      WTF::BindOnce(&H5vccMetrics::OnSetMetricEventInterval,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise<IDLString> H5vccMetrics::requestHistograms(
    ScriptState* script_state,
    bool monitor_mode,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(
      script_state, exception_state.GetContext());
  h5vcc_metrics_promises_.insert(resolver);

  EnsureRemoteIsBound();

  remote_h5vcc_metrics_->RequestHistograms(
      monitor_mode,
      WTF::BindOnce(&H5vccMetrics::OnRequestHistograms, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

void H5vccMetrics::OnMetrics(h5vcc_metrics::mojom::H5vccMetricType metric_type,
                             const WTF::String& metric_payload) {
  // Do not upload UMA payload if execution context is destroyed.
  if (!HasValidExecutionContext()) {
    return;
  }

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

  MaybeRegisterMojoListener();
}

void H5vccMetrics::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTarget::RemovedEventListener(event_type, registered_listener);
  MaybeUnregisterMojoListener();
}

void H5vccMetrics::MaybeUnregisterMojoListener() {
  DCHECK(receiver_.is_bound());
  if (!HasEventListeners(event_type_names::kMetrics)) {
    receiver_.reset();
  }
}

void H5vccMetrics::MaybeRegisterMojoListener() {
  DCHECK(HasEventListeners(event_type_names::kMetrics));
  if (receiver_.is_bound()) {
    return;
  }

  EnsureRemoteIsBound();

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  remote_h5vcc_metrics_->AddListener(
      receiver_.BindNewPipeAndPassRemote(task_runner));
}

void H5vccMetrics::OnEnable(ScriptPromiseResolver<IDLUndefined>* resolver) {
  CleanupPromise(resolver);
  is_reporting_enabled_ = true;
  resolver->Resolve();
}

void H5vccMetrics::OnDisable(ScriptPromiseResolver<IDLUndefined>* resolver) {
  CleanupPromise(resolver);
  is_reporting_enabled_ = false;
  resolver->Resolve();
}

void H5vccMetrics::OnSetMetricEventInterval(
    ScriptPromiseResolver<IDLUndefined>* resolver) {
  CleanupPromise(resolver);
  resolver->Resolve();
}

void H5vccMetrics::OnRequestHistograms(
    ScriptPromiseResolver<IDLString>* resolver,
    const WTF::String& histograms_proto_base64) {
  CleanupPromise(resolver);
  resolver->Resolve(histograms_proto_base64);
}

void H5vccMetrics::EnsureRemoteIsBound() {
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
}

void H5vccMetrics::OnCloseConnection() {
  remote_h5vcc_metrics_.reset();
  receiver_.reset();

  // If execution context is being destroyed, it is not safe to create a new
  // JS error object and reject promises.
  if (!HasValidExecutionContext()) {
    h5vcc_metrics_promises_.clear();
    return;
  }

  HeapHashSet<Member<ScriptPromiseResolverBase>> h5vcc_metrics_promises;
  // Script may execute during a call to Resolve(). Swap these sets to prevent
  // concurrent modification.
  h5vcc_metrics_promises_.swap(h5vcc_metrics_promises);
  for (ScriptPromiseResolverBase* resolver : h5vcc_metrics_promises) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kOperationError,
        "Mojo connection to Browser process was closed.");
  }
}

void H5vccMetrics::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(remote_h5vcc_metrics_);
  visitor->Trace(receiver_);
  visitor->Trace(h5vcc_metrics_promises_);
  EventTarget::Trace(visitor);
}

void H5vccMetrics::CleanupPromise(ScriptPromiseResolverBase* resolver) {
  DCHECK(h5vcc_metrics_promises_.Contains(resolver));
  h5vcc_metrics_promises_.erase(resolver);
}

bool H5vccMetrics::HasValidExecutionContext() {
  return GetExecutionContext() && !GetExecutionContext()->IsContextDestroyed();
}

}  // namespace blink
