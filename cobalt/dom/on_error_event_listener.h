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

#ifndef COBALT_DOM_ON_ERROR_EVENT_LISTENER_H_
#define COBALT_DOM_ON_ERROR_EVENT_LISTENER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/event_listener.h"
#include "cobalt/script/union_type.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// This interface exists to support the special case OnErrorEventHandler:
//   https://html.spec.whatwg.org/#onerroreventhandler
class OnErrorEventListener {
 public:
  using EventOrMessage = script::UnionType2<scoped_refptr<Event>, std::string>;

  // The public interface simply accepts the error event, which will be
  // translated to the more complicated IDL handleEvent interface which is
  // marked protected.  During that translation, the event parameters may
  // be unpacked if the |unpack_error_events| flag is true.
  base::Optional<bool> HandleEvent(
      const scoped_refptr<script::Wrappable>& callback_this,
      const scoped_refptr<Event>& event, bool* had_exception,
      bool unpack_error_events) const;

 protected:
  virtual base::Optional<bool> HandleEvent(
      const scoped_refptr<script::Wrappable>& callback_this,
      EventOrMessage message, const std::string& filename, uint32 lineno,
      uint32 colno, const script::ValueHandleHolder* error,
      bool* had_exception) const = 0;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_ON_ERROR_EVENT_LISTENER_H_
