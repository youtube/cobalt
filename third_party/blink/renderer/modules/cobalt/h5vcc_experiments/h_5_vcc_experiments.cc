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
#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_double_long_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_experiment_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_override_state.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_experiments/experiments_utils.h"

namespace blink {

H5vccExperiments::H5vccExperiments(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_experiments_(window.GetExecutionContext()) {}

void H5vccExperiments::ContextDestroyed() {}

ScriptPromise<IDLUndefined> H5vccExperiments::setExperimentState(
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

  ongoing_requests_.insert(resolver);
  remote_h5vcc_experiments_->SetExperimentState(
      std::move(experiment_config_dict.value()),
      WTF::BindOnce(&H5vccExperiments::OnSetExperimentState,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

ScriptPromise<IDLUndefined> H5vccExperiments::resetExperimentState(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  ongoing_requests_.insert(resolver);
  remote_h5vcc_experiments_->ResetExperimentState(
      WTF::BindOnce(&H5vccExperiments::OnResetExperimentState,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
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

ScriptPromise<IDLString> H5vccExperiments::getActiveExperimentConfigData(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  ongoing_requests_.insert(resolver);
  remote_h5vcc_experiments_->GetActiveExperimentConfigData(
      WTF::BindOnce(&H5vccExperiments::OnGetActiveExperimentConfigData,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise<IDLString> H5vccExperiments::getLatestExperimentConfigHashData(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  ongoing_requests_.insert(resolver);
  remote_h5vcc_experiments_->GetLatestExperimentConfigHashData(
      WTF::BindOnce(&H5vccExperiments::OnGetLatestExperimentConfigHashData,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise<IDLUndefined> H5vccExperiments::setLatestExperimentConfigHashData(
    ScriptState* script_state,
    const String& hash_data,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  ongoing_requests_.insert(resolver);
  remote_h5vcc_experiments_->SetLatestExperimentConfigHashData(
      hash_data,
      WTF::BindOnce(&H5vccExperiments::OnSetLatestExperimentConfigHashData,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise<IDLUndefined> H5vccExperiments::setFinchParameters(
    ScriptState* script_state,
    const HeapVector<
        std::pair<WTF::String, Member<V8UnionBooleanOrDoubleOrLongOrString>>>&
        settings,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  EnsureReceiverIsBound();

  std::optional<base::Value::Dict> settings_dict =
      ParseSettingsToDictionary(settings);

  if (!settings_dict.has_value()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     "Unable to parse settings.");
    return promise;
  }

  ongoing_requests_.insert(resolver);
  remote_h5vcc_experiments_->SetFinchParameters(
      std::move(settings_dict.value()),
      WTF::BindOnce(&H5vccExperiments::OnSetFinchParameters,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void H5vccExperiments::OnGetActiveExperimentConfigData(
    ScriptPromiseResolver* resolver,
    const String& result) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve(result);
}

void H5vccExperiments::OnGetLatestExperimentConfigHashData(
    ScriptPromiseResolver* resolver,
    const String& result) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve(result);
}

void H5vccExperiments::OnSetExperimentState(ScriptPromiseResolver* resolver) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve();
}

void H5vccExperiments::OnSetFinchParameters(ScriptPromiseResolver* resolver) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve();
}

void H5vccExperiments::OnSetLatestExperimentConfigHashData(
    ScriptPromiseResolver* resolver) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve();
}

void H5vccExperiments::OnResetExperimentState(ScriptPromiseResolver* resolver) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve();
}

void H5vccExperiments::OnConnectionError() {
  remote_h5vcc_experiments_.reset();
  HeapHashSet<Member<ScriptPromiseResolver>> h5vcc_experiments_promises;
  // Script may execute during a call to Resolve(). Swap these sets to prevent
  // concurrent modification.
  ongoing_requests_.swap(h5vcc_experiments_promises);
  for (auto& resolver : h5vcc_experiments_promises) {
    resolver->Reject("Mojo connection error.");
  }
  ongoing_requests_.clear();
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
  remote_h5vcc_experiments_.set_disconnect_handler(WTF::BindOnce(
      &H5vccExperiments::OnConnectionError, WrapWeakPersistent(this)));
}

void H5vccExperiments::Trace(Visitor* visitor) const {
  visitor->Trace(remote_h5vcc_experiments_);
  visitor->Trace(ongoing_requests_);
  ExecutionContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
