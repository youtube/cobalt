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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_UPDATER_H_5_VCC_UPDATER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_UPDATER_H_5_VCC_UPDATER_H_

#include "build/build_config.h"

#if BUILDFLAG(USE_EVERGREEN)
#include "cobalt/browser/h5vcc_updater/public/mojom/h5vcc_updater.mojom-blink.h"  // nogncheck
#endif

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
class LocalDOMWindow;
class ScriptState;
class ScriptPromiseResolver;

class MODULES_EXPORT H5vccUpdater final
    : public ScriptWrappable,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit H5vccUpdater(LocalDOMWindow&);

  void ContextDestroyed() override;

  // Web-exposed interface:
  ScriptPromise<IDLString> getUpdaterChannel(ScriptState* script_state,
                                             ExceptionState& exception_state);
  ScriptPromise<IDLUndefined> setUpdaterChannel(
      ScriptState* script_state,
      const String& channel,
      ExceptionState& exception_state);
  ScriptPromise<IDLString> getUpdateStatus(ScriptState* script_state,
                                           ExceptionState& exception_state);
  ScriptPromise<IDLUndefined> resetInstallations(
      ScriptState* script_state,
      ExceptionState& exception_state);
  ScriptPromise<IDLUnsignedShort> getInstallationIndex(
      ScriptState* script_state,
      ExceptionState& exception_state);
  ScriptPromise<IDLBoolean> getAllowSelfSignedPackages(
      ScriptState* script_state,
      ExceptionState& exception_state);
  ScriptPromise<IDLUndefined> setAllowSelfSignedPackages(
      ScriptState* script_state,
      bool allow_self_signed_packages,
      ExceptionState& exception_state);
  ScriptPromise<IDLString> getUpdateServerUrl(ScriptState* script_state,
                                              ExceptionState& exception_state);
  ScriptPromise<IDLUndefined> setUpdateServerUrl(
      ScriptState* script_state,
      const String& update_server_url,
      ExceptionState& exception_state);
  ScriptPromise<IDLBoolean> getRequireNetworkEncryption(
      ScriptState* script_state,
      ExceptionState& exception_state);
  ScriptPromise<IDLUndefined> setRequireNetworkEncryption(
      ScriptState* script_state,
      bool require_network_encryption,
      ExceptionState& exception_state);

  void Trace(Visitor*) const override;

 private:
  void OnGetUpdaterChannel(ScriptPromiseResolver* resolver,
                           const String& result);
  void OnVoidResult(ScriptPromiseResolver* resolver);
  void OnGetUpdateStatus(ScriptPromiseResolver* resolver, const String& result);
  void OnGetInstallationIndex(ScriptPromiseResolver* resolver, uint16_t result);
  void OnGetBool(ScriptPromiseResolver* resolver, bool result);
  void OnGetUpdateServerUrl(ScriptPromiseResolver* resolver,
                            const String& result);
#if BUILDFLAG(USE_EVERGREEN)
  void EnsureReceiverIsBound();
  HeapMojoRemote<h5vcc_updater::mojom::blink::H5vccUpdater>
      remote_h5vcc_updater_;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_UPDATER_H_5_VCC_UPDATER_H_
