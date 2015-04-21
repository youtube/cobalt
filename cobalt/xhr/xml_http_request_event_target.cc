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

#include "cobalt/dom/event_names.h"

namespace cobalt {
namespace xhr {

XMLHttpRequestEventTarget::XMLHttpRequestEventTarget() {}
XMLHttpRequestEventTarget::~XMLHttpRequestEventTarget() {}

scoped_refptr<dom::EventListener> XMLHttpRequestEventTarget::onabort() const {
  return onabort_listener_;
}
scoped_refptr<dom::EventListener> XMLHttpRequestEventTarget::onerror() const {
  return onerror_listener_;
}
scoped_refptr<dom::EventListener> XMLHttpRequestEventTarget::onload() const {
  return onload_listener_;
}
scoped_refptr<dom::EventListener> XMLHttpRequestEventTarget::onloadend() const {
  return onloadend_listener_;
}
scoped_refptr<dom::EventListener> XMLHttpRequestEventTarget::onloadstart()
    const {
  return onloadstart_listener_;
}
scoped_refptr<dom::EventListener> XMLHttpRequestEventTarget::onprogress()
    const {
  return onprogress_listener_;
}
scoped_refptr<dom::EventListener> XMLHttpRequestEventTarget::ontimeout() const {
  return ontimeout_listener_;
}

void XMLHttpRequestEventTarget::set_onabort(
    const scoped_refptr<dom::EventListener>& listener) {
  onabort_listener_ = listener;
  SetAttributeEventListener(dom::EventNames::GetInstance()->abort(), listener);
}
void XMLHttpRequestEventTarget::set_onerror(
    const scoped_refptr<dom::EventListener>& listener) {
  onerror_listener_ = listener;
  SetAttributeEventListener(dom::EventNames::GetInstance()->error(), listener);
}
void XMLHttpRequestEventTarget::set_onload(
    const scoped_refptr<dom::EventListener>& listener) {
  onload_listener_ = listener;
  SetAttributeEventListener(dom::EventNames::GetInstance()->load(), listener);
}
void XMLHttpRequestEventTarget::set_onloadend(
    const scoped_refptr<dom::EventListener>& listener) {
  onloadend_listener_ = listener;
  SetAttributeEventListener(dom::EventNames::GetInstance()->loadend(),
                            listener);
}
void XMLHttpRequestEventTarget::set_onloadstart(
    const scoped_refptr<dom::EventListener>& listener) {
  onloadstart_listener_ = listener;
  SetAttributeEventListener(dom::EventNames::GetInstance()->loadstart(),
                            listener);
}
void XMLHttpRequestEventTarget::set_onprogress(
    const scoped_refptr<dom::EventListener>& listener) {
  onprogress_listener_ = listener;
  SetAttributeEventListener(dom::EventNames::GetInstance()->progress(),
                            listener);
}
void XMLHttpRequestEventTarget::set_ontimeout(
    const scoped_refptr<dom::EventListener>& listener) {
  ontimeout_listener_ = listener;
  SetAttributeEventListener(dom::EventNames::GetInstance()->timeout(),
                            listener);
}
}  // namespace xhr
}  // namespace cobalt
