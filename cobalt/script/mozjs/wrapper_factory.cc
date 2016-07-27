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
#include "cobalt/script/mozjs/wrapper_factory.h"

#include <utility>

#include "base/lazy_instance.h"
#include "cobalt/script/mozjs/mozjs_wrapper_handle.h"
#include "cobalt/script/mozjs/wrapper_private.h"

namespace cobalt {
namespace script {
namespace mozjs {

void WrapperFactory::RegisterWrappableType(
    base::TypeId wrappable_type, const CreateWrapperFunction& create_function) {
  std::pair<CreateWrapperHashMap::iterator, bool> pib =
      create_wrapper_functions_.insert(
          std::make_pair(wrappable_type, create_function));
  DCHECK(pib.second)
      << "RegisterWrappableType registered for type more than once.";
}

JSObject* WrapperFactory::GetWrapper(
    const scoped_refptr<Wrappable>& wrappable) const {
  if (!wrappable) {
    return NULL;
  }

  JS::RootedObject wrapper(context_, MozjsWrapperHandle::GetJSObject(
                                         GetCachedWrapper(wrappable.get())));
  if (!wrapper) {
    scoped_ptr<Wrappable::WeakWrapperHandle> object_handle =
        CreateWrapper(wrappable);
    SetCachedWrapper(wrappable.get(), object_handle.Pass());
    wrapper =
        MozjsWrapperHandle::GetJSObject(GetCachedWrapper(wrappable.get()));
  }
  DCHECK(wrapper);
  return wrapper;
}

bool WrapperFactory::IsWrapper(JS::HandleObject wrapper) const {
  return JS_GetPrivate(wrapper) != NULL;
}

scoped_ptr<Wrappable::WeakWrapperHandle> WrapperFactory::CreateWrapper(
    const scoped_refptr<Wrappable>& wrappable) const {
  CreateWrapperHashMap::const_iterator it =
      create_wrapper_functions_.find(wrappable->GetWrappableType());
  if (it == create_wrapper_functions_.end()) {
    NOTREACHED();
    return scoped_ptr<Wrappable::WeakWrapperHandle>();
  }
  JS::RootedObject new_object(context_, it->second.Run(context_, wrappable));
  WrapperPrivate* wrapper_private =
      reinterpret_cast<WrapperPrivate*>(JS_GetPrivate(new_object));
  DCHECK(wrapper_private);
  return make_scoped_ptr<Wrappable::WeakWrapperHandle>(
      new MozjsWrapperHandle(wrapper_private));
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
