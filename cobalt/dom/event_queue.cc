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

#include "cobalt/dom/event_queue.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

namespace cobalt {
namespace dom {

EventQueue::EventQueue(web::EventTarget* event_target)
    : event_target_(event_target),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DCHECK(event_target_);
  DCHECK(task_runner_);
}

void EventQueue::Enqueue(const scoped_refptr<web::Event>& event) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (events_.empty()) {
    task_runner_->PostTask(
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
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  events_.clear();
}

void EventQueue::TraceMembers(script::Tracer* tracer) {
  tracer->TraceItems(events_);
  tracer->TraceItems(firing_events_);
}

void EventQueue::DispatchEvents() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(firing_events_.empty());

  // Make sure that the event_target_ stays alive for the duration of
  // all event dispatches.
  scoped_refptr<web::EventTarget> keep_alive_reference(event_target_);

  firing_events_.swap(events_);

  for (Events::iterator iter = firing_events_.begin();
       iter != firing_events_.end(); ++iter) {
    scoped_refptr<web::Event>& event = *iter;
    web::EventTarget* target =
        event->target() ? event->target().get() : event_target_;
    target->DispatchEvent(event);
  }
  firing_events_.clear();
}

}  // namespace dom
}  // namespace cobalt
