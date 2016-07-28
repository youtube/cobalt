/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/xhr/xml_http_request_event_target.h"

#include "cobalt/base/tokens.h"

namespace cobalt {
namespace xhr {

XMLHttpRequestEventTarget::XMLHttpRequestEventTarget() {}
XMLHttpRequestEventTarget::~XMLHttpRequestEventTarget() {}

const dom::EventTarget::EventListenerScriptObject*
XMLHttpRequestEventTarget::onabort() const {
  return onabort_listener_ ? &onabort_listener_.value().referenced_object()
                           : NULL;
}
const dom::EventTarget::EventListenerScriptObject*
XMLHttpRequestEventTarget::onerror() const {
  return onerror_listener_ ? &onerror_listener_.value().referenced_object()
                           : NULL;
}
const dom::EventTarget::EventListenerScriptObject*
XMLHttpRequestEventTarget::onload() const {
  return onload_listener_ ? &onload_listener_.value().referenced_object()
                          : NULL;
}
const dom::EventTarget::EventListenerScriptObject*
XMLHttpRequestEventTarget::onloadend() const {
  return onloadend_listener_ ? &onloadend_listener_.value().referenced_object()
                             : NULL;
}
const dom::EventTarget::EventListenerScriptObject*
XMLHttpRequestEventTarget::onloadstart() const {
  return onloadstart_listener_
             ? &onloadstart_listener_.value().referenced_object()
             : NULL;
}
const dom::EventTarget::EventListenerScriptObject*
XMLHttpRequestEventTarget::onprogress() const {
  return onprogress_listener_
             ? &onprogress_listener_.value().referenced_object()
             : NULL;
}
const dom::EventTarget::EventListenerScriptObject*
XMLHttpRequestEventTarget::ontimeout() const {
  return ontimeout_listener_ ? &ontimeout_listener_.value().referenced_object()
                             : NULL;
}

void XMLHttpRequestEventTarget::set_onabort(
    const EventListenerScriptObject& listener) {
  onabort_listener_.emplace(this, listener);
  SetAttributeEventListener(base::Tokens::abort(), listener);
}
void XMLHttpRequestEventTarget::set_onerror(
    const EventListenerScriptObject& listener) {
  onerror_listener_.emplace(this, listener);
  SetAttributeEventListener(base::Tokens::error(), listener);
}
void XMLHttpRequestEventTarget::set_onload(
    const EventListenerScriptObject& listener) {
  onload_listener_.emplace(this, listener);
  SetAttributeEventListener(base::Tokens::load(), listener);
}
void XMLHttpRequestEventTarget::set_onloadend(
    const EventListenerScriptObject& listener) {
  onloadend_listener_.emplace(this, listener);
  SetAttributeEventListener(base::Tokens::loadend(), listener);
}
void XMLHttpRequestEventTarget::set_onloadstart(
    const EventListenerScriptObject& listener) {
  onloadstart_listener_.emplace(this, listener);
  SetAttributeEventListener(base::Tokens::loadstart(), listener);
}
void XMLHttpRequestEventTarget::set_onprogress(
    const EventListenerScriptObject& listener) {
  onprogress_listener_.emplace(this, listener);
  SetAttributeEventListener(base::Tokens::progress(), listener);
}
void XMLHttpRequestEventTarget::set_ontimeout(
    const EventListenerScriptObject& listener) {
  ontimeout_listener_.emplace(this, listener);
  SetAttributeEventListener(base::Tokens::timeout(), listener);
}
}  // namespace xhr
}  // namespace cobalt
