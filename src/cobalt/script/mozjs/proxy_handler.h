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

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
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
//
// See third_party/mozjs/js/src/jsproxy.h for more details.
//
// ProxyHandler provides custom traps for getPropertyDescriptor, delete_, and
// enumerate to implement interfaces that support named and indexed properties.
class ProxyHandler : public js::DirectProxyHandler {
 public:
  typedef bool (*IsSupportedIndexFunction)(JSContext*, JS::HandleObject,
                                           uint32_t);
  typedef bool (*IsSupportedNameFunction)(JSContext*, JS::HandleObject,
                                          const std::string&);
  typedef void (*EnumerateSupportedIndexesFunction)(JSContext*,
                                                    JS::HandleObject,
                                                    JS::AutoIdVector*);
  typedef void (*EnumerateSupportedNamesFunction)(JSContext*, JS::HandleObject,
                                                  JS::AutoIdVector*);
  typedef bool (*IndexedDeleteFunction)(JSContext*, JS::HandleObject, uint32_t);
  typedef bool (*NamedDeleteFunction)(JSContext*, JS::HandleObject,
                                      const std::string&);

  // Hooks for interfaces that support indexed properties.
  struct IndexedPropertyHooks {
    IsSupportedIndexFunction is_supported;
    EnumerateSupportedIndexesFunction enumerate_supported;
    JSPropertyOp getter;
    JSStrictPropertyOp setter;
    IndexedDeleteFunction deleter;
  };

  // Hooks for interfaces that support named properties.
  struct NamedPropertyHooks {
    IsSupportedNameFunction is_supported;
    EnumerateSupportedNamesFunction enumerate_supported;
    JSPropertyOp getter;
    JSStrictPropertyOp setter;
    NamedDeleteFunction deleter;
  };

  static JSObject* NewProxy(JSContext* context, JSObject* object,
                            JSObject* prototype, JSObject* parent,
                            ProxyHandler* handler);

  // Construct a new ProxyHandler with the provided hooks.
  ProxyHandler(const IndexedPropertyHooks& indexed_hooks,
               const NamedPropertyHooks& named_hooks);

  // Overridden fundamental traps.
  bool getPropertyDescriptor(JSContext* context, JS::HandleObject proxy,
                             JS::HandleId id, JSPropertyDescriptor* descriptor,
                             unsigned flags) OVERRIDE;
  bool delete_(JSContext* context, JS::HandleObject proxy, JS::HandleId id,
               bool* succeeded) OVERRIDE;
  bool enumerate(JSContext* context, JS::HandleObject proxy,
                 JS::AutoIdVector& properties) OVERRIDE;  // NOLINT[runtime/references]

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

 private:
  bool supports_named_properties() {
    return named_property_hooks_.getter != NULL;
  }

  bool supports_indexed_properties() {
    return indexed_property_hooks_.getter != NULL;
  }

  bool IsSupportedIndex(JSContext* context, JS::HandleObject object,
                        uint32_t index);

  bool IsSupportedName(JSContext* context, JS::HandleObject object,
                       const std::string& name);

  bool IsArrayIndexPropertyName(JSContext* context,
                                JS::HandleValue property_value,
                                uint32_t* out_index);

  bool IsNamedPropertyVisible(JSContext* context, JS::HandleObject object,
                              const std::string& property_name);

  IndexedPropertyHooks indexed_property_hooks_;
  NamedPropertyHooks named_property_hooks_;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_PROXY_HANDLER_H_
