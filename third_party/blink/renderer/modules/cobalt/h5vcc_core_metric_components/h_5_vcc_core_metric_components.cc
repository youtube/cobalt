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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_core_metric_components/h_5_vcc_core_metric_components.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

H5vccCoreMetricComponents::H5vccCoreMetricComponents(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_core_metric_components_(window.GetExecutionContext()) {}

void H5vccCoreMetricComponents::ContextDestroyed() {
  remote_h5vcc_core_metric_components_.reset();
}

ScriptPromise<IDLSequence<StabilityReport>>
H5vccCoreMetricComponents::getPendingReports(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  EnsureRemoteIsBound();
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<StabilityReport>>>(
      script_state, exception_state.GetContext());

  remote_h5vcc_core_metric_components_->GetPendingReports(WTF::BindOnce(
      [](ScriptPromiseResolver<IDLSequence<StabilityReport>>* resolver,
         Vector<
             h5vcc_core_metric_components::mojom::blink::StabilityReportPtr>
             mojo_reports) {
        HeapVector<Member<StabilityReport>> reports;
        for (const auto& mojo_report : mojo_reports) {
          auto* report = StabilityReport::Create();
          report->setCmcJoinUuid(mojo_report->cmc_join_uuid);
          report->setCreationTime(mojo_report->creation_time);

          switch (mojo_report->report_type) {
            case h5vcc_core_metric_components::mojom::blink::
                StabilityReportType::kCrash:
              report->setReportType(
                  V8StabilityReportType(V8StabilityReportType::Enum::kCrash));
              break;
            case h5vcc_core_metric_components::mojom::blink::
                StabilityReportType::kHangUnrecovered:
              report->setReportType(V8StabilityReportType(
                  V8StabilityReportType::Enum::kHangUnrecovered));
              break;
            case h5vcc_core_metric_components::mojom::blink::
                StabilityReportType::kHangRecovered:
              report->setReportType(V8StabilityReportType(
                  V8StabilityReportType::Enum::kHangRecovered));
              break;
          }

          reports.push_back(report);
        }
        resolver->Resolve(reports);
      },
      WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise<IDLUndefined> H5vccCoreMetricComponents::acknowledgeReports(
    ScriptState* script_state,
    const Vector<String>& acked_uuids,
    ExceptionState& exception_state) {
  EnsureRemoteIsBound();
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());

  remote_h5vcc_core_metric_components_->AcknowledgeReports(
      acked_uuids, WTF::BindOnce(
                       [](ScriptPromiseResolver<IDLUndefined>* resolver) {
                         resolver->Resolve();
                       },
                       WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise<IDLUndefined> H5vccCoreMetricComponents::markHangRecovered(
    ScriptState* script_state,
    const String& cmc_join_uuid,
    ExceptionState& exception_state) {
  EnsureRemoteIsBound();
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());

  remote_h5vcc_core_metric_components_->MarkHangRecovered(
      cmc_join_uuid,
      WTF::BindOnce(
          [](ScriptPromiseResolver<IDLUndefined>* resolver) {
            resolver->Resolve();
          },
          WrapPersistent(resolver)));

  return resolver->Promise();
}

void H5vccCoreMetricComponents::EnsureRemoteIsBound() {
  DCHECK(GetExecutionContext());
  if (remote_h5vcc_core_metric_components_.is_bound()) {
    return;
  }
  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_h5vcc_core_metric_components_.BindNewPipeAndPassReceiver(task_runner));
}

void H5vccCoreMetricComponents::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(remote_h5vcc_core_metric_components_);
}

}  // namespace blink
