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

#include "cobalt/script/mozjs-45/wrapper_private.h"

#include "base/hash_tables.h"
#include "cobalt/script/mozjs-45/mozjs_global_environment.h"
#include "cobalt/script/mozjs-45/proxy_handler.h"
#include "cobalt/script/mozjs-45/referenced_object_map.h"
#include "third_party/mozjs-45/js/src/jsapi.h"
#include "third_party/mozjs-45/js/src/jsfun.h"
#include "third_party/mozjs-45/js/src/proxy/Proxy.h"
#include "third_party/mozjs-45/js/src/vm/NativeObject.h"

namespace cobalt {
namespace script {
namespace mozjs {

// static
void WrapperPrivate::AddPrivateData(JSContext* context,
                                    JS::HandleObject wrapper_proxy,
                                    const scoped_refptr<Wrappable>& wrappable) {
  DCHECK(js::IsProxy(wrapper_proxy));
  WrapperPrivate* private_data =
      new WrapperPrivate(context, wrappable, wrapper_proxy);
  JS::RootedObject target_object(context,
                                 js::GetProxyTargetObject(wrapper_proxy));
  JS_SetPrivate(target_object, private_data);
  DCHECK_EQ(JS_GetPrivate(target_object), private_data);
}

// static
bool WrapperPrivate::HasWrapperPrivate(JSContext* context,
                                       JS::HandleObject object) {
  if (js::IsProxy(object)) {
    JS::RootedObject target_object(context, js::GetProxyTargetObject(object));
    return WrapperPrivate::HasWrapperPrivate(context, target_object);
  }

  DCHECK(object->isNative());
  return object->as<js::NativeObject>().hasPrivate();
}

// static
WrapperPrivate* WrapperPrivate::GetFromWrappable(
    const scoped_refptr<Wrappable>& wrappable, JSContext* context,
    WrapperFactory* wrapper_factory) {
  JS::RootedObject wrapper_proxy(context,
                                 wrapper_factory->GetWrapperProxy(wrappable));
  WrapperPrivate* private_data = GetFromProxyObject(context, wrapper_proxy);
  DCHECK(private_data);
  DCHECK_EQ(private_data->wrappable_, wrappable);
  return private_data;
}

// static
WrapperPrivate* WrapperPrivate::GetFromWrapperObject(JS::HandleObject wrapper) {
  DCHECK(!js::IsProxy(wrapper));
  if (!(JS_GetClass(wrapper)->flags & JSCLASS_HAS_PRIVATE)) {
    return NULL;
  }
  WrapperPrivate* private_data =
      static_cast<WrapperPrivate*>(JS_GetPrivate(wrapper));
  DCHECK(private_data);
  return private_data;
}

// static
WrapperPrivate* WrapperPrivate::GetFromProxyObject(
    JSContext* context, JS::HandleObject proxy_object) {
  DCHECK(js::IsProxy(proxy_object));
  JS::RootedObject target(context, js::GetProxyTargetObject(proxy_object));
  return GetFromWrapperObject(target);
}

// static
WrapperPrivate* WrapperPrivate::GetFromObject(JSContext* context,
                                              JS::HandleObject object) {
  if (js::IsProxy(object)) {
    return GetFromProxyObject(context, object);
  } else {
    return GetFromWrapperObject(object);
  }
}

// static
void WrapperPrivate::Finalizer(JSFreeOp* /* free_op */, JSObject* object) {
  WrapperPrivate* wrapper_private =
      reinterpret_cast<WrapperPrivate*>(JS_GetPrivate(object));
  DCHECK(wrapper_private);
  delete wrapper_private;
}

// static
void WrapperPrivate::Trace(JSTracer* trace, JSObject* object) {
  WrapperPrivate* wrapper_private =
      reinterpret_cast<WrapperPrivate*>(JS_GetPrivate(object));
  // Verify that this trace function is called for the object (rather than the
  // proxy object).
  DCHECK(!js::IsProxy(object));

  // The GC could run on this object before we've had a chance to set its
  // private data, so we must handle the case where JS_GetPrivate returns NULL.
  if (wrapper_private) {
    // Verify that WrapperPrivate::wrapper_proxy_'s target object is this
    // object.
    DCHECK_EQ(object,
              js::GetProxyTargetObject(wrapper_private->wrapper_proxy_));

    // The wrapper's proxy object will keep the wrapper object alive, but the
    // reverse is not true, so we must trace it explicitly.
    JS_CallObjectTracer(trace, &wrapper_private->wrapper_proxy_,
                        "WrapperPrivate::Trace");

    MozjsGlobalEnvironment* global_environment =
        MozjsGlobalEnvironment::GetFromContext(wrapper_private->context_);
    global_environment->referenced_objects()->TraceReferencedObjects(
        trace, wrapper_private->wrappable_);
    MozjsTracer mozjs_tracer(trace);
    mozjs_tracer.TraceFrom(wrapper_private->wrappable_);
  }
}

WrapperPrivate::WrapperPrivate(JSContext* context,
                               const scoped_refptr<Wrappable>& wrappable,
                               JS::HandleObject wrapper_proxy)
    : context_(context), wrappable_(wrappable), wrapper_proxy_(wrapper_proxy) {
  DCHECK(js::IsProxy(wrapper_proxy));
}

WrapperPrivate::~WrapperPrivate() {
  wrapper_proxy_ = NULL;
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
