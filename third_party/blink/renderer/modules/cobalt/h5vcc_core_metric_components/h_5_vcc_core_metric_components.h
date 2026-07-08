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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_CORE_METRIC_COMPONENTS_H_5_VCC_CORE_METRIC_COMPONENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_CORE_METRIC_COMPONENTS_H_5_VCC_CORE_METRIC_COMPONENTS_H_

#include "cobalt/browser/h5vcc_core_metric_components/public/mojom/h5vcc_core_metric_components.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_stability_report.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_stability_report_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExecutionContext;
class LocalDOMWindow;
class ScriptState;

class MODULES_EXPORT H5vccCoreMetricComponents final
    : public ScriptWrappable,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit H5vccCoreMetricComponents(LocalDOMWindow&);
  ~H5vccCoreMetricComponents() override = default;

  void ContextDestroyed() override;

  // Web-exposed interface:
  ScriptPromise<IDLSequence<StabilityReport>> getPendingReports(ScriptState*, ExceptionState&);
  ScriptPromise<IDLUndefined> acknowledgeReports(ScriptState*, const Vector<String>& acked_uuids, ExceptionState&);
  ScriptPromise<IDLUndefined> markHangRecovered(ScriptState*, const String& cmc_join_uuid, ExceptionState&);

  void Trace(Visitor*) const override;

 private:
  void EnsureRemoteIsBound();

  HeapMojoRemote<h5vcc_core_metric_components::mojom::blink::H5vccCoreMetricComponents>
      remote_h5vcc_core_metric_components_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_CORE_METRIC_COMPONENTS_H_5_VCC_CORE_METRIC_COMPONENTS_H_
