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

#include "cobalt/dom/event.h"

#include "base/time.h"
#include "cobalt/dom/event_target.h"

namespace cobalt {
namespace dom {

Event::Event(const std::string& type)
    : event_phase_(kNone), time_stamp_(base::Time::Now().ToJsTime()) {
  InitEvent(type, false, false);
}

Event::Event(const std::string& type, Bubbles bubbles, Cancelable cancelable)
    : event_phase_(kNone), time_stamp_(base::Time::Now().ToJsTime()) {
  InitEvent(type, bubbles == kBubbles, cancelable == kCancelable);
}

const scoped_refptr<EventTarget>& Event::target() const { return target_; }

const scoped_refptr<EventTarget>& Event::current_target() const {
  return current_target_;
}

void Event::InitEvent(const std::string& type, bool bubbles, bool cancelable) {
  // Our event is for single use only.
  DCHECK(!IsBeingDispatched());
  DCHECK(!target());
  DCHECK(!current_target());

  static bool first_time = true;
  DLOG_IF(WARNING, first_time) << "Event.initEvent() is deprecated.";
  first_time = false;

  if (IsBeingDispatched() || target() || current_target()) {
    return;
  }

  type_ = type;
  bubbles_ = bubbles;
  cancelable_ = cancelable;

  propagation_stopped_ = false;
  immediate_propagation_stopped_ = false;
  default_prevented_ = false;
}

void Event::set_target(const scoped_refptr<EventTarget>& target) {
  target_ = target;
}

void Event::set_current_target(const scoped_refptr<EventTarget>& target) {
  current_target_ = target;
}

}  // namespace dom
}  // namespace cobalt
