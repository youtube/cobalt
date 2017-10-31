// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_MOZJS_45_PROXY_HANDLER_H_
#define COBALT_SCRIPT_MOZJS_45_PROXY_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "third_party/mozjs-45/js/public/Proxy.h"
#include "third_party/mozjs-45/js/src/jsapi.h"
#include "third_party/mozjs-45/js/src/proxy/Proxy.h"

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
// See third_party/mozjs-45/js/public/Proxy.h for more details.
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
    JSGetterOp getter;
    JSSetterOp setter;
    IndexedDeleteFunction deleter;
  };

  // Hooks for interfaces that support named properties.
  struct NamedPropertyHooks {
    IsSupportedNameFunction is_supported;
    EnumerateSupportedNamesFunction enumerate_supported;
    JSGetterOp getter;
    JSSetterOp setter;
    NamedDeleteFunction deleter;
  };

  static JSObject* NewProxy(JSContext* context, ProxyHandler* handler,
                            JSObject* object, JSObject* prototype);

  // Construct a new ProxyHandler with the provided hooks.
  ProxyHandler(const IndexedPropertyHooks& indexed_hooks,
               const NamedPropertyHooks& named_hooks);

  // Overridden standard internal methods.
  bool getOwnPropertyDescriptor(
      JSContext* context, JS::HandleObject proxy, JS::HandleId id,
      JS::MutableHandle<JSPropertyDescriptor> descriptor) const OVERRIDE;

  bool defineProperty(JSContext* context, JS::HandleObject proxy,
                      JS::HandleId id,
                      js::Handle<JSPropertyDescriptor> descriptor,
                      JS::ObjectOpResult& result  // NOLINT[runtime/references]
                      ) const OVERRIDE;           // NOLINT[whitespace/parens]
  bool ownPropertyKeys(JSContext* context, JS::HandleObject proxy,
                       JS::AutoIdVector& properties) const OVERRIDE; // NOLINT[runtime/references]
  bool delete_(JSContext* context, JS::HandleObject proxy, JS::HandleId id,
               JS::ObjectOpResult& result)  // NOLINT(runtime/references)
      const OVERRIDE;

  // Standard methods that were overridden in DirectProxyHandler. These are
  // implemented in terms of the standard internal methods in BaseProxyHandler
  // so ensure that we use those, so we don't have to override all of these.
  // TODO: Consider overriding some of these as performance optimizations if
  // necessary.
  bool enumerate(JSContext* context, JS::HandleObject proxy,
                 JS::MutableHandleObject objp) const OVERRIDE {
    return js::BaseProxyHandler::enumerate(context, proxy, objp);
  }

  bool has(JSContext* context, JS::HandleObject proxy, JS::HandleId id,
           bool* bp) const OVERRIDE {
    return js::BaseProxyHandler::has(context, proxy, id, bp);
  }

  bool get(JSContext* context, JS::HandleObject proxy, JS::HandleValue receiver,
           JS::HandleId id, JS::MutableHandleValue vp) const OVERRIDE {
    return js::BaseProxyHandler::get(context, proxy, receiver, id, vp);
  }

  bool set(JSContext* context, JS::HandleObject proxy, JS::HandleId id,
           JS::HandleValue v, JS::HandleValue receiver,
           JS::ObjectOpResult& result)  // NOLINT(runtime/references)
      const OVERRIDE {
    return js::BaseProxyHandler::set(context, proxy, id, v, receiver, result);
  }

  // SpiderMonkey extensions that should also be implemented in terms of the
  // standard methods, as described above.
  bool getPropertyDescriptor(
      JSContext* context, JS::HandleObject proxy, JS::HandleId id,
      JS::MutableHandle<JSPropertyDescriptor> descriptor) const OVERRIDE {
    return js::BaseProxyHandler::getPropertyDescriptor(context, proxy, id,
                                                       descriptor);
  }
  bool hasOwn(JSContext* context, JS::HandleObject proxy, JS::HandleId id,
              bool* bp) const OVERRIDE {
    return js::BaseProxyHandler::hasOwn(context, proxy, id, bp);
  }
  bool getOwnEnumerablePropertyKeys(JSContext* context, JS::HandleObject proxy,
                                    JS::AutoIdVector& props) const OVERRIDE {  // NOLINT[runtime/references]
    return js::BaseProxyHandler::getOwnEnumerablePropertyKeys(context, proxy,
                                                              props);
  }

  bool has_custom_property() const { return has_custom_property_; }

 private:
  // https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
  // This is used to support named and indexed properties.
  // Returns false on failure.
  bool LegacyPlatformObjectGetOwnPropertyDescriptor(
      JSContext* context, JS::HandleObject proxy, JS::HandleId id,
      JS::MutableHandle<JSPropertyDescriptor> descriptor) const;

  bool supports_named_properties() const {
    return named_property_hooks_.getter != NULL;
  }

  bool supports_indexed_properties() const {
    return indexed_property_hooks_.getter != NULL;
  }

  bool IsSupportedIndex(JSContext* context, JS::HandleObject object,
                        uint32_t index) const {
    DCHECK(indexed_property_hooks_.is_supported);
    return indexed_property_hooks_.is_supported(context, object, index);
  }

  bool IsSupportedName(JSContext* context, JS::HandleObject object,
                       const std::string& name) const {
    DCHECK(named_property_hooks_.is_supported);
    return named_property_hooks_.is_supported(context, object, name);
  }

  bool IsArrayIndexPropertyName(JSContext* context,
                                JS::HandleValue property_value,
                                uint32_t* out_index) const;

  bool IsNamedPropertyVisible(JSContext* context, JS::HandleObject object,
                              const std::string& property_name) const;

  IndexedPropertyHooks indexed_property_hooks_;
  NamedPropertyHooks named_property_hooks_;
  // Set to true if this object may have a custom property set on it.
  bool has_custom_property_;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_PROXY_HANDLER_H_
