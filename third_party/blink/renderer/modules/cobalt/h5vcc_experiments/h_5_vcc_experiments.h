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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_H5VCC_EXPERIMENTS_H_5_VCC_EXPERIMENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_H5VCC_EXPERIMENTS_H_5_VCC_EXPERIMENTS_H_

#include "cobalt/browser/h5vcc_experiments/public/mojom/h5vcc_experiments.mojom-blink.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_experiment_configuration.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class LocalDOMWindow;
class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT H5vccExperiments final
    : public ScriptWrappable,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit H5vccExperiments(LocalDOMWindow&);

  void ContextDestroyed() override;

  // Web-exposed interface:
  ScriptPromise setExperimentState(ScriptState*,
                                   const ExperimentConfiguration*,
                                   ExceptionState&);
  ScriptPromise resetExperimentState(ScriptState*, ExceptionState&);
  WTF::Vector<uint32_t> activeExperimentIds();
  String getFeature(const String&);
  const String& getFeatureParam(const String&);

  void Trace(Visitor*) const override;

 private:
  void OnSetExperimentState(ScriptPromiseResolver*);
  void OnResetExperimentState(ScriptPromiseResolver*);
  void OnConnectionError();
  void EnsureReceiverIsBound();
  HeapMojoRemote<h5vcc_experiments::mojom::blink::H5vccExperiments>
      remote_h5vcc_experiments_;

  String feature_param_value_;
  WTF::Vector<uint32_t> active_experiment_ids_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_H5VCC_EXPERIMENTS_H_5_VCC_EXPERIMENTS_H_
