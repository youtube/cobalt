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

  const EventListenerScriptObject* onabort() const;
  const EventListenerScriptObject* onerror() const;
  const EventListenerScriptObject* onload() const;
  const EventListenerScriptObject* onloadend() const;
  const EventListenerScriptObject* onloadstart() const;
  const EventListenerScriptObject* onprogress() const;
  const EventListenerScriptObject* ontimeout() const;

  void set_onabort(const EventListenerScriptObject& listener);
  void set_onerror(const EventListenerScriptObject& listener);
  void set_onload(const EventListenerScriptObject& listener);
  void set_onloadend(const EventListenerScriptObject& listener);
  void set_onloadstart(const EventListenerScriptObject& listener);
  void set_onprogress(const EventListenerScriptObject& listener);
  void set_ontimeout(const EventListenerScriptObject& listener);

  DEFINE_WRAPPABLE_TYPE(XMLHttpRequestEventTarget);

 protected:
  ~XMLHttpRequestEventTarget() OVERRIDE;

  base::optional<EventListenerScriptObject::Reference> onabort_listener_;
  base::optional<EventListenerScriptObject::Reference> onerror_listener_;
  base::optional<EventListenerScriptObject::Reference> onload_listener_;
  base::optional<EventListenerScriptObject::Reference> onloadend_listener_;
  base::optional<EventListenerScriptObject::Reference> onloadstart_listener_;
  base::optional<EventListenerScriptObject::Reference> onprogress_listener_;
  base::optional<EventListenerScriptObject::Reference> ontimeout_listener_;

 private:
  // From EventTarget.
  std::string GetDebugName() OVERRIDE { return "XMLHttpRequestEventTarget"; }

  DISALLOW_COPY_AND_ASSIGN(XMLHttpRequestEventTarget);
};

}  // namespace xhr
}  // namespace cobalt

#endif  // COBALT_XHR_XML_HTTP_REQUEST_EVENT_TARGET_H_
