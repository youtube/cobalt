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

#ifndef COBALT_XHR_XML_HTTP_REQUEST_EVENT_TARGET_H_
#define COBALT_XHR_XML_HTTP_REQUEST_EVENT_TARGET_H_

#include <string>

#include "base/optional.h"
#include "cobalt/dom/event_target.h"

namespace cobalt {
namespace xhr {

class XMLHttpRequestEventTarget : public dom::EventTarget {
 public:
  XMLHttpRequestEventTarget();

  const EventListenerScriptValue* onabort() const;
  const EventListenerScriptValue* onerror() const;
  const EventListenerScriptValue* onload() const;
  const EventListenerScriptValue* onloadend() const;
  const EventListenerScriptValue* onloadstart() const;
  const EventListenerScriptValue* onprogress() const;
  const EventListenerScriptValue* ontimeout() const;

  void set_onabort(const EventListenerScriptValue& listener);
  void set_onerror(const EventListenerScriptValue& listener);
  void set_onload(const EventListenerScriptValue& listener);
  void set_onloadend(const EventListenerScriptValue& listener);
  void set_onloadstart(const EventListenerScriptValue& listener);
  void set_onprogress(const EventListenerScriptValue& listener);
  void set_ontimeout(const EventListenerScriptValue& listener);

  DEFINE_WRAPPABLE_TYPE(XMLHttpRequestEventTarget);
  void TraceMembers(script::Tracer* tracer) override;

 protected:
  ~XMLHttpRequestEventTarget() override;

  base::optional<EventListenerScriptValue::Reference> onabort_listener_;
  base::optional<EventListenerScriptValue::Reference> onerror_listener_;
  base::optional<EventListenerScriptValue::Reference> onload_listener_;
  base::optional<EventListenerScriptValue::Reference> onloadend_listener_;
  base::optional<EventListenerScriptValue::Reference> onloadstart_listener_;
  base::optional<EventListenerScriptValue::Reference> onprogress_listener_;
  base::optional<EventListenerScriptValue::Reference> ontimeout_listener_;

 private:
  // From EventTarget.
  std::string GetDebugName() override { return "XMLHttpRequestEventTarget"; }

  DISALLOW_COPY_AND_ASSIGN(XMLHttpRequestEventTarget);
};

}  // namespace xhr
}  // namespace cobalt

#endif  // COBALT_XHR_XML_HTTP_REQUEST_EVENT_TARGET_H_
