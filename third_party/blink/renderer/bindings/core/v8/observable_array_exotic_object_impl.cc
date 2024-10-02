// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/observable_array.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-proxy.h"

namespace blink {

namespace bindings {

namespace {

const V8PrivateProperty::SymbolKey kV8ProxyTargetToV8WrapperKey;

const WrapperTypeInfo kWrapperTypeInfoBody{
    gin::kEmbedderBlink,
    /*install_interface_template_func=*/nullptr,
    /*install_context_dependent_props_func=*/nullptr,
    "ObservableArrayExoticObject",
    /*parent_class=*/nullptr,
    WrapperTypeInfo::kWrapperTypeNoPrototype,
    // v8::Proxy (without an internal field) is used as a (pseudo) wrapper.
    WrapperTypeInfo::kNoInternalFieldClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
    WrapperTypeInfo::kIdlObservableArray,
};

}  // namespace

// static
const WrapperTypeInfo& ObservableArrayExoticObjectImpl::wrapper_type_info_ =
    kWrapperTypeInfoBody;

// static
bindings::ObservableArrayBase*
ObservableArrayExoticObjectImpl::ProxyTargetToObservableArrayBaseOrDie(
    v8::Isolate* isolate,
    v8::Local<v8::Array> v8_proxy_target) {
  // See the implementation comment in ObservableArrayExoticObjectImpl::Wrap.
  auto private_property =
      V8PrivateProperty::GetSymbol(isolate, kV8ProxyTargetToV8WrapperKey);
  v8::Local<v8::Value> backing_list_wrapper =
      private_property.GetOrUndefined(v8_proxy_target).ToLocalChecked();
  // Crash when author script managed to pass something else other than the
  // right proxy target object.
  CHECK(backing_list_wrapper->IsObject());
  return ToScriptWrappable(backing_list_wrapper.As<v8::Object>())
      ->ToImpl<bindings::ObservableArrayBase>();
}

ObservableArrayExoticObjectImpl::ObservableArrayExoticObjectImpl(
    bindings::ObservableArrayBase* observable_array_backing_list_object)
    : ObservableArrayExoticObject(observable_array_backing_list_object) {}

v8::MaybeLocal<v8::Value> ObservableArrayExoticObjectImpl::Wrap(
    ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  DCHECK(!DOMDataStore::ContainsWrapper(this, isolate));

  // The proxy target object must be a JS Array (v8::Array) by definition.
  // Especially it's important that IsArray(proxy) evaluates to true.
  // https://tc39.es/ecma262/#sec-isarray
  //
  // Thus, we create the following structure of objects:
  //   exotic_object = new Proxy(target_object, handler_object);
  // where
  //   target_object = new Array();
  //   target_object--(private property)-->v8_wrapper_of_backing_list
  //   v8_wrapper_of_backing_list--(internal field)-->blink_backing_list
  //   blink_backing_list = instance of V8ObservableArrayXxxx
  //
  // The back reference from blink_backing_list to the JS Array object is not
  // supported just because there is no use case so far.
  v8::Local<v8::Value> backing_list_wrapper;
  if (!ToV8Traits<bindings::ObservableArrayBase>::ToV8(script_state,
                                                       GetBackingListObject())
           .ToLocal(&backing_list_wrapper)) {
    return {};
  }
  CHECK(backing_list_wrapper->IsObject());
  v8::Local<v8::Array> target = v8::Array::New(isolate);
  auto private_property =
      V8PrivateProperty::GetSymbol(isolate, kV8ProxyTargetToV8WrapperKey);
  private_property.Set(target, backing_list_wrapper);

  v8::Local<v8::Object> handler;
  if (!GetBackingListObject()
           ->GetProxyHandlerObject(script_state)
           .ToLocal(&handler)) {
    return {};
  }
  v8::Local<v8::Proxy> proxy;
  if (!v8::Proxy::New(script_state->GetContext(), target.As<v8::Object>(),
                      handler)
           .ToLocal(&proxy)) {
    return {};
  }
  v8::Local<v8::Object> wrapper = proxy.As<v8::Object>();

  // Register the proxy object as a (pseudo) wrapper object although the proxy
  // object does not have an internal field pointing to a Blink object.
  const bool is_new_entry = script_state->World().DomDataStore().Set(
      script_state->GetIsolate(), this, GetWrapperTypeInfo(), wrapper);
  CHECK(is_new_entry);

  return wrapper;
}

v8::Local<v8::Object> ObservableArrayExoticObjectImpl::AssociateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::Object> wrapper) {
  // The proxy object does not have an internal field and cannot be associated
  // with a Blink object directly.
  NOTREACHED();
  return {};
}

}  // namespace bindings

}  // namespace blink
