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

#include "cobalt/starboard_wasm/core/event_queue.h"

#include <string.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"

namespace cobalt {
namespace starboard_wasm {

namespace {

sb_event MakeEvent(uint32_t type, uint32_t handle, int64_t a, int64_t b) {
  sb_event ev{};
  ev.struct_size = sizeof(sb_event);
  ev.type = type;
  ev.handle = handle;
  ev.a = a;
  ev.b = b;
  return ev;
}

}  // namespace

EventQueue::EventQueue(scoped_refptr<base::SequencedTaskRunner> owner,
                       base::RepeatingClosure on_wake)
    : owner_(std::move(owner)), on_wake_(std::move(on_wake)) {}

EventQueue::~EventQueue() = default;

void EventQueue::Push(uint32_t type, uint32_t handle, int64_t a, int64_t b) {
  base::AutoLock lock(lock_);
  PushLocked(MakeEvent(type, handle, a, b));
}

void EventQueue::PushCoalesced(uint32_t type,
                               uint32_t handle,
                               int64_t a,
                               int64_t b) {
  base::AutoLock lock(lock_);
  std::erase_if(events_, [&](const sb_event& ev) {
    return ev.type == type && ev.handle == handle && ev.a == a;
  });
  PushLocked(MakeEvent(type, handle, a, b));
}

void EventQueue::PushLocked(const sb_event& ev) {
  const bool was_empty = events_.empty();
  events_.push_back(ev);
  if (was_empty && !wake_posted_) {
    wake_posted_ = true;
    owner_->PostTask(FROM_HERE, base::BindOnce(&EventQueue::RunWake,
                                               weak_factory_.GetWeakPtr()));
  }
}

uint32_t EventQueue::Poll(uint8_t* out, uint32_t count) {
  base::AutoLock lock(lock_);
  uint32_t n = 0;
  while (n < count && !events_.empty()) {
    memcpy(out + static_cast<size_t>(n) * sizeof(sb_event), &events_.front(),
           sizeof(sb_event));
    events_.pop_front();
    ++n;
  }
  return n;
}

void EventQueue::RunWake() {
  bool fire = false;
  {
    base::AutoLock lock(lock_);
    wake_posted_ = false;
    fire = !events_.empty();
  }
  if (fire && on_wake_) {
    on_wake_.Run();
  }
}

}  // namespace starboard_wasm
}  // namespace cobalt
