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

#ifndef COBALT_DOM_EVENT_QUEUE_H_
#define COBALT_DOM_EVENT_QUEUE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "cobalt/web/event.h"
#include "cobalt/web/event_target.h"

namespace cobalt {
namespace dom {

// Maintains an event queue that fires events asynchronously. The events in the
// queue are not guaranteed to be fired if the EventQueue is destroyed before
// it gets a chance to fire the events. However it does guarantee that the
// events will be fired asynchronous and in the order of appending.
//
// This is used where the spec requires certain events to be fired
// asynchronously and can be cancelled. For example, most of the events on the
// media element work in this way.
// Note: After putting an event to the queue, the caller should no longer modify
// the content of the event until it is fired or cancelled.
class EventQueue : public base::SupportsWeakPtr<EventQueue>,
                   public script::Traceable {
 public:
  // The EventTarget is guaranteed to be valid during the life time and should
  // usually be the owner.
  explicit EventQueue(web::EventTarget* event_target);

  void Enqueue(const scoped_refptr<web::Event>& event);
  void CancelAllEvents();

  void TraceMembers(script::Tracer* tracer) override;

 private:
  typedef std::vector<scoped_refptr<web::Event> > Events;
  void DispatchEvents();

  web::EventTarget* event_target_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  Events events_;
  // Events that are currently being fired. We need this as local variable for
  // JavaScript tracing purpose.
  Events firing_events_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_EVENT_QUEUE_H_
