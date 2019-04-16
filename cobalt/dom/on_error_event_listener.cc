// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/on_error_event_listener.h"

#include <string>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/error_event.h"
#include "cobalt/dom/event_target.h"

namespace cobalt {
namespace dom {

base::Optional<bool> OnErrorEventListener::HandleEvent(
    const scoped_refptr<script::Wrappable>& callback_this,
    const scoped_refptr<Event>& event, bool* had_exception,
    bool unpack_error_events) const {
  if (unpack_error_events) {
    // Only ErrorEvents should be dispatched to OnErrorEventListeners.
    ErrorEvent* error_event =
        base::polymorphic_downcast<ErrorEvent*>(event.get());

    // Unpack the error event into its components and pass those down.
    return HandleEvent(callback_this, EventOrMessage(error_event->message()),
                       error_event->filename(), error_event->lineno(),
                       error_event->colno(), error_event->error(),
                       had_exception);
  } else {
    return HandleEvent(callback_this, EventOrMessage(event), std::string(), 0,
                       0, NULL, had_exception);
  }
}

}  // namespace dom
}  // namespace cobalt
