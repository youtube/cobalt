// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_ACCESS_STORAGE_ACCESS_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_ACCESS_STORAGE_ACCESS_HANDLE_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/storage_access/storage_access_handle.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_access_types.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_factory.h"
#include "third_party/blink/renderer/modules/locks/lock_manager.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/storage/storage_area.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExceptionState;

class MODULES_EXPORT StorageAccessHandle final
    : public ScriptWrappable,
      public Supplement<LocalDOMWindow> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  static const char kSessionStorageNotRequested[];
  static const char kLocalStorageNotRequested[];
  static const char kIndexedDBNotRequested[];
  static const char kLocksNotRequested[];

  explicit StorageAccessHandle(LocalDOMWindow& window,
                               const StorageAccessTypes* storage_access_types);
  void Trace(Visitor* visitor) const override;

  StorageArea* sessionStorage(ExceptionState& exception_state) const;
  StorageArea* localStorage(ExceptionState& exception_state) const;
  IDBFactory* indexedDB(ExceptionState& exception_state) const;
  LockManager* locks(ExceptionState& exception_state) const;

 private:
  void InitSessionStorage();
  void InitLocalStorage();
  HeapMojoRemote<mojom::blink::StorageAccessHandle>& GetRemote();
  void InitIndexedDB();
  void InitLocks();

  Member<const StorageAccessTypes> storage_access_types_;
  Member<StorageArea> session_storage_;
  Member<StorageArea> local_storage_;
  HeapMojoRemote<mojom::blink::StorageAccessHandle> remote_;
  Member<IDBFactory> indexed_db_;
  Member<LockManager> locks_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_ACCESS_STORAGE_ACCESS_HANDLE_H_
