// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_EVENT_H_
#define COBALT_DOM_EVENT_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "cobalt/base/token.h"
#include "cobalt/dom/event_init.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The forward declaration is necessary for breaking the bi-directional
// dependency between Event and EventTarget.
class EventTarget;

// The Event interface can be passed from the event target to event listener to
// pass information between them.
//   https://www.w3.org/TR/2014/WD-dom-20140710/#interface-event
//
// TODO: We only support the attributes/methods that are in use.
// We need to investigate the exact subset of them required in Cobalt.
class Event : public script::Wrappable {
 public:
  // Web API: Event
  // EventPhase values as defined by Web API Event.eventPhase.
  //
  typedef uint16 EventPhase;
  enum EventPhaseInternal : uint16 {
    kNone,
    kCapturingPhase,
    kAtTarget,
    kBubblingPhase
  };

  // Custom, not in any spec.
  //
  enum Bubbles { kNotBubbles, kBubbles };

  enum Cancelable { kNotCancelable, kCancelable };

  enum UninitializedFlag { Uninitialized };

  // Creates an event with its "initialized flag" unset.
  explicit Event(UninitializedFlag uninitialized_flag);

  // Creates an event that cannot be bubbled and cancelled.
  explicit Event(const std::string& type);
  explicit Event(base::Token type);
  Event(const std::string& type, const EventInit& init_dict);
  Event(base::Token type, Bubbles bubbles, Cancelable cancelable);
  Event(base::Token type, const EventInit& init_dict);

  ~Event() override;

  // Web API: Event
  //
  base::Token type() const { return type_; }
  const scoped_refptr<EventTarget>& target() const;
  const scoped_refptr<EventTarget>& current_target() const;
  EventPhase event_phase() const { return event_phase_; }

  void StopPropagation() { propagation_stopped_ = true; }
  void StopImmediatePropagation() {
    propagation_stopped_ = true;
    immediate_propagation_stopped_ = true;
  }

  bool bubbles() const { return bubbles_; }
  bool cancelable() const { return cancelable_; }
  void PreventDefault() {
    if (cancelable()) {
      default_prevented_ = true;
    }
  }
  bool default_prevented() const { return default_prevented_; }

  uint64 time_stamp() const { return time_stamp_; }

  void InitEvent(const std::string& type, bool bubbles, bool cancelable);

  // Custom, not in any spec.
  //
  bool IsBeingDispatched() const { return event_phase() != kNone; }
  void set_target(const scoped_refptr<EventTarget>& target);
  void set_current_target(const scoped_refptr<EventTarget>& target);
  void set_event_phase(EventPhase event_phase) { event_phase_ = event_phase; }
  // We link whether the event has the "initialized flag" set to whether it has
  // a non-empty type.  Note that the spec doesn't explicitly prevent an Event
  // to have empty string as its type but it is reasonable to make this
  // assumption and Chrome has the same behavior.
  bool initialized_flag() const { return type_ != ""; }

  // The event dispatching process usually looks like:
  //
  // for (each event target) {
  //   for (each event listener in the current target) {
  //     listener->handleEvent(event);
  //     if (event->immediate_propagation_stopped()) break;
  //   }
  //   if (event->propagation_stopped()) break;
  // }
  //
  // When propagation_stopped() returns true, the inside loop will continue but
  // the outside loop will break.
  bool propagation_stopped() const { return propagation_stopped_; }
  // When immediate_propagation_stopped() returns true, the inside loop will
  // break. As StopImmediatePropagation() will also set propagation_stopped_ to
  // true. This implies the outside loop will be break too.
  bool immediate_propagation_stopped() const {
    return immediate_propagation_stopped_;
  }

  void TraceMembers(script::Tracer* tracer) override;

  DEFINE_WRAPPABLE_TYPE(Event);

 private:
  void InitEventInternal(base::Token type, bool bubbles, bool cancelable);

  base::Token type_;

  scoped_refptr<EventTarget> target_;
  scoped_refptr<EventTarget> current_target_;
  EventPhase event_phase_;

  bool propagation_stopped_;
  bool immediate_propagation_stopped_;

  bool bubbles_;
  bool cancelable_;
  bool default_prevented_;

  uint64 time_stamp_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_EVENT_H_
