// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/base/event_dispatcher.h"

#include "base/logging.h"

namespace base {

EventDispatcher::~EventDispatcher() {}

void EventDispatcher::AddEventCallback(TypeId type, const EventCallback& cb) {
  base::AutoLock lock(lock_);
  event_callbacks_[type].push_back(cb);
}

void EventDispatcher::RemoveEventCallback(TypeId type,
                                          const EventCallback& cb) {
  base::AutoLock lock(lock_);
  std::vector<EventCallback>& v = event_callbacks_[type];
  for (std::vector<EventCallback>::iterator it = v.begin(); it != v.end();
       ++it) {
    if (it->Equals(cb)) {
      v.erase(it);
      break;
    }
  }
}

void EventDispatcher::DispatchEvent(std::unique_ptr<Event> event) const {
  base::AutoLock lock(lock_);
  const TypeId type = event->GetTypeId();
  CallbackMap::const_iterator hash_it = event_callbacks_.find(type);
  if (hash_it == event_callbacks_.end()) {
    return;
  }

  const std::vector<EventCallback>& v = hash_it->second;
  for (std::vector<EventCallback>::const_iterator it = v.begin(); it != v.end();
       ++it) {
    it->Run(event.get());
  }
}

}  // namespace base
