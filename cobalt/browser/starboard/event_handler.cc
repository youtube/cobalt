/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/starboard/event_handler.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/system_window/application_event.h"
#include "cobalt/system_window/starboard/system_window.h"

namespace cobalt {
namespace browser {

namespace {
EventHandler* g_the_event_handler = NULL;
}  // namespace

EventHandler::EventHandler(base::EventDispatcher* event_dispatcher)
    : event_dispatcher_(event_dispatcher) {
  DCHECK(!g_the_event_handler) << "There should be only one event handler.";
  g_the_event_handler = this;
}

// static
void EventHandler::HandleEvent(const SbEvent* starboard_event) {
  DCHECK(starboard_event);

  // Forward input events to |SystemWindow|.
  if (starboard_event->type == kSbEventTypeInput) {
    system_window::HandleInputEvent(starboard_event);
    return;
  }

  // Handle all other events internally.
  DCHECK(g_the_event_handler);
  g_the_event_handler->DispatchEvent(starboard_event);
}

void EventHandler::DispatchEvent(const SbEvent* starboard_event) const {
  // Create a Cobalt event from the Starboard event, if recognized.
  scoped_ptr<base::Event> cobalt_event;
  if (starboard_event->type == kSbEventTypeResume) {
    cobalt_event.reset(new system_window::ApplicationEvent(
        system_window::ApplicationEvent::kResume));
  } else if (starboard_event->type == kSbEventTypeSuspend) {
    cobalt_event.reset(new system_window::ApplicationEvent(
        system_window::ApplicationEvent::kSuspend));
  }

  // Dispatch the Cobalt event, if created.
  if (cobalt_event) {
    event_dispatcher_->DispatchEvent(cobalt_event.Pass());
  } else {
    DLOG(WARNING) << "Unhandled Starboard event of type: "
                  << starboard_event->type;
  }
}

}  // namespace browser
}  // namespace cobalt
