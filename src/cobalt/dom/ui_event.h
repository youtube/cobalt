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

#ifndef COBALT_DOM_UI_EVENT_H_
#define COBALT_DOM_UI_EVENT_H_

#include <string>

#include "cobalt/dom/document.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/ui_event_init.h"
#include "cobalt/dom/window.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The UIEvent provides specific contextual information associated with User
// Interface events.
//   https://www.w3.org/TR/2016/WD-uievents-20160804/#events-uievents
class UIEvent : public Event {
 public:
  explicit UIEvent(const std::string& type);
  UIEvent(const std::string& type, const UIEventInit& init_dict);

  // Creates an event with its "initialized flag" unset.
  explicit UIEvent(UninitializedFlag uninitialized_flag);

  UIEvent(base::Token type, Bubbles bubbles, Cancelable cancelable,
          const scoped_refptr<Window>& view);

  // Web API: UIEvent
  //
  void InitUIEvent(const std::string& type, bool bubbles, bool cancelable,
                   const scoped_refptr<Window>& view, int32 detail);

  const scoped_refptr<Window>& view() const { return view_; }
  int32 detail() const { return detail_; }

  // The following property is defined inside UIEvent but is not valid for all
  // UIEvent subtypes.  Subtypes should override the getter when necessary.
  virtual uint32 which() const { return which_; }

  DEFINE_WRAPPABLE_TYPE(UIEvent);

 protected:
  explicit UIEvent(base::Token type);
  explicit UIEvent(base::Token type, Bubbles bubbles, Cancelable cancelable,
                   const scoped_refptr<Window>& view,
                   const UIEventInit& init_dict);

  ~UIEvent() override {}

  scoped_refptr<Window> view_;
  int32 detail_;
  uint32 which_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_UI_EVENT_H_
