// Copyright 2016 Google Inc. All Rights Reserved.
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

// Module Overview: Starboard Queue module
//
// Defines a C++-only synchronized queue implementation, built entirely on top
// of Starboard synchronization primitives. It can be safely used by both
// clients and implementations.

#ifndef STARBOARD_QUEUE_H_
#define STARBOARD_QUEUE_H_

#include <algorithm>
#include <deque>

#include "starboard/condition_variable.h"
#include "starboard/export.h"
#include "starboard/mutex.h"
#include "starboard/time.h"
#include "starboard/types.h"

#ifndef __cplusplus
#error "Only C++ files can include this header."
#endif

namespace starboard {

// Synchronized, blocking queue, based on starboard::ConditionVariable. This
// class is designed for T to be a pointer type, or something otherwise
// inherently Nullable, as there is no way to distinguish T() from "no result."
template <typename T>
class Queue {
 public:
  Queue() : condition_(mutex_), wake_(false) {}
  ~Queue() {}

  // Polls for an item, returning the default value of T if nothing is present.
  T Poll() {
    ScopedLock lock(mutex_);
    if (!queue_.empty()) {
      T entry = queue_.front();
      queue_.pop_front();
      return entry;
    }

    return T();
  }

  // Gets the item at the front of the queue, blocking until there is such an
  // item, or the queue is woken up. If there are multiple waiters, this Queue
  // guarantees that only one waiter will receive any given queue item.
  T Get() {
    ScopedLock lock(mutex_);
    while (queue_.empty()) {
      if (wake_) {
        wake_ = false;
        return T();
      }

      condition_.Wait();
    }

    T entry = queue_.front();
    queue_.pop_front();
    return entry;
  }

  // Gets the item at the front of the queue, blocking until there is such an
  // item, or the given timeout duration expires, or the queue is woken up. If
  // there are multiple waiters, this Queue guarantees that only one waiter will
  // receive any given queue item.
  T GetTimed(SbTime duration) {
    ScopedLock lock(mutex_);
    SbTimeMonotonic start = SbTimeGetMonotonicNow();
    while (queue_.empty()) {
      if (wake_) {
        wake_ = false;
        return T();
      }

      SbTimeMonotonic elapsed = SbTimeGetMonotonicNow() - start;
      if (elapsed >= duration) {
        return T();
      }
      condition_.WaitTimed(duration - elapsed);
    }

    T entry = queue_.front();
    queue_.pop_front();
    return entry;
  }

  // Pushes |value| onto the back of the queue, waking up a single waiter, if
  // any exist.
  void Put(T value) {
    ScopedLock lock(mutex_);
    queue_.push_back(value);
    condition_.Signal();
  }

  // Forces one waiter to wake up empty-handed.
  void Wake() {
    ScopedLock lock(mutex_);
    wake_ = true;
    condition_.Signal();
  }

  // It is guaranteed that after this function returns there is no element in
  // the queue equals to |value|.  This is useful to remove un-processed values
  // before destroying the owning object.
  // Note that it is the responsibility of the caller to free any resources
  // associated with |value| after this function returns.  It is possible that
  // another thread is still using |value|, the caller should make sure that
  // this is properly coordinated with the free of resources.  Usually this can
  // be solved by calling Remove() on the same thread that calls Get().
  void Remove(T value) {
    ScopedLock lock(mutex_);
    queue_.erase(std::remove(queue_.begin(), queue_.end(), value),
                 queue_.end());
  }

  void Clear() {
    ScopedLock lock(mutex_);
    queue_.clear();
  }

  size_t Size() {
    ScopedLock lock(mutex_);
    return queue_.size();
  }

 private:
  Mutex mutex_;
  ConditionVariable condition_;
  std::deque<T> queue_;
  bool wake_;
};

}  // namespace starboard

#endif  // STARBOARD_QUEUE_H_
