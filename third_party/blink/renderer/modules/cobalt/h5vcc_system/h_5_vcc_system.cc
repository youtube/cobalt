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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_system/h_5_vcc_system.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_function.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_h_5_vcc_system_conceal_or_stop_params.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

H5vccSystem::H5vccSystem(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_system_(window.GetExecutionContext()) {}

void H5vccSystem::ContextDestroyed() {}

ScriptPromise H5vccSystem::getAdvertisingId(ScriptState* script_state,
                                            ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  remote_h5vcc_system_->GetAdvertisingId(
      WTF::BindOnce(&H5vccSystem::OnGetAdvertisingId, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

void H5vccSystem::OnGetAdvertisingId(ScriptPromiseResolver* resolver,
                                     const String& result) {
  resolver->Resolve(result);
}

ScriptPromise H5vccSystem::getLimitAdTracking(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  remote_h5vcc_system_->GetLimitAdTracking(
      WTF::BindOnce(&H5vccSystem::OnGetLimitAdTracking, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

void H5vccSystem::OnGetLimitAdTracking(ScriptPromiseResolver* resolver,
                                       bool result) {
  resolver->Resolve(result);
}

ScriptPromise H5vccSystem::concealOrStop(
    ScriptState* script_state,
    const H5vccSystemConcealOrStopParams* params,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  EnsureReceiverIsBound();

  remote_h5vcc_system_->GetUserOnExitStrategy(
      WTF::BindOnce(&H5vccSystem::OnGetUserOnExitStrategy, WrapPersistent(this),
                    WrapPersistent(resolver), WrapPersistent(params)));
  return resolver->Promise();
}

ScriptPromise H5vccSystem::concealOrStop(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  return concealOrStop(script_state, /*callbacks=*/nullptr, exception_state);
}

void H5vccSystem::OnGetUserOnExitStrategy(
    ScriptPromiseResolver* resolver,
    const H5vccSystemConcealOrStopParams* params,
    h5vcc_system::mojom::blink::UserOnExitStrategy result) {
  if (result == h5vcc_system::mojom::blink::UserOnExitStrategy::kNoExit) {
    resolver->Resolve(/*concealedOrStopped=*/false);
    return;
  }

  if (result == h5vcc_system::mojom::blink::UserOnExitStrategy::kConceal &&
      params && params->hasBeforeConceal()) {
    params->beforeConceal()->InvokeAndReportException(this);
  }

  if (result == h5vcc_system::mojom::blink::UserOnExitStrategy::kStop &&
      params && params->hasBeforeStop()) {
    params->beforeStop()->InvokeAndReportException(this);
  }
  remote_h5vcc_system_->ConcealOrStop(WTF::BindOnce(
      &ScriptPromiseResolver::Resolve<bool>, WrapPersistent(resolver),
      /*concealedOrStopped=*/true));
}

void H5vccSystem::EnsureReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_h5vcc_system_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_h5vcc_system_.BindNewPipeAndPassReceiver(task_runner));
}

void H5vccSystem::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(remote_h5vcc_system_);
}

}  // namespace blink
