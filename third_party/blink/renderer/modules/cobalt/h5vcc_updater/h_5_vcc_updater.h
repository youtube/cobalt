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
#include "cobalt/browser/h5vcc_updater/public/mojom/h5vcc_updater.mojom-blink.h"  // nogncheck
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
class LocalDOMWindow;
class ScriptState;

class MODULES_EXPORT H5vccUpdater final
    : public ScriptWrappable,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit H5vccUpdater(LocalDOMWindow&);

  void ContextDestroyed() override;

  // Web-exposed interface:
  ScriptPromise<IDLString> getUpdaterChannel(ScriptState*, ExceptionState&);
  ScriptPromise<IDLUndefined> setUpdaterChannel(ScriptState*,
                                                const String&,
                                                ExceptionState&);
  ScriptPromise<IDLString> getUpdateStatus(ScriptState*, ExceptionState&);
  ScriptPromise<IDLUndefined> resetInstallations(ScriptState*, ExceptionState&);
  ScriptPromise<IDLUnsignedShort> getInstallationIndex(ScriptState*,
                                                       ExceptionState&);
  ScriptPromise<IDLString> getLibrarySha256(ScriptState*,
                                            uint16_t,
                                            ExceptionState&);
  ScriptPromise<IDLBoolean> getAllowSelfSignedPackages(ScriptState*,
                                                       ExceptionState&);
  ScriptPromise<IDLUndefined> setAllowSelfSignedPackages(ScriptState*,
                                                         bool,
                                                         ExceptionState&);
  ScriptPromise<IDLString> getUpdateServerUrl(ScriptState*, ExceptionState&);
  ScriptPromise<IDLUndefined> setUpdateServerUrl(ScriptState*,
                                                 const String&,
                                                 ExceptionState&);
  ScriptPromise<IDLBoolean> getRequireNetworkEncryption(ScriptState*,
                                                        ExceptionState&);
  ScriptPromise<IDLUndefined> setRequireNetworkEncryption(ScriptState*,
                                                          bool,
                                                          ExceptionState&);

  void Trace(Visitor*) const override;

 private:
  void OnGetUpdaterChannel(ScriptPromiseResolver<IDLString>*, const String&);
  void OnSetUpdaterChannel(ScriptPromiseResolver<IDLUndefined>*);
  void OnGetUpdateStatus(ScriptPromiseResolver<IDLString>*, const String&);
  void OnResetInstallations(ScriptPromiseResolver<IDLUndefined>*);
  void OnGetInstallationIndex(ScriptPromiseResolver<IDLUnsignedShort>*,
                              uint16_t);
  void OnGetLibrarySha256(ScriptPromiseResolver<IDLString>*, const String&);
  void OnGetAllowSelfSignedPackages(ScriptPromiseResolver<IDLBoolean>*, bool);
  void OnSetAllowSelfSignedPackages(ScriptPromiseResolver<IDLUndefined>*);
  void OnGetUpdateServerUrl(ScriptPromiseResolver<IDLString>*, const String&);
  void OnSetUpdateServerUrl(ScriptPromiseResolver<IDLUndefined>*);
  void OnGetRequireNetworkEncryption(ScriptPromiseResolver<IDLBoolean>*, bool);
  void OnSetRequireNetworkEncryption(ScriptPromiseResolver<IDLUndefined>*);
  void OnConnectionError();
  void OnSideloadingConnectionError();
  void EnsureReceiverIsBound();
  void EnsureSideloadingReceiverIsBound();
  HeapHashSet<Member<ScriptPromiseResolverBase>> ongoing_requests_;
  HeapHashSet<Member<ScriptPromiseResolverBase>> ongoing_sideloading_requests_;
  HeapMojoRemote<h5vcc_updater::mojom::blink::H5vccUpdater>
      remote_h5vcc_updater_;
  HeapMojoRemote<h5vcc_updater::mojom::blink::H5vccUpdaterSideloading>
      remote_h5vcc_updater_sideloading_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_UPDATER_H_5_VCC_UPDATER_H_
