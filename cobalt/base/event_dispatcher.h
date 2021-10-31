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

#ifndef COBALT_BASE_EVENT_DISPATCHER_H_
#define COBALT_BASE_EVENT_DISPATCHER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/synchronization/lock.h"
#include "cobalt/base/event.h"

namespace base {

typedef Callback<void(const Event*)> EventCallback;

// A base class for sending events to a set of registered listeners.
// Event producers will call DispatchEvent and each callback will be called.
// The recipient of the callback is responsible for re-posting to an appropriate
// message loop. No other work should be done on the thread that receives
// the callback.
class EventDispatcher {
 public:
  ~EventDispatcher();
  // Send the event to all callers registered for this type of event.
  // May be called from any thread.
  void DispatchEvent(std::unique_ptr<Event> event) const;

  // Register a callback to be notified about Events of the given type.
  // May be called from any thread, but not from a dispatched event.
  void AddEventCallback(TypeId type, const EventCallback& cb);

  // De-register the given callback from subsequent notifications.
  // May be called from any thread, but not from a dispatched event.
  void RemoveEventCallback(TypeId type, const EventCallback& cb);

 private:
  typedef hash_map<TypeId, std::vector<EventCallback> > CallbackMap;
  CallbackMap event_callbacks_;
  // Lock to protect access to the callback map.
  mutable base::Lock lock_;
};

}  // namespace base

#endif  // COBALT_BASE_EVENT_DISPATCHER_H_
