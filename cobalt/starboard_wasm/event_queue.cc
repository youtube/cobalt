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

#include "cobalt/starboard_wasm/event_queue.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"

namespace cobalt {
namespace starboard_wasm {

EventQueue::EventQueue(scoped_refptr<base::SequencedTaskRunner> task_runner,
                       base::RepeatingClosure on_wake)
    : task_runner_(std::move(task_runner)), on_wake_(std::move(on_wake)) {}

EventQueue::~EventQueue() = default;

void EventQueue::Push(const WasmSbEvent& event) {
  base::AutoLock lock(lock_);
  events_.push_back(event);
  if (wake_requested_) {
    PostWakeLocked();
  }
}

void EventQueue::DropPlayer(uint32_t player) {
  base::AutoLock lock(lock_);
  base::EraseIf(events_, [player](const WasmSbEvent& event) {
    return event.player == player;
  });
}

size_t EventQueue::Poll(base::span<uint8_t> out) {
  base::AutoLock lock(lock_);
  size_t n = 0;
  while (out.size() >= sizeof(WasmSbEvent) && !events_.empty()) {
    out.take_first<sizeof(WasmSbEvent)>().copy_from(
        base::byte_span_from_ref(events_.front()));
    events_.pop_front();
    ++n;
  }
  return n;
}

void EventQueue::RequestWake() {
  base::AutoLock lock(lock_);
  if (events_.empty()) {
    wake_requested_ = true;
  } else {
    PostWakeLocked();
  }
}

void EventQueue::Shutdown() {
  base::AutoLock lock(lock_);
  on_wake_.Reset();
  wake_requested_ = false;
}

void EventQueue::PostWakeLocked() {
  wake_requested_ = false;
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&EventQueue::RunWake,
                                                   base::WrapRefCounted(this)));
}

void EventQueue::RunWake() {
  base::RepeatingClosure on_wake;
  {
    base::AutoLock lock(lock_);
    on_wake = on_wake_;
  }
  if (on_wake) {
    on_wake.Run();
  }
}

}  // namespace starboard_wasm
}  // namespace cobalt
