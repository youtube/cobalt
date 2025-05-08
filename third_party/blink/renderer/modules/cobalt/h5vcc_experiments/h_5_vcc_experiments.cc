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
#include "cobalt/browser/cobalt_experiment_names.h"
#include "cobalt/browser/h5vcc_experiments/public/mojom/h5vcc_experiments.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_long_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_experiment_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_override_state.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

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

  if (!experiment_configuration->hasFeatures() ||
      !experiment_configuration->hasFeatureParams() ||
      !experiment_configuration->hasExperimentIds()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     "Incomplete experiment configuration.");
    return promise;
  }

  base::Value::Dict experiment_config_dict;

  base::Value::Dict features;
  for (auto& feature_name_and_value : experiment_configuration->features()) {
    features.Set(feature_name_and_value.first.Utf8(),
                 feature_name_and_value.second);
  }
  experiment_config_dict.Set(cobalt::kExperimentConfigFeatures,
                             std::move(features));

  // All FieldTrialParams are stored as strings, including booleans.
  base::Value::Dict feature_params;
  std::string param_value;
  for (auto& param_name_and_value : experiment_configuration->featureParams()) {
    if (param_name_and_value.second->IsString()) {
      param_value = param_name_and_value.second->GetAsString().Utf8();
    } else if (param_name_and_value.second->IsLong()) {
      param_value = std::to_string(param_name_and_value.second->GetAsLong());
    } else if (param_name_and_value.second->GetAsBoolean()) {
      param_value = "true";
    } else if (!param_name_and_value.second->GetAsBoolean()) {
      param_value = "false";
    } else {
      // TODO (b/416323107) - In thoery this never gets executed because
      // wrong types are implicitly converted. We need to enforce the typing
      // from the callsite.
      resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                       "Invalid feature param type.");
      return promise;
    }
    feature_params.Set(param_name_and_value.first.Utf8(), param_value);
  }
  experiment_config_dict.Set(cobalt::kExperimentConfigFeatureParams,
                             std::move(feature_params));

  base::Value::List experiment_ids;
  for (int exp_id : experiment_configuration->experimentIds()) {
    experiment_ids.Append(exp_id);
  }
  experiment_config_dict.Set(cobalt::kExperimentConfigExpIds,
                             std::move(experiment_ids));

  remote_h5vcc_experiments_->SetExperimentState(
      std::move(experiment_config_dict),
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
