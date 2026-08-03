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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_NATIVE_STABILITY_H_5_VCC_NATIVE_STABILITY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_NATIVE_STABILITY_H_5_VCC_NATIVE_STABILITY_H_

#include "cobalt/browser/h5vcc_native_stability/public/mojom/h5vcc_native_stability.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_hangreport_nativecrashreport.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class ExecutionContext;
class LocalDOMWindow;
class ScriptState;
class ScriptPromiseResolverBase;
template <typename T>
class ScriptPromiseResolver;

using V8NativeStabilityReport = V8UnionHangReportOrNativeCrashReport;

class MODULES_EXPORT H5vccNativeStability final
    : public ScriptWrappable,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit H5vccNativeStability(LocalDOMWindow&);

  void ContextDestroyed() override;

  // Web-exposed interface:
  ScriptPromise<IDLSequence<V8NativeStabilityReport>> getPendingReports(
      ScriptState*,
      ExceptionState&);

  ScriptPromise<IDLUndefined> acknowledgeReports(
      ScriptState*,
      const Vector<String>& native_stability_event_uuids,
      ExceptionState&);

  void Trace(Visitor*) const override;

 private:
  void EnsureReceiverIsBound();
  void OnConnectionError();

  void OnGetPendingReports(
      ScriptPromiseResolver<IDLSequence<V8NativeStabilityReport>>* resolver,
      Vector<h5vcc_native_stability::mojom::blink::NativeStabilityReportPtr>
          mojo_reports);

  void OnAcknowledgeReports(ScriptPromiseResolver<IDLUndefined>* resolver);

  HeapHashSet<Member<ScriptPromiseResolverBase>> ongoing_requests_;

  HeapMojoRemote<h5vcc_native_stability::mojom::blink::H5vccNativeStability>
      remote_native_stability_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_NATIVE_STABILITY_H_5_VCC_NATIVE_STABILITY_H_
