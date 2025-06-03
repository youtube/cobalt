// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BUCKETS_STORAGE_BUCKET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BUCKETS_STORAGE_BUCKET_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/storage_manager_file_system_access.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class CacheStorage;
class IDBFactory;
class LockManager;
class ScriptState;

class StorageBucket final : public ScriptWrappable,
                            public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  StorageBucket(NavigatorBase* navigator,
                const String& name,
                mojo::PendingRemote<mojom::blink::BucketHost> remote);

  ~StorageBucket() override = default;

  const String& name();
  ScriptPromise persist(ScriptState*);
  ScriptPromise persisted(ScriptState*);
  ScriptPromise estimate(ScriptState*);
  ScriptPromise durability(ScriptState*);
  ScriptPromise setExpires(ScriptState*, const DOMHighResTimeStamp&);
  ScriptPromise expires(ScriptState*);
  IDBFactory* indexedDB();
  LockManager* locks();
  CacheStorage* caches(ExceptionState&);
  ScriptPromise getDirectory(ScriptState*, ExceptionState&);

  void GetDirectoryForDevTools(
      ExecutionContext* context,
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr,
                              FileSystemDirectoryHandle*)> callback);

  // GarbageCollected
  void Trace(Visitor*) const override;

 private:
  void DidRequestPersist(ScriptPromiseResolver* resolver,
                         bool persisted,
                         bool success);
  void DidGetPersisted(ScriptPromiseResolver* resolver,
                       bool persisted,
                       bool success);
  void DidGetEstimate(ScriptPromiseResolver* resolver,
                      int64_t current_usage,
                      int64_t current_quota,
                      bool success);
  void DidGetDurability(ScriptPromiseResolver* resolver,
                        mojom::blink::BucketDurability durability,
                        bool success);
  void DidSetExpires(ScriptPromiseResolver* resolver, bool success);
  void DidGetExpires(ScriptPromiseResolver* resolver,
                     const absl::optional<base::Time> expires,
                     bool success);
  void GetSandboxedFileSystem(ScriptPromiseResolver* resolver);
  void GetSandboxedFileSystemForDevtools(
      ExecutionContext* context,
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr,
                              FileSystemDirectoryHandle*)> callback,
      mojom::blink::FileSystemAccessErrorPtr result);

  String name_;

  // BucketHost in the browser process.
  HeapMojoRemote<mojom::blink::BucketHost> remote_;

  Member<IDBFactory> idb_factory_;
  Member<LockManager> lock_manager_;
  Member<CacheStorage> caches_;
  Member<NavigatorBase> navigator_base_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BUCKETS_STORAGE_BUCKET_H_
