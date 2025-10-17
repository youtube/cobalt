// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_COMMON_CHECKED_LOCK_IMPL_H_
#define BASE_TASK_COMMON_CHECKED_LOCK_IMPL_H_

#include <memory>

#include "base/base_export.h"
#include "base/synchronization/lock.h"

namespace base {

class ConditionVariable;

namespace internal {

struct UniversalPredecessor {};
struct UniversalSuccessor {};

// A regular lock with simple deadlock correctness checking.
// This lock tracks all of the available locks to make sure that any locks are
// acquired in an expected order.
// See scheduler_lock.h for details.
class BASE_EXPORT CheckedLockImpl {
 public:
  CheckedLockImpl();
  explicit CheckedLockImpl(const CheckedLockImpl* predecessor);
  explicit CheckedLockImpl(UniversalPredecessor);
  explicit CheckedLockImpl(UniversalSuccessor);

  CheckedLockImpl(const CheckedLockImpl&) = delete;
  CheckedLockImpl& operator=(const CheckedLockImpl&) = delete;

  ~CheckedLockImpl();

  static void AssertNoLockHeldOnCurrentThread();

  void Acquire() EXCLUSIVE_LOCK_FUNCTION(lock_);
  void Release() UNLOCK_FUNCTION(lock_);

  void AssertAcquired() const;
  void AssertNotHeld() const;

  std::unique_ptr<ConditionVariable> CreateConditionVariable();

  bool is_universal_predecessor() const { return is_universal_predecessor_; }
  bool is_universal_successor() const { return is_universal_successor_; }

 private:
  Lock lock_;
  const bool is_universal_predecessor_ = false;
  const bool is_universal_successor_ = false;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_COMMON_CHECKED_LOCK_IMPL_H_
