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

#ifndef XHR_XML_HTTP_REQUEST_EVENT_TARGET_H_
#define XHR_XML_HTTP_REQUEST_EVENT_TARGET_H_

#include "cobalt/dom/event_target.h"

namespace cobalt {
namespace xhr {

class XMLHttpRequestEventTarget : public dom::EventTarget {
 public:
  XMLHttpRequestEventTarget();

  scoped_refptr<dom::EventListener> onabort() const;
  scoped_refptr<dom::EventListener> onerror() const;
  scoped_refptr<dom::EventListener> onload() const;
  scoped_refptr<dom::EventListener> onloadend() const;
  scoped_refptr<dom::EventListener> onloadstart() const;
  scoped_refptr<dom::EventListener> onprogress() const;
  scoped_refptr<dom::EventListener> ontimeout() const;

  void set_onabort(const scoped_refptr<dom::EventListener>& listener);
  void set_onerror(const scoped_refptr<dom::EventListener>& listener);
  void set_onload(const scoped_refptr<dom::EventListener>& listener);
  void set_onloadend(const scoped_refptr<dom::EventListener>& listener);
  void set_onloadstart(const scoped_refptr<dom::EventListener>& listener);
  void set_onprogress(const scoped_refptr<dom::EventListener>& listener);
  void set_ontimeout(const scoped_refptr<dom::EventListener>& listener);

  DEFINE_WRAPPABLE_TYPE(XMLHttpRequestEventTarget);

 protected:
  ~XMLHttpRequestEventTarget() OVERRIDE;

  scoped_refptr<dom::EventListener> onabort_listener_;
  scoped_refptr<dom::EventListener> onerror_listener_;
  scoped_refptr<dom::EventListener> onload_listener_;
  scoped_refptr<dom::EventListener> onloadend_listener_;
  scoped_refptr<dom::EventListener> onloadstart_listener_;
  scoped_refptr<dom::EventListener> onprogress_listener_;
  scoped_refptr<dom::EventListener> ontimeout_listener_;

 private:
  DISALLOW_COPY_AND_ASSIGN(XMLHttpRequestEventTarget);
};

}  // namespace xhr
}  // namespace cobalt

#endif  // XHR_XML_HTTP_REQUEST_EVENT_TARGET_H_
