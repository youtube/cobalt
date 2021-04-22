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

#ifndef COBALT_BASE_MESSAGE_QUEUE_H_
#define COBALT_BASE_MESSAGE_QUEUE_H_

#include <queue>
#include "base/callback.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"

namespace base {

// A thread safe in-order message queue.  In some situations this can be
// preferred versus PostTask()ing directly to a base::MessageLoop since the data
// stored within the queued Closures can be destroyed on demand, and also the
// queue messages can be processed at a time of the user's choosing.
class MessageQueue {
 public:
  ~MessageQueue() {
    // Leave no message behind!
    ProcessAll();
  }

  // Add a message to the end of the queue.
  void AddMessage(const base::Closure& message) {
    TRACE_EVENT0("cobalt::base", "MessageQueue::AddMessage()");
    base::AutoLock lock(mutex_);
    DCHECK(!message.is_null());
    queue_.push(message);
  }

  // Execute all messages in the MessageQueue until the queue is empty and then
  // return.
  void ProcessAll() {
    TRACE_EVENT0("cobalt::base", "MessageQueue::ProcessAll()");
    std::queue<base::Closure> work_queue;
    {
      base::AutoLock lock(mutex_);
      work_queue.swap(queue_);
    }

    while (!work_queue.empty()) {
      work_queue.front().Run();
      work_queue.pop();
    }
  }

  // Clear all the messages in the queue.
  void ClearAll() {
    TRACE_EVENT0("cobalt::base", "MessageQueue::ClearAll()");
    base::AutoLock lock(mutex_);
    std::queue<base::Closure> empty_queue;
    empty_queue.swap(queue_);
  }

 private:
  base::Lock mutex_;
  std::queue<base::Closure> queue_;
};

}  // namespace base

#endif  // COBALT_BASE_MESSAGE_QUEUE_H_
