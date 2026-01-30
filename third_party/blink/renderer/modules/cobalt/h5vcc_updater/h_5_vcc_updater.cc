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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_updater/h_5_vcc_updater.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/event_interface_modules_names.h"

namespace blink {

H5vccUpdater::H5vccUpdater(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_updater_(window.GetExecutionContext()),
      remote_h5vcc_updater_sideloading_(window.GetExecutionContext()) {}

void H5vccUpdater::ContextDestroyed() {}

ScriptPromise H5vccUpdater::getUpdaterChannel(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  ongoing_requests_.insert(resolver);
  remote_h5vcc_updater_->GetUpdaterChannel(
      WTF::BindOnce(&H5vccUpdater::OnGetUpdaterChannel, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise H5vccUpdater::setUpdaterChannel(ScriptState* script_state,
                                              const String& channel,
                                              ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  ongoing_requests_.insert(resolver);
  remote_h5vcc_updater_->SetUpdaterChannel(
      channel, WTF::BindOnce(&H5vccUpdater::OnSetUpdaterChannel,
                             WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise H5vccUpdater::getUpdateStatus(ScriptState* script_state,
                                            ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  ongoing_requests_.insert(resolver);
  remote_h5vcc_updater_->GetUpdateStatus(
      WTF::BindOnce(&H5vccUpdater::OnGetUpdateStatus, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise H5vccUpdater::resetInstallations(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  ongoing_requests_.insert(resolver);
  remote_h5vcc_updater_->ResetInstallations(
      WTF::BindOnce(&H5vccUpdater::OnResetInstallations, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise H5vccUpdater::getInstallationIndex(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  ongoing_requests_.insert(resolver);
  remote_h5vcc_updater_->GetInstallationIndex(
      WTF::BindOnce(&H5vccUpdater::OnGetInstallationIndex, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise H5vccUpdater::getAllowSelfSignedPackages(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  ongoing_sideloading_requests_.insert(resolver);
  remote_h5vcc_updater_sideloading_->GetAllowSelfSignedPackages(
      WTF::BindOnce(&H5vccUpdater::OnGetAllowSelfSignedPackages,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise H5vccUpdater::setAllowSelfSignedPackages(
    ScriptState* script_state,
    bool allow_self_signed_packages,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureSideloadingReceiverIsBound();

  ongoing_sideloading_requests_.insert(resolver);
  remote_h5vcc_updater_sideloading_->SetAllowSelfSignedPackages(
      allow_self_signed_packages,
      WTF::BindOnce(&H5vccUpdater::OnSetAllowSelfSignedPackages,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise H5vccUpdater::getUpdateServerUrl(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  ongoing_sideloading_requests_.insert(resolver);
  remote_h5vcc_updater_sideloading_->GetUpdateServerUrl(
      WTF::BindOnce(&H5vccUpdater::OnGetUpdateServerUrl, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise H5vccUpdater::setUpdateServerUrl(
    ScriptState* script_state,
    const String& update_server_url,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureSideloadingReceiverIsBound();

  ongoing_sideloading_requests_.insert(resolver);
  remote_h5vcc_updater_sideloading_->SetUpdateServerUrl(
      update_server_url,
      WTF::BindOnce(&H5vccUpdater::OnSetUpdateServerUrl, WrapPersistent(this),
                    WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise H5vccUpdater::getRequireNetworkEncryption(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  ongoing_sideloading_requests_.insert(resolver);
  remote_h5vcc_updater_sideloading_->GetRequireNetworkEncryption(
      WTF::BindOnce(&H5vccUpdater::OnGetRequireNetworkEncryption,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise H5vccUpdater::setRequireNetworkEncryption(
    ScriptState* script_state,
    bool require_network_encryption,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureSideloadingReceiverIsBound();

  ongoing_sideloading_requests_.insert(resolver);
  remote_h5vcc_updater_sideloading_->SetRequireNetworkEncryption(
      require_network_encryption,
      WTF::BindOnce(&H5vccUpdater::OnSetRequireNetworkEncryption,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise H5vccUpdater::getLibrarySha256(ScriptState* script_state,
                                             unsigned short index,
                                             ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  ongoing_requests_.insert(resolver);
  remote_h5vcc_updater_->GetLibrarySha256(
      index, WTF::BindOnce(&H5vccUpdater::OnGetLibrarySha256,
                           WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
}

void H5vccUpdater::OnGetUpdaterChannel(ScriptPromiseResolver* resolver,
                                       const String& result) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve(result);
}

void H5vccUpdater::OnSetUpdaterChannel(ScriptPromiseResolver* resolver) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve();
}

void H5vccUpdater::OnGetUpdateStatus(ScriptPromiseResolver* resolver,
                                     const String& result) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve(result);
}

void H5vccUpdater::OnResetInstallations(ScriptPromiseResolver* resolver) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve();
}

void H5vccUpdater::OnGetInstallationIndex(ScriptPromiseResolver* resolver,
                                          uint16_t result) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve(result);
}

void H5vccUpdater::OnGetLibrarySha256(ScriptPromiseResolver* resolver,
                                      const String& result) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve(result);
}

void H5vccUpdater::OnGetAllowSelfSignedPackages(ScriptPromiseResolver* resolver,
                                                bool result) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve(result);
}

void H5vccUpdater::OnSetAllowSelfSignedPackages(
    ScriptPromiseResolver* resolver) {
  ongoing_sideloading_requests_.erase(resolver);
  resolver->Resolve();
}

void H5vccUpdater::OnGetUpdateServerUrl(ScriptPromiseResolver* resolver,
                                        const String& result) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve(result);
}

void H5vccUpdater::OnSetUpdateServerUrl(ScriptPromiseResolver* resolver) {
  ongoing_sideloading_requests_.erase(resolver);
  resolver->Resolve();
}

void H5vccUpdater::OnGetRequireNetworkEncryption(
    ScriptPromiseResolver* resolver,
    bool result) {
  ongoing_requests_.erase(resolver);
  resolver->Resolve(result);
}

void H5vccUpdater::OnSetRequireNetworkEncryption(
    ScriptPromiseResolver* resolver) {
  ongoing_sideloading_requests_.erase(resolver);
  resolver->Resolve();
}

void H5vccUpdater::EnsureReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_h5vcc_updater_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_h5vcc_updater_.BindNewPipeAndPassReceiver(task_runner));
  remote_h5vcc_updater_.set_disconnect_handler(WTF::BindOnce(
      &H5vccUpdater::OnConnectionError, WrapWeakPersistent(this)));
}

void H5vccUpdater::OnConnectionError() {
  remote_h5vcc_updater_.reset();
  HeapHashSet<Member<ScriptPromiseResolver>> h5vcc_updater_promises;
  // Script may execute during a call to Reject(). Swap these sets to prevent
  // concurrent modification.
  ongoing_requests_.swap(h5vcc_updater_promises);
  for (auto& resolver : h5vcc_updater_promises) {
#if BUILDFLAG(USE_EVERGREEN)
    resolver->Reject("Mojo connection error.");
#else
    resolver->Reject("API not supported for this platform.");
#endif
  }
  ongoing_requests_.clear();
}

void H5vccUpdater::EnsureSideloadingReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_h5vcc_updater_sideloading_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_h5vcc_updater_sideloading_.BindNewPipeAndPassReceiver(
          task_runner));
  remote_h5vcc_updater_sideloading_.set_disconnect_handler(WTF::BindOnce(
      &H5vccUpdater::OnSideloadingConnectionError, WrapWeakPersistent(this)));
}

void H5vccUpdater::OnSideloadingConnectionError() {
  remote_h5vcc_updater_sideloading_.reset();
  HeapHashSet<Member<ScriptPromiseResolver>> h5vcc_updater_promises;
  // Script may execute during a call to Reject(). Swap these sets to prevent
  // concurrent modification.
  ongoing_sideloading_requests_.swap(h5vcc_updater_promises);
  for (auto& resolver : h5vcc_updater_promises) {
// TODO(b/458483469): Remove the ALLOW_EVERGREEN_SIDELOADING check after
// security review.
#if BUILDFLAG(USE_EVERGREEN) && !BUILDFLAG(COBALT_IS_RELEASE_BUILD) && \
    ALLOW_EVERGREEN_SIDELOADING
    resolver->Reject("Mojo connection error.");
#else
    resolver->Reject(
        "API not supported for this build configuration. Enabled for "
        "sideloading only.");
#endif
  }
  ongoing_sideloading_requests_.clear();
}

void H5vccUpdater::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(ongoing_requests_);
  visitor->Trace(ongoing_sideloading_requests_);
  visitor->Trace(remote_h5vcc_updater_);
  visitor->Trace(remote_h5vcc_updater_sideloading_);
}

}  // namespace blink
