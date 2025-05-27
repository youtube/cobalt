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

#include "base/values.h"
#include "cobalt/browser/constants/cobalt_experiment_names.h"
#include "cobalt/browser/h5vcc_experiments/public/mojom/h5vcc_experiments.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_long_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_experiment_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_override_state.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_experiments/experiments_utils.h"

namespace blink {

H5vccExperiments::H5vccExperiments(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_experiments_(window.GetExecutionContext()) {}

void H5vccExperiments::ContextDestroyed() {}

ScriptPromise H5vccExperiments::setExperimentState(
    ScriptState* script_state,
    const ExperimentConfiguration* experiment_configuration,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  EnsureReceiverIsBound();

  std::optional<base::Value::Dict> experiment_config_dict =
      ParseConfigToDictionary(experiment_configuration);

  if (!experiment_config_dict.has_value()) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Unable to parse experiment configuration.");
    return promise;
  }

  remote_h5vcc_experiments_->SetExperimentState(
      std::move(experiment_config_dict.value()),
      WTF::BindOnce(&H5vccExperiments::OnSetExperimentState,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
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

WTF::Vector<uint32_t> H5vccExperiments::activeExperimentIds() {
  EnsureReceiverIsBound();
  remote_h5vcc_experiments_->GetActiveExperimentIds(&active_experiment_ids_);
  return active_experiment_ids_;
}

String H5vccExperiments::getFeature(const String& feature_name) {
  EnsureReceiverIsBound();
  h5vcc_experiments::mojom::blink::OverrideState feature_state;
  remote_h5vcc_experiments_->GetFeature(feature_name, &feature_state);
  switch (feature_state) {
    case h5vcc_experiments::mojom::blink::OverrideState::OVERRIDE_USE_DEFAULT:
      return V8OverrideState(V8OverrideState::Enum::kDEFAULT);
    case h5vcc_experiments::mojom::blink::OverrideState::
        OVERRIDE_ENABLE_FEATURE:
      return V8OverrideState(V8OverrideState::Enum::kENABLED);
    case h5vcc_experiments::mojom::blink::OverrideState::
        OVERRIDE_DISABLE_FEATURE:
      return V8OverrideState(V8OverrideState::Enum::kDISABLED);
  }
  NOTREACHED_NORETURN() << "Invalid feature OverrideState for feature "
                        << feature_name;
}

const String& H5vccExperiments::getFeatureParam(
    const String& feature_param_name) {
  EnsureReceiverIsBound();
  remote_h5vcc_experiments_->GetFeatureParam(feature_param_name,
                                             &feature_param_value_);
  return feature_param_value_;
}

void H5vccExperiments::OnSetExperimentState(ScriptPromiseResolver* resolver) {
  resolver->Resolve();
}

void H5vccExperiments::OnResetExperimentState(ScriptPromiseResolver* resolver) {
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
