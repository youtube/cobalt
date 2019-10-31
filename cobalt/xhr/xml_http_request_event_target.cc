// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/xhr/xml_http_request_event_target.h"

#include "cobalt/base/tokens.h"

namespace cobalt {
namespace xhr {

XMLHttpRequestEventTarget::XMLHttpRequestEventTarget(
    script::EnvironmentSettings* settings)
    : EventTarget(settings) {}
XMLHttpRequestEventTarget::~XMLHttpRequestEventTarget() {}

const dom::EventTarget::EventListenerScriptValue*
XMLHttpRequestEventTarget::onabort() const {
  return onabort_listener_ ? &onabort_listener_.value().referenced_value()
                           : NULL;
}
const dom::EventTarget::EventListenerScriptValue*
XMLHttpRequestEventTarget::onerror() const {
  return onerror_listener_ ? &onerror_listener_.value().referenced_value()
                           : NULL;
}
const dom::EventTarget::EventListenerScriptValue*
XMLHttpRequestEventTarget::onload() const {
  return onload_listener_ ? &onload_listener_.value().referenced_value() : NULL;
}
const dom::EventTarget::EventListenerScriptValue*
XMLHttpRequestEventTarget::onloadend() const {
  return onloadend_listener_ ? &onloadend_listener_.value().referenced_value()
                             : NULL;
}
const dom::EventTarget::EventListenerScriptValue*
XMLHttpRequestEventTarget::onloadstart() const {
  return onloadstart_listener_
             ? &onloadstart_listener_.value().referenced_value()
             : NULL;
}
const dom::EventTarget::EventListenerScriptValue*
XMLHttpRequestEventTarget::onprogress() const {
  return onprogress_listener_ ? &onprogress_listener_.value().referenced_value()
                              : NULL;
}
const dom::EventTarget::EventListenerScriptValue*
XMLHttpRequestEventTarget::ontimeout() const {
  return ontimeout_listener_ ? &ontimeout_listener_.value().referenced_value()
                             : NULL;
}

void XMLHttpRequestEventTarget::set_onabort(
    const EventListenerScriptValue& listener) {
  if (listener.IsNull()) {
    onabort_listener_ = base::nullopt;
  } else {
    onabort_listener_.emplace(this, listener);
  }
  SetAttributeEventListener(base::Tokens::abort(), listener);
}
void XMLHttpRequestEventTarget::set_onerror(
    const EventListenerScriptValue& listener) {
  if (listener.IsNull()) {
    onerror_listener_ = base::nullopt;
  } else {
    onerror_listener_.emplace(this, listener);
  }
  SetAttributeEventListener(base::Tokens::error(), listener);
}
void XMLHttpRequestEventTarget::set_onload(
    const EventListenerScriptValue& listener) {
  if (listener.IsNull()) {
    onload_listener_ = base::nullopt;
  } else {
    onload_listener_.emplace(this, listener);
  }
  SetAttributeEventListener(base::Tokens::load(), listener);
}
void XMLHttpRequestEventTarget::set_onloadend(
    const EventListenerScriptValue& listener) {
  if (listener.IsNull()) {
    onloadend_listener_ = base::nullopt;
  } else {
    onloadend_listener_.emplace(this, listener);
  }
  SetAttributeEventListener(base::Tokens::loadend(), listener);
}
void XMLHttpRequestEventTarget::set_onloadstart(
    const EventListenerScriptValue& listener) {
  if (listener.IsNull()) {
    onloadstart_listener_ = base::nullopt;
  } else {
    onloadstart_listener_.emplace(this, listener);
  }
  SetAttributeEventListener(base::Tokens::loadstart(), listener);
}
void XMLHttpRequestEventTarget::set_onprogress(
    const EventListenerScriptValue& listener) {
  if (listener.IsNull()) {
    onprogress_listener_ = base::nullopt;
  } else {
    onprogress_listener_.emplace(this, listener);
  }
  SetAttributeEventListener(base::Tokens::progress(), listener);
}
void XMLHttpRequestEventTarget::set_ontimeout(
    const EventListenerScriptValue& listener) {
  if (listener.IsNull()) {
    ontimeout_listener_ = base::nullopt;
  } else {
    ontimeout_listener_.emplace(this, listener);
  }
  SetAttributeEventListener(base::Tokens::timeout(), listener);
}

void XMLHttpRequestEventTarget::TraceMembers(script::Tracer* tracer) {
  dom::EventTarget::TraceMembers(tracer);
}

}  // namespace xhr
}  // namespace cobalt
