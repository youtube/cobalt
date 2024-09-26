// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor.h"

#include <stddef.h>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_dispatcher.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WebIDBCursor::WebIDBCursor(
    mojo::PendingAssociatedRemote<mojom::blink::IDBCursor> cursor_info,
    int64_t transaction_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : transaction_id_(transaction_id) {
  cursor_.Bind(std::move(cursor_info), std::move(task_runner));
  IndexedDBDispatcher::RegisterCursor(this);
}

WebIDBCursor::~WebIDBCursor() {
  // It's not possible for there to be pending callbacks that address this
  // object since inside WebKit, they hold a reference to the object which owns
  // this object. But, if that ever changed, then we'd need to invalidate
  // any such pointers.
  IndexedDBDispatcher::UnregisterCursor(this);
}

void WebIDBCursor::Advance(uint32_t count,
                           std::unique_ptr<WebIDBCallbacks> callbacks) {
  if (count <= prefetch_keys_.size()) {
    CachedAdvance(count, callbacks.get());
    return;
  }
  ResetPrefetchCache();

  // Reset all cursor prefetch caches except for this cursor.
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id_, this);

  callbacks->SetState(weak_factory_.GetWeakPtr(), transaction_id_);
  cursor_->Advance(count,
                   WTF::BindOnce(&WebIDBCursor::AdvanceCallback,
                                 WTF::Unretained(this), std::move(callbacks)));
}

void WebIDBCursor::AdvanceCallback(std::unique_ptr<WebIDBCallbacks> callbacks,
                                   mojom::blink::IDBCursorResultPtr result) {
  if (result->is_error_result()) {
    callbacks->Error(result->get_error_result()->error_code,
                     std::move(result->get_error_result()->error_message));
    callbacks.reset();
    return;
  }

  if (result->is_empty() && result->get_empty()) {
    callbacks->SuccessValue(nullptr);
    callbacks.reset();
    return;
  } else if (result->is_empty()) {
    callbacks->Error(mojom::blink::IDBException::kUnknownError,
                     "Invalid response");
    callbacks.reset();
    return;
  }

  if (result->get_values()->keys.size() != 1u ||
      result->get_values()->primary_keys.size() != 1u ||
      result->get_values()->values.size() != 1u) {
    callbacks->Error(mojom::blink::IDBException::kUnknownError,
                     "Invalid response");
    callbacks.reset();
    return;
  }

  callbacks->SuccessCursorContinue(
      std::move(result->get_values()->keys[0]),
      std::move(result->get_values()->primary_keys[0]),
      std::move(result->get_values()->values[0]));
  callbacks.reset();
}

void WebIDBCursor::CursorContinue(const IDBKey* key,
                                  const IDBKey* primary_key,
                                  std::unique_ptr<WebIDBCallbacks> callbacks) {
  DCHECK(key && primary_key);

  if (key->GetType() == mojom::blink::IDBKeyType::None &&
      primary_key->GetType() == mojom::blink::IDBKeyType::None) {
    // No key(s), so this would qualify for a prefetch.
    ++continue_count_;

    if (!prefetch_keys_.empty()) {
      // We have a prefetch cache, so serve the result from that.
      CachedContinue(callbacks.get());
      return;
    }

    if (continue_count_ > kPrefetchContinueThreshold) {
      // Request pre-fetch.
      ++pending_onsuccess_callbacks_;

      callbacks->SetState(weak_factory_.GetWeakPtr(), transaction_id_);
      cursor_->Prefetch(
          prefetch_amount_,
          WTF::BindOnce(&WebIDBCursor::PrefetchCallback, WTF::Unretained(this),
                        std::move(callbacks)));

      // Increase prefetch_amount_ exponentially.
      prefetch_amount_ *= 2;
      if (prefetch_amount_ > kMaxPrefetchAmount)
        prefetch_amount_ = kMaxPrefetchAmount;

      return;
    }
  } else {
    // Key argument supplied. We couldn't prefetch this.
    ResetPrefetchCache();
  }

  // Reset all cursor prefetch caches except for this cursor.
  IndexedDBDispatcher::ResetCursorPrefetchCaches(transaction_id_, this);
  callbacks->SetState(weak_factory_.GetWeakPtr(), transaction_id_);
  cursor_->CursorContinue(
      IDBKey::Clone(key), IDBKey::Clone(primary_key),
      WTF::BindOnce(&WebIDBCursor::CursorContinueCallback,
                    WTF::Unretained(this), std::move(callbacks)));
}

void WebIDBCursor::CursorContinueCallback(
    std::unique_ptr<WebIDBCallbacks> callbacks,
    mojom::blink::IDBCursorResultPtr result) {
  if (result->is_error_result()) {
    callbacks->Error(result->get_error_result()->error_code,
                     std::move(result->get_error_result()->error_message));
    callbacks.reset();
    return;
  }

  if (result->is_empty() && result->get_empty()) {
    callbacks->SuccessValue(nullptr);
    callbacks.reset();
    return;
  } else if (result->is_empty()) {
    callbacks->Error(mojom::blink::IDBException::kUnknownError,
                     "Invalid response");
    callbacks.reset();
    return;
  }

  if (result->get_values()->keys.size() != 1u ||
      result->get_values()->primary_keys.size() != 1u ||
      result->get_values()->values.size() != 1u) {
    callbacks->Error(mojom::blink::IDBException::kUnknownError,
                     "Invalid response");
    callbacks.reset();
    return;
  }

  callbacks->SuccessCursorContinue(
      std::move(result->get_values()->keys[0]),
      std::move(result->get_values()->primary_keys[0]),
      std::move(result->get_values()->values[0]));
  callbacks.reset();
}

void WebIDBCursor::PrefetchCallback(std::unique_ptr<WebIDBCallbacks> callbacks,
                                    mojom::blink::IDBCursorResultPtr result) {
  if (result->is_error_result()) {
    callbacks->Error(result->get_error_result()->error_code,
                     std::move(result->get_error_result()->error_message));
    callbacks.reset();
    return;
  }

  if (result->is_empty() && result->get_empty()) {
    callbacks->SuccessValue(nullptr);
    callbacks.reset();
    return;
  } else if (result->is_empty()) {
    callbacks->Error(mojom::blink::IDBException::kUnknownError,
                     "Invalid response");
    callbacks.reset();
    return;
  }

  if (result->get_values()->keys.size() !=
          result->get_values()->primary_keys.size() ||
      result->get_values()->keys.size() !=
          result->get_values()->values.size()) {
    callbacks->Error(mojom::blink::IDBException::kUnknownError,
                     "Invalid response");
    callbacks.reset();
    return;
  }

  callbacks->SuccessCursorPrefetch(
      std::move(result->get_values()->keys),
      std::move(result->get_values()->primary_keys),
      std::move(result->get_values()->values));
  callbacks.reset();
}

void WebIDBCursor::PostSuccessHandlerCallback() {
  pending_onsuccess_callbacks_--;

  // If the onsuccess callback called continue()/advance() on the cursor
  // again, and that request was served by the prefetch cache, then
  // pending_onsuccess_callbacks_ would be incremented. If not, it means the
  // callback did something else, or nothing at all, in which case we need to
  // reset the cache.

  if (pending_onsuccess_callbacks_ == 0)
    ResetPrefetchCache();
}

void WebIDBCursor::SetPrefetchData(Vector<std::unique_ptr<IDBKey>> keys,
                                   Vector<std::unique_ptr<IDBKey>> primary_keys,
                                   Vector<std::unique_ptr<IDBValue>> values) {
  // Keys and values are stored in reverse order so that a cache'd continue can
  // pop a value off of the back and prevent new memory allocations.
  prefetch_keys_.AppendRange(std::make_move_iterator(keys.rbegin()),
                             std::make_move_iterator(keys.rend()));
  prefetch_primary_keys_.AppendRange(
      std::make_move_iterator(primary_keys.rbegin()),
      std::make_move_iterator(primary_keys.rend()));
  prefetch_values_.AppendRange(std::make_move_iterator(values.rbegin()),
                               std::make_move_iterator(values.rend()));

  used_prefetches_ = 0;
  pending_onsuccess_callbacks_ = 0;
}

void WebIDBCursor::CachedAdvance(uint32_t count, WebIDBCallbacks* callbacks) {
  DCHECK_GE(prefetch_keys_.size(), count);
  DCHECK_EQ(prefetch_primary_keys_.size(), prefetch_keys_.size());
  DCHECK_EQ(prefetch_values_.size(), prefetch_keys_.size());

  while (count > 1) {
    prefetch_keys_.pop_back();
    prefetch_primary_keys_.pop_back();
    prefetch_values_.pop_back();
    ++used_prefetches_;
    --count;
  }

  CachedContinue(callbacks);
}

void WebIDBCursor::CachedContinue(WebIDBCallbacks* callbacks) {
  DCHECK_GT(prefetch_keys_.size(), 0ul);
  DCHECK_EQ(prefetch_primary_keys_.size(), prefetch_keys_.size());
  DCHECK_EQ(prefetch_values_.size(), prefetch_keys_.size());

  // Keys and values are stored in reverse order so that a cache'd continue can
  // pop a value off of the back and prevent new memory allocations.
  std::unique_ptr<IDBKey> key = std::move(prefetch_keys_.back());
  std::unique_ptr<IDBKey> primary_key =
      std::move(prefetch_primary_keys_.back());
  std::unique_ptr<IDBValue> value = std::move(prefetch_values_.back());

  prefetch_keys_.pop_back();
  prefetch_primary_keys_.pop_back();
  prefetch_values_.pop_back();
  ++used_prefetches_;

  ++pending_onsuccess_callbacks_;

  if (!continue_count_) {
    // The cache was invalidated by a call to ResetPrefetchCache()
    // after the RequestIDBCursorPrefetch() was made. Now that the
    // initiating continue() call has been satisfied, discard
    // the rest of the cache.
    ResetPrefetchCache();
  }

  callbacks->SuccessCursorContinue(std::move(key), std::move(primary_key),
                                   std::move(value));
}

void WebIDBCursor::ResetPrefetchCache() {
  continue_count_ = 0;
  prefetch_amount_ = kMinPrefetchAmount;

  if (prefetch_keys_.empty()) {
    // No prefetch cache, so no need to reset the cursor in the back-end.
    return;
  }

  // Reset the back-end cursor.
  cursor_->PrefetchReset(used_prefetches_, prefetch_keys_.size());

  // Reset the prefetch cache.
  prefetch_keys_.clear();
  prefetch_primary_keys_.clear();
  prefetch_values_.clear();

  pending_onsuccess_callbacks_ = 0;
}

}  // namespace blink
