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

#include "cobalt/script/mozjs-45/wrapper_factory.h"

#include <utility>

#include "base/lazy_instance.h"
#include "cobalt/script/mozjs-45/mozjs_wrapper_handle.h"
#include "cobalt/script/mozjs-45/wrapper_private.h"
#include "third_party/mozjs-45/js/public/Proxy.h"

namespace cobalt {
namespace script {
namespace mozjs {

void WrapperFactory::RegisterWrappableType(
    base::TypeId wrappable_type, const CreateWrapperFunction& create_function,
    const PrototypeClassFunction& class_function) {
  std::pair<WrappableTypeFunctionsHashMap::iterator, bool> pib =
      wrappable_type_functions_.insert(std::make_pair(
          wrappable_type,
          WrappableTypeFunctions(create_function, class_function)));
  DCHECK(pib.second)
      << "RegisterWrappableType registered for type more than once.";
}

JSObject* WrapperFactory::GetWrapperProxy(
    const scoped_refptr<Wrappable>& wrappable) const {
  if (!wrappable) {
    return NULL;
  }

  JS::RootedObject wrapper_proxy(
      context_,
      MozjsWrapperHandle::GetObjectProxy(GetCachedWrapper(wrappable.get())));
  if (!wrapper_proxy) {
    scoped_ptr<Wrappable::WeakWrapperHandle> object_handle =
        CreateWrapper(wrappable);
    SetCachedWrapper(wrappable.get(), object_handle.Pass());
    wrapper_proxy =
        MozjsWrapperHandle::GetObjectProxy(GetCachedWrapper(wrappable.get()));
  }
  DCHECK(wrapper_proxy);
  DCHECK(js::IsProxy(wrapper_proxy));
  return wrapper_proxy;
}

bool WrapperFactory::HasWrapperProxy(
    const scoped_refptr<Wrappable>& wrappable) const {
  return wrappable &&
         !!MozjsWrapperHandle::GetObjectProxy(
             GetCachedWrapper(wrappable.get()));
}

bool WrapperFactory::IsWrapper(JS::HandleObject wrapper) const {
  // If the object doesn't have a wrapper private, it means that it is not a
  // platform object.
  return (JS_GetClass(wrapper)->flags & JSCLASS_HAS_PRIVATE) &&
         JS_GetPrivate(wrapper) != NULL;
}

scoped_ptr<Wrappable::WeakWrapperHandle> WrapperFactory::CreateWrapper(
    const scoped_refptr<Wrappable>& wrappable) const {
  WrappableTypeFunctionsHashMap::const_iterator it =
      wrappable_type_functions_.find(wrappable->GetWrappableType());
  if (it == wrappable_type_functions_.end()) {
    NOTREACHED();
    return scoped_ptr<Wrappable::WeakWrapperHandle>();
  }
  JS::RootedObject new_proxy(
      context_, it->second.create_wrapper.Run(context_, wrappable));
  WrapperPrivate* wrapper_private =
      WrapperPrivate::GetFromProxyObject(context_, new_proxy);
  DCHECK(wrapper_private);
  return make_scoped_ptr<Wrappable::WeakWrapperHandle>(
      new MozjsWrapperHandle(wrapper_private));
}

bool WrapperFactory::DoesObjectImplementInterface(JS::HandleObject object,
                                                  base::TypeId type_id) const {
  // If the object doesn't have a wrapper private which means it is not a
  // platform object, so the object doesn't implement the interface.
  if (!WrapperPrivate::HasWrapperPrivate(context_, object)) {
    return false;
  }

  WrappableTypeFunctionsHashMap::const_iterator it =
      wrappable_type_functions_.find(type_id);
  if (it == wrappable_type_functions_.end()) {
    NOTREACHED();
    return false;
  }
  const JSClass* proto_class = it->second.prototype_class.Run(context_);
  JS::RootedObject object_proto_object(context_);
  bool success = JS_GetPrototype(context_, object, &object_proto_object);
  bool equality = false;
  while (!equality && success && object_proto_object) {
    // Get the class of the prototype.
    const JSClass* object_proto_class = JS_GetClass(object_proto_object);
    equality = (object_proto_class == proto_class);
    // Get the prototype of the previous prototype.
    success =
        JS_GetPrototype(context_, object_proto_object, &object_proto_object);
  }
  return equality;
}
}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
