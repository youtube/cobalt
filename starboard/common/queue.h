// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_QUEUE_H_
#define STARBOARD_COMMON_QUEUE_H_

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace starboard {

// Synchronized, blocking queue, based on starboard::ConditionVariable. This
// class is designed for T to be a pointer type, or something otherwise
// inherently Nullable, as there is no way to distinguish T() from "no result".
// Note: The class allows its user to block and wait for a new item.
// It's not meant to be used as a thread safe replacement of
// std::deque<>/std::queue<>.
template <typename T>
class Queue {
 public:
  Queue() : wake_(false) {}
  ~Queue() {}

  // Polls for an item, returning the default value of T if nothing is present.
  T Poll() {
    std::lock_guard lock(mutex_);
    if (!queue_.empty()) {
      T entry = std::move(queue_.front());
      queue_.pop_front();
      return entry;
    }

    return T();
  }

  // Gets the item at the front of the queue, blocking until there is such an
  // item, or the queue is woken up. If there are multiple waiters, this Queue
  // guarantees that only one waiter will receive any given queue item.
  T Get() {
    std::unique_lock lock(mutex_);
    condition_.wait(lock, [this] { return !queue_.empty() || wake_; });

    if (!queue_.empty()) {
      T entry = std::move(queue_.front());
      queue_.pop_front();
      return entry;
    }

    // The queue is empty, so this must be a wake-up.
    wake_ = false;
    return T();
  }

  // Gets the item at the front of the queue, blocking until there is such an
  // item, or the given timeout duration (in microseconds) expires, or the queue
  // is woken up. If there are multiple waiters, this Queue guarantees that only
  // one waiter will receive any given queue item.
  T GetTimed(int64_t duration) {
    std::unique_lock lock(mutex_);
    if (!condition_.wait_for(lock, std::chrono::microseconds(duration),
                             [this] { return !queue_.empty() || wake_; })) {
      return T();
    }

    if (!queue_.empty()) {
      T entry = std::move(queue_.front());
      queue_.pop_front();
      return entry;
    }

    // The queue is empty, so this must be a wake-up.
    wake_ = false;
    return T();
  }

  // Pushes |value| onto the back of the queue, waking up a single waiter, if
  // any exist.
  void Put(T value) {
    std::lock_guard lock(mutex_);
    queue_.push_back(std::move(value));
    condition_.notify_one();
  }

  // Forces one waiter to wake up empty-handed.
  void Wake() {
    std::lock_guard lock(mutex_);
    wake_ = true;
    condition_.notify_one();
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
    std::lock_guard lock(mutex_);
    queue_.erase(std::remove(queue_.begin(), queue_.end(), value),
                 queue_.end());
  }

  void Clear() {
    std::lock_guard lock(mutex_);
    queue_.clear();
  }

  size_t Size() const {
    std::lock_guard lock(mutex_);
    return queue_.size();
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::deque<T> queue_;
  bool wake_;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_QUEUE_H_
