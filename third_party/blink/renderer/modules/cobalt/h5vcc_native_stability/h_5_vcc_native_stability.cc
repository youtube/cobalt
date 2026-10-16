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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_native_stability/h_5_vcc_native_stability.h"

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_hang_report.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_native_crash_report.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_native_stability_report_type.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

H5vccNativeStability::H5vccNativeStability(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_native_stability_(window.GetExecutionContext()) {}

void H5vccNativeStability::ContextDestroyed() {
  remote_native_stability_.reset();
}

ScriptPromise<IDLSequence<V8NativeStabilityReport>>
H5vccNativeStability::getPendingReports(ScriptState* script_state,
                                        ExceptionState& exception_state) {
  EnsureReceiverIsBound();
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<V8NativeStabilityReport>>>(
      script_state, exception_state.GetContext());

  ongoing_requests_.insert(resolver);
  remote_native_stability_->GetPendingReports(
      WTF::BindOnce(&H5vccNativeStability::OnGetPendingReports,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise<IDLUndefined> H5vccNativeStability::acknowledgeReports(
    ScriptState* script_state,
    const Vector<String>& native_stability_event_uuids,
    ExceptionState& exception_state) {
  EnsureReceiverIsBound();
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());

  ongoing_requests_.insert(resolver);
  remote_native_stability_->AcknowledgeReports(
      native_stability_event_uuids,
      WTF::BindOnce(&H5vccNativeStability::OnAcknowledgeReports,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
}

namespace {
template <typename T>
void PopulateBaseReport(
    T* report,
    const h5vcc_native_stability::mojom::blink::BaseReportDataPtr& base,
    V8NativeStabilityReportType::Enum report_type_enum) {
  CHECK(base);
  report->setNativeStabilityEventUuid(base->native_stability_event_uuid);
  report->setEventTimeSec(base->event_time_sec);
  report->setReportType(V8NativeStabilityReportType(report_type_enum));
}
}  // namespace

void H5vccNativeStability::OnGetPendingReports(
    ScriptPromiseResolver<IDLSequence<V8NativeStabilityReport>>* resolver,
    Vector<h5vcc_native_stability::mojom::blink::NativeStabilityReportPtr>
        mojo_reports) {
  ongoing_requests_.erase(resolver);
  HeapVector<Member<V8NativeStabilityReport>> result;
  for (const auto& mojo_report : mojo_reports) {
    if (mojo_report->is_crash_report()) {
      const auto& mojo_crash_report = mojo_report->get_crash_report();
      auto* blink_crash_report = NativeCrashReport::Create();
      PopulateBaseReport(blink_crash_report, mojo_crash_report->base,
                         V8NativeStabilityReportType::Enum::kNativeCrash);
      result.push_back(
          MakeGarbageCollected<V8NativeStabilityReport>(blink_crash_report));
    } else if (mojo_report->is_hang_report()) {
      const auto& mojo_hang_report = mojo_report->get_hang_report();
      auto* blink_hang_report = HangReport::Create();
      PopulateBaseReport(blink_hang_report, mojo_hang_report->base,
                         V8NativeStabilityReportType::Enum::kHang);
      blink_hang_report->setIsRecovered(mojo_hang_report->is_recovered);
      result.push_back(
          MakeGarbageCollected<V8NativeStabilityReport>(blink_hang_report));
    }
  }
  resolver->Resolve(std::move(result));
}

void H5vccNativeStability::OnAcknowledgeReports(
    ScriptPromiseResolver<IDLUndefined>* resolver) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve();
}

void H5vccNativeStability::EnsureReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_native_stability_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_native_stability_.BindNewPipeAndPassReceiver(task_runner));
  remote_native_stability_.set_disconnect_handler(WTF::BindOnce(
      &H5vccNativeStability::OnConnectionError, WrapWeakPersistent(this)));
}

void H5vccNativeStability::OnConnectionError() {
  remote_native_stability_.reset();
  HeapHashSet<Member<ScriptPromiseResolverBase>> pending_promises;
  // Script may execute during a call to Reject(). Swap these sets to prevent
  // concurrent modification.
  ongoing_requests_.swap(pending_promises);
  for (auto& resolver : pending_promises) {
    resolver->Reject("Mojo connection error.");
  }
}

void H5vccNativeStability::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(ongoing_requests_);
  visitor->Trace(remote_native_stability_);
}

}  // namespace blink
