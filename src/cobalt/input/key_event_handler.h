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

#ifndef COBALT_INPUT_KEY_EVENT_HANDLER_H_
#define COBALT_INPUT_KEY_EVENT_HANDLER_H_

#include "base/callback.h"
#include "cobalt/dom/keyboard_event.h"

namespace cobalt {
namespace input {

typedef base::Callback<void(const dom::KeyboardEvent::Data&)>
    KeyboardEventCallback;

// Base class for objects that can process keyboard events.
// This includes input device manangers and key event filters.
// Objects of this class can either handle the key event directly using the
// callback provided at construction, or pass the event on to a filter.
// This allows filters to be chained together to implement the combined
// functionality of multiple filters.
class KeyEventHandler {
 public:
  // Called to handle a key event. The base class implementation just calls
  // DispatchKeyboardEvent. Overridden versions of this function may modify the
  // incoming event and/or generate new events. Overriden versions should call
  // DispatchKeyboardEvent for each event filtered/produced.
  virtual void HandleKeyboardEvent(const dom::KeyboardEvent::Data& event);

 protected:
  explicit KeyEventHandler(const KeyboardEventCallback& callback);

  explicit KeyEventHandler(KeyEventHandler* filter);

  virtual ~KeyEventHandler() {}

  // Called to dispatch a key event. The event will either be sent to the key
  // event filter attached to this object or passed directly to the stored
  // callback function if there is no attached filter.
  void DispatchKeyboardEvent(const dom::KeyboardEvent::Data& event) const;

 private:
  // The event callback should not be called directly by subclasses.
  // Instead, use the HandleKeyboardEvent function, which may do further
  // processing, and/or pass the event on to the attached filter.
  KeyboardEventCallback keyboard_event_callback_;

  // Another KeyEventHandler object to filter the key events processed by
  // this object. If not defined (NULL), then the keyboard event callback is
  // called directly. The pointer is used as a NULLable reference - this class
  // has no ownership of the filter object.
  KeyEventHandler* keyboard_event_filter_;
};

}  // namespace input
}  // namespace cobalt

#endif  // COBALT_INPUT_KEY_EVENT_HANDLER_H_
