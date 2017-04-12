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

#include "cobalt/browser/starboard/event_handler.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/accessibility_settings_changed_event.h"
#include "cobalt/base/deep_link_event.h"
#include "cobalt/network/network_event.h"
#include "cobalt/system_window/application_event.h"
#include "cobalt/system_window/input_event.h"

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

void EventHandler::DispatchEventInternal(base::Event* event) const {
  event_dispatcher_->DispatchEvent(make_scoped_ptr<base::Event>(event));
}

void EventHandler::DispatchEvent(const SbEvent* starboard_event) const {
  // Create a Cobalt event from the Starboard event, if recognized.
  switch (starboard_event->type) {
    case kSbEventTypePause:
      DispatchEventInternal(new system_window::ApplicationEvent(
          system_window::ApplicationEvent::kPause));
      break;
    case kSbEventTypeUnpause:
      DispatchEventInternal(new system_window::ApplicationEvent(
          system_window::ApplicationEvent::kUnpause));
      break;
    case kSbEventTypeSuspend:
      DispatchEventInternal(new system_window::ApplicationEvent(
          system_window::ApplicationEvent::kSuspend));
      break;
    case kSbEventTypeResume:
      DispatchEventInternal(new system_window::ApplicationEvent(
          system_window::ApplicationEvent::kResume));
      break;
    case kSbEventTypeNetworkConnect:
      DispatchEventInternal(
          new network::NetworkEvent(network::NetworkEvent::kConnection));
      break;
    case kSbEventTypeNetworkDisconnect:
      DispatchEventInternal(
          new network::NetworkEvent(network::NetworkEvent::kDisconnection));
      break;
    case kSbEventTypeLink: {
      const char* link = static_cast<const char*>(starboard_event->data);
      DispatchEventInternal(new base::DeepLinkEvent(link));
      break;
    }
#if SB_API_VERSION >= 4
    case kSbEventTypeAccessiblitySettingsChanged:
      DispatchEventInternal(new base::AccessibilitySettingsChangedEvent());
      break;
#endif  // SB_API_VERSION >= 4
    default:
      DLOG(WARNING) << "Unhandled Starboard event of type: "
                    << starboard_event->type;
  }
}

}  // namespace browser
}  // namespace cobalt
