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

#include "base/hash.h"
#include "cobalt/trace_event/event_parser.h"

namespace cobalt {
namespace trace_event {

EventParser::ScopedEvent::ScopedEvent(
    const scoped_refptr<ScopedEvent>& parent,
    const base::debug::TraceEvent& begin_event, EventParser* event_parser) {
  event_parser_ = event_parser;
  begin_event_ = begin_event;

  parent_ = parent;
  if (parent_) {
    parent_->children_.push_back(make_scoped_refptr(this));
    ++parent_->flow_active_children_;
  }
  flow_active_children_ = 0;

  state_ = kActiveState;
}

EventParser::ScopedEvent::~ScopedEvent() {}

void EventParser::ScopedEvent::Abort() {
  scoped_refptr<ScopedEvent> abort_event(this);

  // We maintain a list of all events ended so that we can prevent the
  // destructors from recursively calling itself and causing a stack overflow
  // in long chains of flow events.
  std::vector<scoped_refptr<ScopedEvent> > flow_events_aborted;

  while (abort_event) {
    if (abort_event->state_ == kFlowEndedState) {
      break;
    }

    // Report the event as complete, even though we aborted it.  The callee here
    // is responsible for checking if the event values it is interested in are
    // actually available before accessing them.  It is useful to report aborted
    // events because events are only reported as ended when their entire flow
    // has ended, but a benchmark may only be interested in the
    // in-scope-duration, and thus events that are aborted can still provide
    // information to benchmarks.
    abort_event->event_parser_->scoped_event_flow_end_callback_.Run(
        abort_event);

    flow_events_aborted.push_back(abort_event);

    scoped_refptr<ScopedEvent> next_abort_event;
    if (abort_event->parent_) {
      DCHECK_GT(abort_event->parent_->flow_active_children_, 0);
      --abort_event->parent_->flow_active_children_;
      if (abort_event->parent_->flow_active_children_ == 0) {
        next_abort_event = abort_event->parent_;
      }
    }

    abort_event->parent_ = NULL;
    abort_event->state_ = kAbortedState;

    abort_event = next_abort_event;
  }

  // Release our references in last-in-first-out order.  This results in the
  // parents being freed before the children, but it prevents all nodes from
  // recursively being freed when the root node (the top node on the stack
  // here) is released, which can prevent a stack overflow if the ancestry
  // chain is very long.
  while (!flow_events_aborted.empty()) {
    flow_events_aborted.pop_back();
  }
}

const base::debug::TraceEvent& EventParser::ScopedEvent::LastTraceEvent()
    const {
  if (end_event_) return *end_event_;
  if (!instant_events_.empty()) {
    return instant_events_.back();
  }
  return begin_event_;
}

void EventParser::ScopedEvent::OnEnd(
    const base::debug::TraceEvent& end_event) {
  DCHECK(!end_event_);
  DCHECK(end_event.timestamp() >= LastTraceEvent().timestamp());
  end_event_ = end_event;
  if (end_event.phase() == TRACE_EVENT_PHASE_FLOW_END) {
    // If we're ending a flow, it is possible that the flow may still be linked
    // with a TRACE_EVENT that immediately follows it (e.g. like in
    // MessageLoop::RunTask()).  Thus, we have to keep it alive only until
    // another trace event is encountered.  During this time, we leave it in
    // the kEndedAndLastTouchedState state.
    state_ = kEndedAndLastTouchedState;
  } else {
    // Mark the event as ended, and if the conditions are met for this event's
    // flow to also be ended that, trigger the OnEndFlow() processing as well.
    state_ = kEndedState;
    if (AreEndFlowConditionsMet()) {
      OnEndFlow(end_event.timestamp());
    }
  }
}

void EventParser::ScopedEvent::OnEndFlow(const base::TimeTicks& timestamp) {
  scoped_refptr<ScopedEvent> flow_end_event(this);

  // We maintain a list of all events ended so that we can prevent the
  // destructors from recursively calling itself and causing a stack overflow
  // in long chains of flow events.
  std::vector<scoped_refptr<ScopedEvent> > flow_events_ended;

  while (flow_end_event) {
    DCHECK(flow_end_event->AreEndFlowConditionsMet());

    // This ScopedEvent has ended and so have all of its descendants.  Update
    // the end flow timestamp and fire a flow end event to our clients so that
    // they may analyze this completed flow.
    flow_end_event->end_flow_time_ = timestamp;
    flow_end_event->event_parser_->scoped_event_flow_end_callback_.Run(
        flow_end_event);
    flow_events_ended.push_back(flow_end_event);

    // If we have a parent, we should properly decrement its active child count
    // since we are no longer active.  If we are the parent's last active child,
    // we may need to setup the parent as the next |flow_end_event| so that
    // its flow end can also be processed.  This is *not* done recursively on
    // purpose because these flows can end up quite long and we wish to avoid
    // stack overflows.
    scoped_refptr<ScopedEvent> next_flow_end_event;
    if (flow_end_event->parent_) {
      DCHECK_GT(flow_end_event->parent_->flow_active_children_, 0);
      --flow_end_event->parent_->flow_active_children_;
      if (flow_end_event->parent_->AreEndFlowConditionsMet()) {
        next_flow_end_event = flow_end_event->parent_;
      }
    }

    // Mark the parent_ as NULL to clear our reference to it.  This is more
    // a technicality to remove the cyclical dependency between child and parent
    // node once the child has ended its flow.
    flow_end_event->parent_ = NULL;
    flow_end_event->state_ = kFlowEndedState;

    flow_end_event = next_flow_end_event;
  }

  // Release our references in last-in-first-out order.  This results in the
  // parents being freed before the children, but it prevents all nodes from
  // recursively being freed when the root node (the top node on the stack
  // here) is released, which can prevent a stack overflow if the ancestry
  // chain is very long.
  while (!flow_events_ended.empty()) {
    flow_events_ended.pop_back();
  }
}

void EventParser::ScopedEvent::OnNotLastTouched() {
  if (state_ == kEndedAndLastTouchedState) {
    if (AreEndFlowConditionsMet()) {
      OnEndFlow(end_event_->timestamp());
    }
  }
}

void EventParser::ScopedEvent::AddInstantEvent(
    const base::debug::TraceEvent& event) {
  DCHECK(event.timestamp() >= LastTraceEvent().timestamp());
  instant_events_.push_back(event);
}

bool EventParser::ScopedEvent::AreEndFlowConditionsMet() const {
  // These conditions represent when we are able to begin the OnEndFlow()
  // processing to mark our flow as ended.
  return state_ != kFlowEndedState && state_ != kActiveState &&
         flow_active_children_ == 0 && end_event_;
}

std::set<scoped_refptr<EventParser::ScopedEvent> >
EventParser::ScopedEvent::GetLeafEvents() {
  std::set<scoped_refptr<ScopedEvent> > results;
  if (children_.empty()) {
    results.insert(this);
  } else {
    for (size_t i = 0; i < children_.size(); ++i) {
      std::set<scoped_refptr<ScopedEvent> > sub_results =
          children_[i]->GetLeafEvents();
      results.insert(sub_results.begin(), sub_results.end());
    }
  }
  return results;
}

EventParser::EventParser(
    const ScopedEventFlowEndCallback& scoped_event_flow_end_callback) {
  scoped_event_flow_end_callback_ = scoped_event_flow_end_callback;
}

EventParser::~EventParser() {
  // Knowing that there will be no future events, clear all existing events of
  // "last touched" status as this may permit them to end normally.
  for (ThreadMap::iterator thread_iter = thread_id_to_info_map_.begin();
       thread_iter != thread_id_to_info_map_.end(); ++thread_iter) {
    UpdateLastTouchedEvent(&thread_iter->second, NULL);
  }

  // Abort all leaf events.  When the last of a parent node's children is
  // aborted, the parent node will itself be aborted and thus aborting all
  // the leaf nodes will result in all nodes being aborted.
  std::set<scoped_refptr<ScopedEvent> > leaf_events =
      EventParser::GetLeafEvents();

  for (std::set<scoped_refptr<ScopedEvent> >::const_iterator iter =
           leaf_events.begin();
       iter != leaf_events.end(); ++iter) {
    (*iter)->Abort();
  }
}

std::set<scoped_refptr<EventParser::ScopedEvent> >
EventParser::GetLeafEvents() {
  // Go through all root event handles we have and obtain the leaf nodes from
  // each of their event flow trees.  Combine them all together into a set
  // and return this set of all leaf event nodes.
  std::set<scoped_refptr<ScopedEvent> > leaf_events;

  // First go through all flow event handles.
  for (FlowEventMap::iterator flow_iter = flow_id_to_event_map_.begin();
       flow_iter != flow_id_to_event_map_.end(); ++flow_iter) {
    scoped_refptr<ScopedEvent> current_flow_event = flow_iter->second;
    std::set<scoped_refptr<ScopedEvent> > current_leaf_events =
        current_flow_event->GetLeafEvents();
    leaf_events.insert(current_leaf_events.begin(), current_leaf_events.end());
  }

  // Then go through all thread stacks.
  for (ThreadMap::iterator thread_iter = thread_id_to_info_map_.begin();
       thread_iter != thread_id_to_info_map_.end(); ++thread_iter) {
    std::vector<scoped_refptr<ScopedEvent> >* current_event_stack =
        &thread_iter->second.event_stack_;
    if (!current_event_stack->empty()) {
      // We only need to look at the last element of each stack, since we know
      // that everything on top of that element is a parent of the element under
      // it (and hence not a leaf node).
      std::set<scoped_refptr<ScopedEvent> > current_leaf_events =
          current_event_stack->back()->GetLeafEvents();
      leaf_events.insert(current_leaf_events.begin(),
                         current_leaf_events.end());
    }
  }

  return leaf_events;
}

namespace {

// This function mangles the event's specified Id with the event name in order
// to produce a new id that is unique to both name and user-provided id.  It is
// specified by trace_event.h that flow events should only be linked together
// when both their name and user provided ids match.
uint64_t MakeFlowId(const base::debug::TraceEvent& event) {
  // The least significant end of both the event id and event name typically
  // change more than the most significant end, so mix the most significant
  // end of one with the least significant end of the other to better ensure
  // uniqueness.
  return static_cast<uint64_t>(event.id() << 32) ^
         (static_cast<uint64_t>(event.id() >> 32) & 0xFFFFFFFF) ^
         static_cast<uint64_t>(base::SuperFastHash(
             event.name(), strlen(event.name())));
}

}  // namespace

void EventParser::ParseEvent(const base::debug::TraceEvent& event) {
  // Get a reference to the thread context we are maintaining for the thread
  // this incoming event is linked to.
  ThreadInfo* thread_info = &thread_id_to_info_map_[event.thread_id()];

  switch (event.phase()) {
    case TRACE_EVENT_PHASE_BEGIN: {
      // Handle the case of a call to TRACE_EVENT occurring.  First, figure
      // out who its parent should be.  If this call follows a TRACE_EVENT_FLOW
      // call and matches in name, it will be parented to to that
      // TRACE_EVENT_FLOW allowing flow chains to cross multiple thread
      // boundaries.  Othewrise, it will be parented to the top TRACE_EVENT
      // on the event thread's event stack.
      scoped_refptr<ScopedEvent> parent;
      if (thread_info->last_touched_event_) {
        // Check if we should link this TRACE_EVENT log event with a directly
        // preceding TRACE_EVENT_FLOW log event.
        const base::debug::TraceEvent& trace_event =
            thread_info->last_touched_event_->LastTraceEvent();

        switch (trace_event.phase()) {
          case TRACE_EVENT_PHASE_FLOW_STEP: {
            // We would like to be able to scope TRACE_EVENT_FLOW_STEP calls,
            // so we do this by linking together a scoped TRACE_EVENT that
            // immediately follows a TRACE_EVENT_FLOW_STOP call.
            if (strcmp(trace_event.arg_values()[0].as_string, event.name()) ==
                0) {
              parent = thread_info->last_touched_event_;
            }
          } break;
          case TRACE_EVENT_PHASE_FLOW_END: {
            // This is a special case to support linking together MessageLoop
            // messages.  In particular, it is catered to the pattern of
            // trace event calls found in MessageLoop::RunTask.
            if (strcmp(trace_event.name(), "MessageLoop::PostTask") == 0 &&
                strcmp(event.name(), "MessageLoop::RunTask") == 0) {
              parent = thread_info->last_touched_event_;
            }
          } break;
        }
      }
      if (!parent && !thread_info->event_stack_.empty()) {
        // Setup our parent as the top event on the current thread's event
        // stack.
        parent = thread_info->event_stack_.back();
      }
      scoped_refptr<ScopedEvent> new_event =
          make_scoped_refptr(new ScopedEvent(parent, event, this));
      UpdateLastTouchedEvent(thread_info, new_event);
      // Make this the top event on the current thread's event stack
      thread_info->event_stack_.push_back(new_event);
    } break;

    case TRACE_EVENT_PHASE_END: {
      // If the thread's stack is empty, we were not tracking when the currently
      // ending scope had begun, so we ignore it.
      if (!thread_info->event_stack_.empty()) {
        // A TRACE_EVENT's scope has closed, ending the event.
        scoped_refptr<ScopedEvent> ended_event =
            thread_info->event_stack_.back();
        UpdateLastTouchedEvent(thread_info, ended_event);
        DCHECK_EQ(std::string(event.name()), ended_event->begin_event().name());

        // Mark the event as ended.
        ended_event->OnEnd(event);
        thread_info->event_stack_.pop_back();
      }
    } break;

    case TRACE_EVENT_PHASE_FLOW_BEGIN: {
      // A call to TRACE_EVENT_FLOW_BEGIN has been made.  Here we create a new
      // flow identified by a unique id.  It is only associated with the
      // current thread by it being parented to the top TRACE_EVENT on the
      // current thread's event stack, if there are any.
      scoped_refptr<ScopedEvent> parent;
      if (!thread_info->event_stack_.empty()) {
        parent = thread_info->event_stack_.back();
      }
      scoped_refptr<ScopedEvent> new_event =
          make_scoped_refptr(new ScopedEvent(parent, event, this));
      UpdateLastTouchedEvent(thread_info, new_event);
      DCHECK(flow_id_to_event_map_.find(MakeFlowId(event)) ==
                 flow_id_to_event_map_.end());
      // Save the event in the event map.
      flow_id_to_event_map_[MakeFlowId(event)] = new_event;
    } break;

    case TRACE_EVENT_PHASE_FLOW_STEP: {
      // Respond to a TRACE_EVENT_FLOW_STEP call.  When UpdateLastTouchedEvent()
      // is called for this event, it may end up being a parent if the next
      // call to TRACE_EVENT shares the same name as this event's step's name.
      // We only process this event if we have tracked the beginning of the
      // event.
      if (flow_id_to_event_map_.find(MakeFlowId(event)) !=
          flow_id_to_event_map_.end()) {
        scoped_refptr<ScopedEvent> flow_event =
            flow_id_to_event_map_[MakeFlowId(event)];
        UpdateLastTouchedEvent(thread_info, flow_event);
        DCHECK_EQ(std::string(event.name()), flow_event->begin_event().name());
        flow_event->AddInstantEvent(event);
      }
    } break;

    case TRACE_EVENT_PHASE_FLOW_END: {
      // Respond to TRACE_EVENT_FLOW_END.  When UpdateLastTouchedEvent() is
      // called, it may result in this flow being a parent if the next
      // call to TRACE_EVENT is "MessageLoop::RunTask" and this flow's name
      // is "MessageLoop::PostTask".  This allows linking flows across
      // message loops.
      FlowEventMap::iterator found =
          flow_id_to_event_map_.find(MakeFlowId(event));
      // Only process this flow end event if we were tracking when the beginning
      // of the flow took place.
      if (found != flow_id_to_event_map_.end()) {
        scoped_refptr<ScopedEvent> flow_event = found->second;
        UpdateLastTouchedEvent(thread_info, flow_event);
        DCHECK_EQ(std::string(event.name()), flow_event->begin_event().name());
        flow_event->OnEnd(event);
        flow_id_to_event_map_.erase(found);
      }
    } break;
  }
}

void EventParser::UpdateLastTouchedEvent(
    ThreadInfo* thread_info, const scoped_refptr<ScopedEvent>& event) {
  // Reset the provided thread's last touched event.  A last touched event
  // is whatever event was last processed (e.g. a TRACE_EVENT start or end or
  // a TRACE_EVENT_FLOW start, step or end.)
  if (thread_info->last_touched_event_) {
    thread_info->last_touched_event_->OnNotLastTouched();
  }
  thread_info->last_touched_event_ = event;
}

}  // namespace trace_event
}  // namespace cobalt
