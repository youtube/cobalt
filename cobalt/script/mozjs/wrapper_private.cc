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

#include "cobalt/script/mozjs/wrapper_private.h"

#include "third_party/mozjs/js/src/jsapi.h"
#include "third_party/mozjs/js/src/jsproxy.h"

namespace cobalt {
namespace script {
namespace mozjs {

void WrapperPrivate::AddReferencedObject(JS::HandleObject referee) {
  referenced_objects_.push_back(new JS::Heap<JSObject*>(referee));
}

void WrapperPrivate::RemoveReferencedObject(JS::HandleObject referee) {
  for (ReferencedObjectVector::iterator it = referenced_objects_.begin();
       it != referenced_objects_.end(); ++it) {
    if ((**it) == referee) {
      referenced_objects_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

// static
void WrapperPrivate::AddPrivateData(JS::HandleObject wrapper_proxy,
                                    const scoped_refptr<Wrappable>& wrappable) {
  DCHECK(js::IsProxy(wrapper_proxy));
  WrapperPrivate* private_data = new WrapperPrivate(wrappable, wrapper_proxy);
  JSObject* target_object = js::GetProxyTargetObject(wrapper_proxy);
  JS_SetPrivate(target_object, private_data);
  DCHECK_EQ(JS_GetPrivate(target_object), private_data);
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
  DCHECK(wrapper_private);
  for (ReferencedObjectVector::iterator it =
           wrapper_private->referenced_objects_.begin();
       it != wrapper_private->referenced_objects_.end(); ++it) {
    JS::Heap<JSObject*>* referenced_object = *it;
    JS_CallHeapObjectTracer(trace, referenced_object, "WrapperPrivate::Trace");
  }
}

WrapperPrivate::WrapperPrivate(const scoped_refptr<Wrappable>& wrappable,
                               JS::HandleObject wrapper_proxy)
    : wrappable_(wrappable), wrapper_proxy_(wrapper_proxy) {
  DCHECK(js::IsProxy(wrapper_proxy));
}

WrapperPrivate::~WrapperPrivate() { wrapper_proxy_ = NULL; }

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
