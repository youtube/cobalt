/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_TRANSACTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_TRANSACTION_H_

#include <memory>

#include "base/dcheck_is_on.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/dom/dom_string_list.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_transaction.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DOMException;
class EventQueue;
class ExecutionContext;
class ExceptionState;
class IDBDatabase;
class IDBIndex;
class IDBObjectStore;
class IDBOpenDBRequest;
class IDBRequest;
class IDBRequestQueueItem;
class ScriptState;

class MODULES_EXPORT IDBTransaction final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<IDBTransaction>,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IDBTransaction* CreateNonVersionChange(
      ScriptState* script_state,
      std::unique_ptr<WebIDBTransaction> transaction_backend,
      int64_t transaction_id,
      const HashSet<String>& scope,
      mojom::IDBTransactionMode,
      mojom::IDBTransactionDurability,
      IDBDatabase* database);
  static IDBTransaction* CreateVersionChange(
      ExecutionContext*,
      std::unique_ptr<WebIDBTransaction> transaction_backend,
      int64_t transaction_id,
      IDBDatabase*,
      IDBOpenDBRequest*,
      const IDBDatabaseMetadata& old_metadata);

  // For non-upgrade transactions.
  IDBTransaction(ScriptState*,
                 std::unique_ptr<WebIDBTransaction> transaction_backend,
                 int64_t,
                 const HashSet<String>& scope,
                 mojom::IDBTransactionMode,
                 mojom::IDBTransactionDurability,
                 IDBDatabase*);
  // For upgrade transactions.
  IDBTransaction(ExecutionContext*,
                 std::unique_ptr<WebIDBTransaction> transaction_backend,
                 int64_t,
                 IDBDatabase*,
                 IDBOpenDBRequest*,
                 const IDBDatabaseMetadata&);
  ~IDBTransaction() override;

  void Trace(Visitor*) const override;

  static mojom::IDBTransactionMode StringToMode(const String&);

  // When the connection is closed backend will be 0.
  WebIDBDatabase* BackendDB() const;

  int64_t Id() const { return id_; }
  bool IsActive() const { return state_ == kActive; }
  bool IsFinished() const { return state_ == kFinished; }
  bool IsFinishing() const {
    return state_ == kCommitting || state_ == kAborting;
  }
  bool IsReadOnly() const {
    return mode_ == mojom::IDBTransactionMode::ReadOnly;
  }
  bool IsVersionChange() const {
    return mode_ == mojom::IDBTransactionMode::VersionChange;
  }
  int64_t NumErrorsHandled() const { return num_errors_handled_; }
  void IncrementNumErrorsHandled() { ++num_errors_handled_; }

  // Implement the IDBTransaction IDL
  const String& mode() const;
  const String& durability() const;
  DOMStringList* objectStoreNames() const;
  IDBDatabase* db() const { return database_.Get(); }
  DOMException* error() const { return error_; }
  IDBObjectStore* objectStore(const String& name, ExceptionState&);
  void abort(ExceptionState&);
  void commit(ExceptionState&);

  void RegisterRequest(IDBRequest*);
  void UnregisterRequest(IDBRequest*);

  // True if this transaction has at least one request whose result is being
  // post-processed.
  //
  // While this is true, new results must be queued using EnqueueResult().
  inline bool HasQueuedResults() const { return !result_queue_.empty(); }
  void EnqueueResult(std::unique_ptr<IDBRequestQueueItem> result);
  // Called when a result's post-processing has completed.
  void OnResultReady();

  // The methods below are called right before the changes are applied to the
  // database's metadata. We use this unusual sequencing because some of the
  // methods below need to access the metadata values before the change, and
  // following the same lifecycle for all methods makes the code easier to
  // reason about.
  void ObjectStoreCreated(const String& name, IDBObjectStore*);
  void ObjectStoreDeleted(const int64_t object_store_id, const String& name);
  void ObjectStoreRenamed(const String& old_name, const String& new_name);
  // Called when deleting an index whose IDBIndex had been created.
  void IndexDeleted(IDBIndex*);

  // Called during event dispatch.
  //
  // This can trigger transaction auto-commit.
  void SetActive(bool new_is_active);

  // Called right before and after structured serialization.
  void SetActiveDuringSerialization(bool new_is_active);

  void SetError(DOMException*);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(abort, kAbort)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(complete, kComplete)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error, kError)

  void OnAbort(DOMException*);
  void OnComplete();

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ScriptWrappable
  bool HasPendingActivity() const final;

  // For use in IDBObjectStore::IsNewlyCreated(). The rest of the code should
  // use IDBObjectStore::IsNewlyCreated() instead of calling this method
  // directly.
  int64_t OldMaxObjectStoreId() const {
    DCHECK(IsVersionChange());
    return old_database_metadata_.max_object_store_id;
  }

  // Returns a detailed message to use when throwing TransactionInactiveError,
  // depending on whether the transaction is just inactive or has finished.
  const char* InactiveErrorMessage() const;

  WebIDBTransaction* transaction_backend() {
    return transaction_backend_.get();
  }

  void ContextDestroyed() override {}

 protected:
  // EventTarget
  DispatchEventResult DispatchEventInternal(Event&) override;

 private:
  using IDBObjectStoreMap = HeapHashMap<String, Member<IDBObjectStore>>;

  void EnqueueEvent(Event*);

  // Called when a transaction is aborted.
  void AbortOutstandingRequests();
  void RevertDatabaseMetadata();

  // Called when a transaction is completed (committed or aborted).
  void Finished();

  enum State {
    kInactive,    // Created or started, but not in an event callback.
    kActive,      // Created or started, in creation scope or an event callback.
    kCommitting,  // In the process of completing.
    kAborting,    // In the process of aborting.
    kFinished,    // No more events will fire and no new requests may be filed.
  };

  std::unique_ptr<WebIDBTransaction> transaction_backend_;
  const int64_t id_;
  Member<IDBDatabase> database_;
  Member<IDBOpenDBRequest> open_db_request_;
  const mojom::IDBTransactionMode mode_;
  const mojom::IDBTransactionDurability durability_;

  // The names of the object stores that make up this transaction's scope.
  //
  // Transactions may not access object stores outside their scope.
  //
  // The scope of versionchange transactions is the entire database. We
  // represent this case with an empty |scope_|, because copying all the store
  // names would waste both time and memory.
  //
  // Using object store names to represent a transaction's scope is safe
  // because object stores cannot be renamed in non-versionchange
  // transactions.
  const HashSet<String> scope_;

  State state_ = kActive;
  bool has_pending_activity_ = true;
  int64_t num_errors_handled_ = 0;
  Member<DOMException> error_;

  HeapLinkedHashSet<Member<IDBRequest>> request_list_;

  // The IDBRequest results whose events have not been enqueued yet.
  //
  // When a result requires post-processing, such as large value unwrapping, it
  // is queued up until post-processing completes. All the results that arrive
  // during the post-processing phase are also queued up, so their result events
  // are fired in the order in which the requests were performed, as prescribed
  // by the IndexedDB specification.
  Deque<std::unique_ptr<IDBRequestQueueItem>> result_queue_;

#if DCHECK_IS_ON()
  bool finish_called_ = false;
#endif  // DCHECK_IS_ON()

  // Caches the IDBObjectStore instances returned by the objectStore() method.
  //
  // The spec requires that a transaction's objectStore() returns the same
  // IDBObjectStore instance for a specific store, so this cache is necessary
  // for correctness.
  //
  // objectStore() throws for completed/aborted transactions, so this is not
  // used after a transaction is finished, and can be cleared.
  IDBObjectStoreMap object_store_map_;

  // The metadata of object stores when they are opened by this transaction.
  //
  // Only valid for versionchange transactions.
  HeapHashMap<Member<IDBObjectStore>, scoped_refptr<IDBObjectStoreMetadata>>
      old_store_metadata_;

  // The metadata of deleted object stores without IDBObjectStore instances.
  //
  // Only valid for versionchange transactions.
  Vector<scoped_refptr<IDBObjectStoreMetadata>> deleted_object_stores_;

  // Tracks the indexes deleted by this transaction.
  //
  // This set only includes indexes that were created before this transaction,
  // and were deleted during this transaction. Once marked for deletion, these
  // indexes are removed from their object stores' index maps, so we need to
  // stash them somewhere else in case the transaction gets aborted.
  //
  // This set does not include indexes created and deleted during this
  // transaction, because we don't need to change their metadata when the
  // transaction aborts, as they will still be marked for deletion.
  //
  // Only valid for versionchange transactions.
  HeapVector<Member<IDBIndex>> deleted_indexes_;

  // Shallow snapshot of the database metadata when the transaction starts.
  //
  // This does not include a snapshot of the database's object store / index
  // metadata.
  //
  // Only valid for versionchange transactions.
  IDBDatabaseMetadata old_database_metadata_;

  Member<EventQueue> event_queue_;

  FrameScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_TRANSACTION_H_
