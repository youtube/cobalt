/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/event_queue.h"

#include "base/bind.h"
#include "base/logging.h"

namespace cobalt {
namespace dom {

EventQueue::EventQueue(EventTarget* event_target)
    : event_target_(event_target),
      message_loop_(base::MessageLoopProxy::current()) {
  DCHECK(event_target_);
  DCHECK(message_loop_);
}

void EventQueue::Enqueue(const scoped_refptr<Event>& event) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (events_.empty()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&EventQueue::DispatchEvents, AsWeakPtr()));
  }

  // Clear the target if it is the same as the stored one to avoid circular
  // reference.
  if (event->target() == event_target_) {
    event->set_target(NULL);
  }

  events_.push_back(event);
}

void EventQueue::CancelAllEvents() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  events_.clear();
}

void EventQueue::DispatchEvents() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  Events events;
  events.swap(events_);

  for (Events::iterator iter = events.begin(); iter != events.end(); ++iter) {
    scoped_refptr<Event>& event = *iter;
    EventTarget* target =
        event->target() ? event->target().get() : event_target_;
    target->DispatchEvent(event);
  }
}

}  // namespace dom
}  // namespace cobalt
