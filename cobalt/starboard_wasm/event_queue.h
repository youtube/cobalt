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

#ifndef COBALT_STARBOARD_WASM_EVENT_QUEUE_H_
#define COBALT_STARBOARD_WASM_EVENT_QUEUE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "cobalt/starboard_wasm/wasm_starboard_abi.h"

namespace cobalt {
namespace starboard_wasm {

// The events waiting for H5vccWasmPlayer.pollEvents(). Starboard callbacks
// Push() on their own threads; the queue never drops events. Players hold a
// reference, so callbacks during an SbPlayerDestroy() that outlives the
// execution context stay safe.
// `on_wake` runs on `task_runner` at most once per RequestWake(): as soon as
// an event is queued, or right away if one already is.
class EventQueue : public base::RefCountedThreadSafe<EventQueue> {
 public:
  EventQueue(scoped_refptr<base::SequencedTaskRunner> task_runner,
             base::RepeatingClosure on_wake);
  EventQueue(const EventQueue&) = delete;
  EventQueue& operator=(const EventQueue&) = delete;

  void Push(const WasmSbEvent& event);
  void DropPlayer(uint32_t player);
  // Moves as many events as fit into `out`, which need not be aligned;
  // returns how many.
  size_t Poll(base::span<uint8_t> out);
  void RequestWake();
  const scoped_refptr<base::SequencedTaskRunner>& task_runner() const {
    return task_runner_;
  }
  // Called on `task_runner`; no wake runs afterwards.
  void Shutdown();

 private:
  friend class base::RefCountedThreadSafe<EventQueue>;
  ~EventQueue();

  void PostWakeLocked() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void RunWake();

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::Lock lock_;
  base::RepeatingClosure on_wake_ GUARDED_BY(lock_);
  base::circular_deque<WasmSbEvent> events_ GUARDED_BY(lock_);
  bool wake_requested_ GUARDED_BY(lock_) = false;
};

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_EVENT_QUEUE_H_
