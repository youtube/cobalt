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

#include "cobalt/script/mozjs/proxy_handler.h"

#include "cobalt/script/mozjs/conversion_helpers.h"
#include "cobalt/script/mozjs/mozjs_exception_state.h"

namespace cobalt {
namespace script {
namespace mozjs {

ProxyHandler::ProxyHandler(const IndexedPropertyHooks& indexed_hooks,
                           const NamedPropertyHooks& named_hooks)
    : js::DirectProxyHandler(NULL),
      indexed_property_hooks_(indexed_hooks),
      named_property_hooks_(named_hooks) {
  // If an interface supports named/indexed properties, they must have a hook to
  // check if the name/index is supported and to enumerate the properties.
  if (supports_named_properties()) {
    DCHECK(named_property_hooks_.is_supported);
    DCHECK(named_property_hooks_.enumerate_supported);
  }
  if (supports_indexed_properties()) {
    DCHECK(indexed_property_hooks_.is_supported);
    DCHECK(indexed_property_hooks_.enumerate_supported);
  }
}

JSObject* ProxyHandler::NewProxy(JSContext* context, JSObject* object,
                                 JSObject* prototype, JSObject* parent,
                                 ProxyHandler* handler) {
  JS::RootedValue as_value(context, OBJECT_TO_JSVAL(object));
  return js::NewProxyObject(context, handler, as_value, prototype, parent);
}

bool ProxyHandler::getPropertyDescriptor(JSContext* context,
                                         JS::HandleObject proxy,
                                         JS::HandleId id,
                                         JSPropertyDescriptor* descriptor,
                                         unsigned flags) {
  // https://www.w3.org/TR/WebIDL/#getownproperty
  if (supports_named_properties() || supports_indexed_properties()) {
    // Convert the id to a JSValue, so we can easily convert it to Uint32 and
    // JSString.
    JS::RootedValue id_value(context);
    if (!JS_IdToValue(context, id, id_value.address())) {
      NOTREACHED();
      return false;
    }
    JS::RootedObject object(context, js::GetProxyTargetObject(proxy));
    if (supports_indexed_properties()) {
      // If the interface supports indexed properties and this is an array index
      // property name, and it is a supported property index.
      uint32_t index;
      if (IsArrayIndexPropertyName(context, id_value, &index) &&
          IsSupportedIndex(context, object, index)) {
        descriptor->obj = object;
        descriptor->attrs = JSPROP_SHARED | JSPROP_INDEX | JSPROP_ENUMERATE;
        descriptor->getter = indexed_property_hooks_.getter;
        if (indexed_property_hooks_.setter) {
          descriptor->setter = indexed_property_hooks_.setter;
        } else {
          descriptor->attrs |= JSPROP_READONLY;
        }
        return true;
      }
    }
    if (supports_named_properties()) {
      std::string property_name;
      MozjsExceptionState exception_state(context);
      FromJSValue(context, id_value, kNoConversionFlags, &exception_state,
                  &property_name);
      if (exception_state.is_exception_set()) {
        // The ID should be an integer or a string, so we shouldn't have any
        // exceptions converting to string.
        NOTREACHED();
        return false;
      }
      if (IsNamedPropertyVisible(context, object, property_name)) {
        descriptor->obj = object;
        descriptor->attrs = JSPROP_SHARED | JSPROP_ENUMERATE;
        descriptor->getter = named_property_hooks_.getter;
        if (named_property_hooks_.setter) {
          descriptor->setter = named_property_hooks_.setter;
        } else {
          descriptor->attrs |= JSPROP_READONLY;
        }
        return true;
      }
    }
  }
  return js::DirectProxyHandler::getPropertyDescriptor(context, proxy, id,
                                                       descriptor, flags);
}

bool ProxyHandler::delete_(JSContext* context, JS::HandleObject proxy,
                           JS::HandleId id, bool* succeeded) {
  // https://www.w3.org/TR/WebIDL/#delete
  if (supports_named_properties() || supports_indexed_properties()) {
    // Convert the id to a JSValue, so we can easily convert it to Uint32 and
    // JSString.
    JS::RootedValue id_value(context);
    if (!JS_IdToValue(context, id, id_value.address())) {
      NOTREACHED();
      return false;
    }
    DCHECK(js::IsProxy(proxy));
    JS::RootedObject object(context, js::GetProxyTargetObject(proxy));
    if (supports_indexed_properties()) {
      // If the interface supports indexed properties and this is an array index
      // property name, and it is a supported property index.
      uint32_t index;
      // 1. If O supports indexed properties and P is an array index property
      // name,
      //    then:
      if (IsArrayIndexPropertyName(context, id_value, &index)) {
        if (!IsSupportedIndex(context, object, index)) {
          // 1.2. If index is not a supported property index, then return true.
          *succeeded = true;
        } else if (!indexed_property_hooks_.deleter) {
          // 1.3. If O does not implement an interface with an indexed property
          //    deleter, then Reject.
          *succeeded = false;
        } else {
          *succeeded = indexed_property_hooks_.deleter(context, object, index);
        }
        return true;
      }
    }
    if (supports_named_properties()) {
      std::string property_name;
      MozjsExceptionState exception_state(context);
      FromJSValue(context, id_value, kNoConversionFlags, &exception_state,
                  &property_name);
      if (exception_state.is_exception_set()) {
        // The ID should be an integer or a string, so we shouldn't have any
        // exceptions converting to string.
        NOTREACHED();
        return false;
      }
      if (IsNamedPropertyVisible(context, object, property_name)) {
        if (!named_property_hooks_.deleter) {
          *succeeded = false;
        } else {
          *succeeded =
              named_property_hooks_.deleter(context, object, property_name);
        }
        return true;
      }
    }
  }
  return js::DirectProxyHandler::delete_(context, proxy, id, succeeded);
}

bool ProxyHandler::enumerate(JSContext* context, JS::HandleObject proxy,
                             JS::AutoIdVector& properties) {
  // https://www.w3.org/TR/WebIDL/#property-enumeration
  // Indexed properties go first, then named properties, then everything else.
  JS::RootedObject object(context, js::GetProxyTargetObject(proxy));
  if (supports_indexed_properties()) {
    indexed_property_hooks_.enumerate_supported(context, object, &properties);
  }
  if (supports_named_properties()) {
    named_property_hooks_.enumerate_supported(context, object, &properties);
  }
  return js::DirectProxyHandler::enumerate(context, proxy, properties);
}

bool ProxyHandler::IsSupportedIndex(JSContext* context, JS::HandleObject object,
                                    uint32_t index) {
  DCHECK(indexed_property_hooks_.is_supported);
  return indexed_property_hooks_.is_supported(context, object, index);
}

bool ProxyHandler::IsSupportedName(JSContext* context, JS::HandleObject object,
                                   const std::string& name) {
  DCHECK(named_property_hooks_.is_supported);
  return named_property_hooks_.is_supported(context, object, name);
}

bool ProxyHandler::IsArrayIndexPropertyName(JSContext* context,
                                            JS::HandleValue property_value,
                                            uint32_t* out_index) {
  // https://www.w3.org/TR/WebIDL/#dfn-array-index-property-name
  // 1. Let i be ToUint32(P).
  uint32_t index;
  if (!JS::ToUint32(context, property_value, &index)) {
    return false;
  }

  // 3. If i = 2^32 - 1, then return false.
  if (index == 0xFFFFFFFF) {
    return false;
  }

  // 2. Let s be ToString(i).
  // 3. If s != P then return false.
  JSBool are_equal;
  JS::RootedString index_as_string(
      context, JS_ValueToString(context, UINT_TO_JSVAL(index)));
  if (!JS_LooselyEqual(context, JS::StringValue(index_as_string),
                       property_value, &are_equal) ||
      !are_equal) {
    return false;
  }

  // 4. Return true.
  *out_index = index;
  return true;
}

bool ProxyHandler::IsNamedPropertyVisible(JSContext* context,
                                          JS::HandleObject object,
                                          const std::string& property_name) {
  // Named property visiblity algorithm.
  // https://www.w3.org/TR/WebIDL/#dfn-named-property-visibility

  // 1. If P is an unforgeable property name on O, then return false.
  // 2. If O implements an interface with an [Unforgeable]-annotated attribute
  //    whose identifier is P, then return false.
  // TODO: Implement Unforgeable extended attribute.

  // 3. If P is not a supported property name of O, then return false.
  if (!IsSupportedName(context, object, property_name)) {
    return false;
  }

  // 4. If O implements an interface that has the [OverrideBuiltins] extended
  // attribute, then return true.
  // TODO: Implement OverrideBuiltins extended attribute

  // 5. If O has an own property named P, then return false.
  // 6~7. ( Walk the prototype chain and if the prootype has P, return false)

  JSBool found_property;
  if (!JS_HasProperty(context, object, property_name.c_str(),
                      &found_property)) {
    // An error occurred searching for the property.
    NOTREACHED();
    return true;
  }
  return !found_property;
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
