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
#ifndef COBALT_SCRIPT_MOZJS_PROXY_HANDLER_H_
#define COBALT_SCRIPT_MOZJS_PROXY_HANDLER_H_

#include "third_party/mozjs/js/src/jsapi.h"
#include "third_party/mozjs/js/src/jsproxy.h"

namespace cobalt {
namespace script {
namespace mozjs {

// SpiderMonkey has a concept of a Proxy object which is associated with another
// arbitrary object and a js::BaseProxyHandler interface. The handler interface
// provides a number of traps for providing custom implementations of
// fundamental ECMAScript operations such as getPropertyDescriptor.
//
// The implementation of each trap in the js::DirectProxyHandler class simply
// forwards each trap to the target object.
//
// In defining a JSClass a number of function pointers can be set that will
// be called when getting, setting, deleting, etc. a property, but these do not
// map well onto the Web IDL spec for implementing interfaces that support named
// and indexed properties.
// The traps on the js::BaseProxyHandler interface are at a much lower level and
// allow for a cleaner and more conformant implementation.
//
// See third_party/mozjs/js/src/jsproxy.h for more details.
class ProxyHandler : public js::DirectProxyHandler {
 public:
  ProxyHandler() : js::DirectProxyHandler(NULL) {}

  static JSObject* NewProxy(JSContext* context, JSObject* object,
                            JSObject* prototype, JSObject* parent,
                            ProxyHandler* handler) {
    JS::RootedValue as_value(context, OBJECT_TO_JSVAL(object));
    return js::NewProxyObject(context, handler, as_value, prototype, parent);
  }

  // The derived traps in js::DirectProxyHandler are not implemented in terms of
  // the fundamental traps, where the traps in js::BaseProxyHandler are.
  // Redefining the derived traps to be in terms of the fundamental traps means
  // that we only need to override the fundamental traps when implementing
  // custom behavior for i.e. interfaces that support named properties.
  bool has(JSContext* context, JS::HandleObject proxy, JS::HandleId id,
           bool* bp) OVERRIDE {
    return js::BaseProxyHandler::has(context, proxy, id, bp);
  }

  bool hasOwn(JSContext* context, JS::HandleObject proxy, JS::HandleId id,
              bool* bp) OVERRIDE {
    return js::BaseProxyHandler::hasOwn(context, proxy, id, bp);
  }

  bool get(JSContext* context, JS::HandleObject proxy,
           JS::HandleObject receiver, JS::HandleId id,
           JS::MutableHandleValue vp) OVERRIDE {
    return js::BaseProxyHandler::get(context, proxy, receiver, id, vp);
  }

  bool set(JSContext* context, JS::HandleObject proxy,
           JS::HandleObject receiver, JS::HandleId id, bool strict,
           JS::MutableHandleValue vp) OVERRIDE {
    return js::BaseProxyHandler::set(context, proxy, receiver, id, strict, vp);
  }

  bool keys(JSContext* context, JS::HandleObject proxy,
            JS::AutoIdVector& props) OVERRIDE {  // NOLINT[runtime/references]
    return js::BaseProxyHandler::keys(context, proxy, props);
  }

  bool iterate(JSContext* context, JS::HandleObject proxy, unsigned flags,
               JS::MutableHandleValue vp) OVERRIDE {
    return js::BaseProxyHandler::iterate(context, proxy, flags, vp);
  }
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_PROXY_HANDLER_H_
