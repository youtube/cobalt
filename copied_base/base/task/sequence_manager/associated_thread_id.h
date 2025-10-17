// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_ASSOCIATED_THREAD_ID_H_
#define BASE_TASK_SEQUENCE_MANAGER_ASSOCIATED_THREAD_ID_H_

#include <atomic>
#include <memory>

#include "base/base_export.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/threading/platform_thread.h"
#include "base/threading/platform_thread_ref.h"
#include "base/threading/thread_checker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace sequence_manager {
namespace internal {

// TODO(eseckler): Make this owned by SequenceManager once the TaskQueue
// refactor has happened (https://crbug.com/865411).
//
// This class is thread-safe. But see notes about memory ordering guarantees for
// the various methods.
class BASE_EXPORT AssociatedThreadId
    : public base::RefCountedThreadSafe<AssociatedThreadId> {
 public:
  AssociatedThreadId();

  // TODO(eseckler): Replace thread_checker with sequence_checker everywhere.
  THREAD_CHECKER(thread_checker);
  SEQUENCE_CHECKER(sequence_checker);

  static scoped_refptr<AssociatedThreadId> CreateUnbound() {
    return MakeRefCounted<AssociatedThreadId>();
  }

  static scoped_refptr<AssociatedThreadId> CreateBound() {
    auto associated_thread = MakeRefCounted<AssociatedThreadId>();
    associated_thread->BindToCurrentThread();
    return associated_thread;
  }

  // Rebind the associated thread to the current thread. This allows creating
  // the SequenceManager and TaskQueues on a different thread/sequence than the
  // one it will manage.
  //
  // Can only be called once.
  void BindToCurrentThread();

  // Checks whether this object has already been bound to a thread.
  //
  // This method guarantees a happens-before ordering with
  // BindToCurrentThread(), that is all memory writes that happened-before the
  // call to BindToCurrentThread() will become visible side-effects in the
  // current thread.
  //
  // Attention: The result might be stale by the time this method returns.
  bool IsBound() const {
    return !thread_ref_.load(std::memory_order_acquire).is_null();
  }

  // Checks whether this object is bound to the current thread. Returns false if
  // this object is not bound to any thread.
  //
  // Note that this method provides no memory ordering guarantees but those are
  // not really needed. If this method returns true we are on the same thread
  // that called BindToCurrentThread(). If the method returns false this object
  // could be unbound, so there is no possible ordering.
  //
  // Attention:: The result might be stale by the time this method returns.
  bool IsBoundToCurrentThread() const {
    return thread_ref_.load(std::memory_order_relaxed) ==
           PlatformThread::CurrentRef();
  }

 private:
  friend class base::RefCountedThreadSafe<AssociatedThreadId>;
  ~AssociatedThreadId();

  std::atomic<PlatformThreadRef> thread_ref_{};
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_ASSOCIATED_THREAD_ID_H_
