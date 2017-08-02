// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_STARBOARD_EVENT_HANDLER_H_
#define COBALT_BROWSER_STARBOARD_EVENT_HANDLER_H_

#include "cobalt/base/event_dispatcher.h"
#include "starboard/event.h"

namespace cobalt {
namespace browser {

class EventHandler {
 public:
  explicit EventHandler(base::EventDispatcher* event_dispatcher);

  // Static event handler called by |SbEventHandle|. Forwards input events to
  // |system_window::HandleInputEvent| and passes all other events to
  // |DispatchEvent|.
  static void HandleEvent(const SbEvent* event);

 private:
  // Creates a Cobalt event from a Starboard event and dispatches to the rest
  // of the system via |event_dispatcher_|.
  void DispatchEvent(const SbEvent* event) const;

  void DispatchEventInternal(base::Event*) const;

  // The event dispatcher that dispatches Cobalt events to the rest of the
  // system.
  base::EventDispatcher* event_dispatcher_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_STARBOARD_EVENT_HANDLER_H_
