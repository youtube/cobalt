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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_experiments/h_5_vcc_experiments.h"

#include "cobalt/browser/h5vcc_experiments/public/mojom/h5vcc_experiments.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

H5vccExperiments::H5vccExperiments(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_experiments_(window.GetExecutionContext()) {}

void H5vccExperiments::ContextDestroyed() {}

ScriptPromise H5vccExperiments::setExperimentState(
    ScriptState* script_state,
    const String& experiment_state_map,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  remote_h5vcc_experiments_->SetExperimentState(
      experiment_state_map,
      WTF::BindOnce(&H5vccExperiments::OnSetExperimentState,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
}

void H5vccExperiments::OnSetExperimentState(ScriptPromiseResolver* resolver) {
  // TODO: Reject when this fails.
  resolver->Resolve();
}

bool H5vccExperiments::getFeature(const String& feature_name) {
  bool result;
  EnsureReceiverIsBound();
  remote_h5vcc_experiments_->GetFeature(feature_name, &result);

  return result;
}

void H5vccExperiments::OnGetFeature(ScriptPromiseResolver* resolver,
                                    bool result) {
  resolver->Resolve(result);
}

const String& H5vccExperiments::getFeatureParam(
    const String& feature_param_name) {
  EnsureReceiverIsBound();

  String result;

  remote_h5vcc_experiments_->GetFeatureParam(feature_param_name, &result);

  return result;
}

void H5vccExperiments::OnGetFeatureParam(ScriptPromiseResolver* resolver,
                                         const String& result) {
  resolver->Resolve(result);
}

ScriptPromise H5vccExperiments::resetExperimentState(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  remote_h5vcc_experiments_->ResetExperimentState(
      WTF::BindOnce(&H5vccExperiments::OnResetExperimentState,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
}

void H5vccExperiments::OnResetExperimentState(ScriptPromiseResolver* resolver) {
  // TODO: Reject when this fails.
  resolver->Resolve();
}

void H5vccExperiments::EnsureReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_h5vcc_experiments_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_h5vcc_experiments_.BindNewPipeAndPassReceiver(task_runner));
}

void H5vccExperiments::Trace(Visitor* visitor) const {
  visitor->Trace(remote_h5vcc_experiments_);
  ExecutionContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
