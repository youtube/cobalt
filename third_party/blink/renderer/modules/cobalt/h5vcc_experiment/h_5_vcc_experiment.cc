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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_experiment/h_5_vcc_experiment.h"

#include "cobalt/browser/h5vcc_experiment/public/mojom/h5vcc_experiment.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

H5vccExperiment::H5vccExperiment(LocalDOMWindow& window) {}

void H5vccExperiment::setExperimentState(
    std::string experiment_state_map) const {
  EnsureReceiverIsBound();

  remote_h5vcc_experiment_->SetExperimentState(
      std::string experiment_state_map);
}

ScriptPromise H5vccExperiment::getFeatureValue(
    ScriptState* script_state,
    std::string feature_name,
    std::string feature_param_name,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  remote_h5vcc_experiment_->GetFeatureValue(
      feature_name, feature_param_name,
      WTF::BindOnce(&H5vccExperiment::OnGetFeatureValue, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

void H5vccSystem::OnGetFeatureValue(ScriptPromiseResolver* resolver,
                                    const String& result) {
  resolver->Resolve(result);
}

void H5vccExperiment::EnsureReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_h5vcc_experiment_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_h5vcc_experiment_.BindNewPipeAndPassReceiver(task_runner));
}

void H5vccExperiment::Trace(Visitor* visitor) const {
  visitor->Trace(remote_h5vcc_experiment_);
  ExecutionContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
