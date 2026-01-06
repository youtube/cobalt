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
    : ExecutionContextLifecycleObserver(window.GetExecutionContext())
#if BUILDFLAG(USE_EVERGREEN)
      ,
      remote_h5vcc_updater_(window.GetExecutionContext())
#endif
{
}

void H5vccUpdater::ContextDestroyed() {}

ScriptPromise H5vccUpdater::getUpdaterChannel(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

#if BUILDFLAG(USE_EVERGREEN)
  EnsureReceiverIsBound();

  remote_h5vcc_updater_->GetUpdaterChannel(
      WTF::BindOnce(&H5vccUpdater::OnGetUpdaterChannel, WrapPersistent(this),
                    WrapPersistent(resolver)));
#else
  resolver->Reject();
#endif
  return resolver->Promise();
}

ScriptPromise H5vccUpdater::setUpdaterChannel(ScriptState* script_state,
                                              const String& channel,
                                              ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

#if BUILDFLAG(USE_EVERGREEN)
  EnsureReceiverIsBound();

  remote_h5vcc_updater_->SetUpdaterChannel(
      channel, WTF::BindOnce(&H5vccUpdater::OnVoidResult, WrapPersistent(this),
                             WrapPersistent(resolver)));
#else
  resolver->Reject();
#endif
  return resolver->Promise();
}

ScriptPromise H5vccUpdater::getUpdateStatus(ScriptState* script_state,
                                            ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

#if BUILDFLAG(USE_EVERGREEN)
  EnsureReceiverIsBound();

  remote_h5vcc_updater_->GetUpdateStatus(
      WTF::BindOnce(&H5vccUpdater::OnGetUpdateStatus, WrapPersistent(this),
                    WrapPersistent(resolver)));
#else
  resolver->Reject();
#endif
  return resolver->Promise();
}

ScriptPromise H5vccUpdater::resetInstallations(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

#if BUILDFLAG(USE_EVERGREEN)
  EnsureReceiverIsBound();

  remote_h5vcc_updater_->ResetInstallations();
  resolver->Resolve();
#else
  resolver->Reject();
#endif
  return resolver->Promise();
}

ScriptPromise H5vccUpdater::getInstallationIndex(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

#if BUILDFLAG(USE_EVERGREEN)
  EnsureReceiverIsBound();

  remote_h5vcc_updater_->GetInstallationIndex(
      WTF::BindOnce(&H5vccUpdater::OnGetInstallationIndex, WrapPersistent(this),
                    WrapPersistent(resolver)));
#else
  resolver->Reject();
#endif
  return resolver->Promise();
}

ScriptPromise H5vccUpdater::getAllowSelfSignedPackages(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

#if BUILDFLAG(USE_EVERGREEN)
  EnsureReceiverIsBound();

  remote_h5vcc_updater_->GetAllowSelfSignedPackages(
      WTF::BindOnce(&H5vccUpdater::OnGetBool, WrapPersistent(this),
                    WrapPersistent(resolver)));
#else
  resolver->Reject();
#endif
  return resolver->Promise();
}

ScriptPromise H5vccUpdater::setAllowSelfSignedPackages(
    ScriptState* script_state,
    bool allow_self_signed_packages,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

#if BUILDFLAG(USE_EVERGREEN)
  EnsureReceiverIsBound();

  remote_h5vcc_updater_->SetAllowSelfSignedPackages(allow_self_signed_packages);
  resolver->Resolve();
#else
  resolver->Reject();
#endif
  return resolver->Promise();
}

ScriptPromise H5vccUpdater::getUpdateServerUrl(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

#if BUILDFLAG(USE_EVERGREEN)
  EnsureReceiverIsBound();

  remote_h5vcc_updater_->GetUpdateServerUrl(
      WTF::BindOnce(&H5vccUpdater::OnGetUpdateServerUrl, WrapPersistent(this),
                    WrapPersistent(resolver)));
#else
  resolver->Reject();
#endif
  return resolver->Promise();
}

ScriptPromise H5vccUpdater::setUpdateServerUrl(
    ScriptState* script_state,
    const String& update_server_url,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

#if BUILDFLAG(USE_EVERGREEN)
  EnsureReceiverIsBound();

  remote_h5vcc_updater_->SetUpdateServerUrl(update_server_url);
  resolver->Resolve();
#else
  resolver->Reject();
#endif
  return resolver->Promise();
}

ScriptPromise H5vccUpdater::getRequireNetworkEncryption(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

#if BUILDFLAG(USE_EVERGREEN)
  EnsureReceiverIsBound();

  remote_h5vcc_updater_->GetRequireNetworkEncryption(
      WTF::BindOnce(&H5vccUpdater::OnGetBool, WrapPersistent(this),
                    WrapPersistent(resolver)));
#else
  resolver->Reject();
#endif
  return resolver->Promise();
}

ScriptPromise H5vccUpdater::setRequireNetworkEncryption(
    ScriptState* script_state,
    bool require_network_encryption,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

#if BUILDFLAG(USE_EVERGREEN)
  EnsureReceiverIsBound();

  remote_h5vcc_updater_->SetRequireNetworkEncryption(
      require_network_encryption);
  resolver->Resolve();
#else
  resolver->Reject();
#endif
  return resolver->Promise();
}

ScriptPromise H5vccUpdater::getLibrarySha256(ScriptState* script_state,
                                             unsigned short index,
                                             ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

#if BUILDFLAG(USE_EVERGREEN)
  EnsureReceiverIsBound();

  remote_h5vcc_updater_->GetLibrarySha256(
      WTF::BindOnce(&H5vccUpdater::OnGetLibrarySha256, WrapPersistent(this),
                    WrapPersistent(resolver)));
#else
  resolver->Reject();
#endif
  return resolver->Promise();
}

void H5vccUpdater::OnGetUpdaterChannel(ScriptPromiseResolver* resolver,
                                       const String& result) {
  resolver->Resolve(result);
}

void H5vccUpdater::OnVoidResult(ScriptPromiseResolver* resolver) {
  resolver->Resolve();
}

void H5vccUpdater::OnGetUpdateStatus(ScriptPromiseResolver* resolver,
                                     const String& result) {
  resolver->Resolve(result);
}

void H5vccUpdater::OnGetInstallationIndex(ScriptPromiseResolver* resolver,
                                          uint16_t result) {
  resolver->Resolve(result);
}

void H5vccUpdater::OnGetBool(ScriptPromiseResolver* resolver, bool result) {
  resolver->Resolve(result);
}

void H5vccUpdater::OnGetUpdateServerUrl(ScriptPromiseResolver* resolver,
                                        const String& result) {
  resolver->Resolve(result);
}

void H5vccUpdater::OnGetLibrarySha256(ScriptPromiseResolver* resolver,
                                      const String& result) {
  resolver->Resolve(result);
}

#if BUILDFLAG(USE_EVERGREEN)
void H5vccUpdater::EnsureReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_h5vcc_updater_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_h5vcc_updater_.BindNewPipeAndPassReceiver(task_runner));
}

#endif

void H5vccUpdater::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
#if BUILDFLAG(USE_EVERGREEN)
  visitor->Trace(remote_h5vcc_updater_);
#endif
}

}  // namespace blink
