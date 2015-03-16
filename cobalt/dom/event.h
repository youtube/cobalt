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

#ifndef DOM_EVENT_H_
#define DOM_EVENT_H_

#include "base/memory/weak_ptr.h"
#include "base/string_piece.h"
#include "cobalt/dom/event_wrapper_creator.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The forward declaration is necessary for breaking the bi-directional
// dependency between Event and EventTarget.
class EventTarget;

// The Event interface can be passed from the event target to event listener to
// pass information between them.
//   http://www.w3.org/TR/2014/WD-dom-20140710/#interface-event
//
// TODO(b/19335817): We only support the attributes/methods that are in use.
// We need to investigate the exact subset of them required in Cobalt.
class Event : public script::Wrappable {
 public:
  explicit Event(const std::string& type);

  // Web API: Event
  //
  const std::string& type() const { return type_; }
  const scoped_refptr<EventTarget>& target() const;
  void PreventDefault() { default_prevented_ = true; }
  bool default_prevented() const { return default_prevented_; }

  // Custom, not in any spec.
  //
  void SetTarget(const scoped_refptr<EventTarget>& target);

  // Custom, not in any spec.
  //
  virtual scoped_ptr<script::ScriptObjectHandle> CreateWrapper(
      EventWrapperCreator* creator) {
    DCHECK(!get_wrapper_handle());
    return creator->CreateWrapper(this);
  }

 private:
  std::string type_;
  scoped_refptr<EventTarget> target_;
  bool default_prevented_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_EVENT_H_
