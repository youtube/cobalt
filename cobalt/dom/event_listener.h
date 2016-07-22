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

#ifndef COBALT_DOM_EVENT_LISTENER_H_
#define COBALT_DOM_EVENT_LISTENER_H_

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/dom/event.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The EventListener interface represents a callable object that will be called
// when an event is fired.
//   https://www.w3.org/TR/2014/WD-dom-20140710/#eventtarget
class EventListener {
 public:
  // EventHandlers are implemented as EventListener?, so use this to
  // differentiate between the two.
  enum Type {
    kAttribute,
    kNotAttribute,
  };

  // Custom, not in any spec.
  //
  // Specify whether this is an attribute-type listener or not.
  void HandleEvent(const scoped_refptr<Event>& event, Type listener_type) const;

 protected:
  // Web API: EventListener
  //
  // Cobalt's implementation of callback interfaces requires the 'callback this'
  // to be explicitly passed in.
  virtual base::optional<bool> HandleEvent(
      const scoped_refptr<script::Wrappable>& callback_this,
      const scoped_refptr<Event>& event, bool* had_exception) const = 0;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_EVENT_LISTENER_H_
