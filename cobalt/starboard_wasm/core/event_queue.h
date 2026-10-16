// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_STARBOARD_WASM_CORE_EVENT_QUEUE_H_
#define COBALT_STARBOARD_WASM_CORE_EVENT_QUEUE_H_

#include <stdint.h>

#include <deque>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "cobalt/starboard_wasm/core/abi.h"

namespace cobalt {
namespace starboard_wasm {

// Queue of sb_event records for one execution context. Push() may be called
// from any thread (typically Starboard callbacks); when it makes the queue
// non-empty, `on_wake` runs once on the owning sequence. The guest drains the
// queue with Poll(), via H5vccWasmSystem.pollEvents().
//
// Events are never dropped. Repeatable events (such as a request for more
// data) use PushCoalesced(), so the queue stays small even if the guest stops
// polling.
class EventQueue {
 public:
  EventQueue(scoped_refptr<base::SequencedTaskRunner> owner,
             base::RepeatingClosure on_wake);
  EventQueue(const EventQueue&) = delete;
  EventQueue& operator=(const EventQueue&) = delete;
  ~EventQueue();

  void Push(uint32_t type, uint32_t handle, int64_t a, int64_t b);

  // Like Push(), but first removes any queued event with the same `type`,
  // `handle` and `a`.
  void PushCoalesced(uint32_t type, uint32_t handle, int64_t a, int64_t b);

  // Moves up to `count` events into `out` (an array of sb_event) and returns
  // how many.
  uint32_t Poll(uint8_t* out, uint32_t count);

 private:
  void PushLocked(const sb_event& ev) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void RunWake();

  const scoped_refptr<base::SequencedTaskRunner> owner_;
  const base::RepeatingClosure on_wake_;

  base::Lock lock_;
  std::deque<sb_event> events_ GUARDED_BY(lock_);
  bool wake_posted_ GUARDED_BY(lock_) = false;

  base::WeakPtrFactory<EventQueue> weak_factory_{this};
};

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_CORE_EVENT_QUEUE_H_
