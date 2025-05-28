// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_WORKLET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_WORKLET_H_

#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_fencedframeconfig_usvstring.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/shared_storage/util.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"

namespace blink {
class ScriptPromiseResolverBase;
class SharedStorageUrlWithMetadata;
class SharedStorageRunOperationMethodOptions;
class WorkletOptions;

// Implement the worklet attribute under window.sharedStorage.
class MODULES_EXPORT SharedStorageWorklet final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SharedStorageWorklet* Create(ScriptState*);

  SharedStorageWorklet() = default;

  ~SharedStorageWorklet() override = default;

  void Trace(Visitor*) const override;

  // shared_storage_worklet.idl
  // addModule() imports ES6 module scripts.
  ScriptPromise<IDLUndefined> addModule(ScriptState*,
                                        const String& module_url,
                                        const WorkletOptions* options,
                                        ExceptionState&);
  ScriptPromise<V8SharedStorageResponse> selectURL(
      ScriptState*,
      const String& name,
      HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
      ExceptionState&);
  ScriptPromise<V8SharedStorageResponse> selectURL(
      ScriptState*,
      const String& name,
      HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
      const SharedStorageRunOperationMethodOptions* options,
      ExceptionState&);
  ScriptPromise<IDLAny> run(ScriptState*, const String& name, ExceptionState&);
  ScriptPromise<IDLAny> run(
      ScriptState*,
      const String& name,
      const SharedStorageRunOperationMethodOptions* options,
      ExceptionState&);

  // Helper implementation method for `sharedStorage.worklet.addModule()` and
  // for `sharedStorage.createWorklet()`.
  void AddModuleHelper(ScriptState*,
                       ScriptPromiseResolverBase*,
                       const String& module_url,
                       const WorkletOptions* options,
                       ExceptionState&,
                       bool resolve_to_worklet,
                       SharedStorageDataOrigin data_origin_type,
                       scoped_refptr<SecurityOrigin> custom_data_origin);

 private:
  // Set when addModule() was called and passed early renderer checks.
  HeapMojoAssociatedRemote<mojom::blink::SharedStorageWorkletHost>
      worklet_host_{nullptr};

  // Set to the script origin when addModule() was called and passed early
  // renderer checks (i.e. initialized along with `worklet_host_`).
  url::Origin shared_storage_origin_;

  bool keep_alive_after_operation_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_WORKLET_H_
