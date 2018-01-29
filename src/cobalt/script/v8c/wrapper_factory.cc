// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/script/v8c/wrapper_factory.h"

#include <utility>

#include "base/lazy_instance.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/v8c/v8c_global_environment.h"
#include "cobalt/script/v8c/v8c_wrapper_handle.h"
#include "cobalt/script/v8c/wrapper_private.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

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

v8::Local<v8::Object> WrapperFactory::GetWrapper(
    const scoped_refptr<Wrappable>& wrappable) {
  v8::Local<v8::Object> wrapper;
  v8::MaybeLocal<v8::Object> maybe_wrapper = V8cWrapperHandle::MaybeGetObject(
      isolate_, GetCachedWrapper(wrappable.get()));
  if (!maybe_wrapper.ToLocal(&wrapper)) {
    scoped_ptr<Wrappable::WeakWrapperHandle> object_handle =
        CreateWrapper(wrappable);
    SetCachedWrapper(wrappable.get(), object_handle.Pass());
    wrapper = V8cWrapperHandle::MaybeGetObject(
                  isolate_, GetCachedWrapper(wrappable.get()))
                  .ToLocalChecked();
  }
  return wrapper;
}

WrapperPrivate* WrapperFactory::MaybeGetWrapperPrivate(Wrappable* wrappable) {
  return V8cWrapperHandle::MaybeGetWrapperPrivate(isolate_,
                                                  GetCachedWrapper(wrappable));
}

scoped_ptr<Wrappable::WeakWrapperHandle> WrapperFactory::CreateWrapper(
    const scoped_refptr<Wrappable>& wrappable) const {
  WrappableTypeFunctionsHashMap::const_iterator it =
      wrappable_type_functions_.find(wrappable->GetWrappableType());
  if (it == wrappable_type_functions_.end()) {
    NOTREACHED();
    return scoped_ptr<Wrappable::WeakWrapperHandle>();
  }
  v8::Local<v8::Object> new_object =
      it->second.create_wrapper.Run(isolate_, wrappable);
  WrapperPrivate* wrapper_private =
      WrapperPrivate::GetFromWrapperObject(new_object);
  DCHECK(wrapper_private);
  return make_scoped_ptr<Wrappable::WeakWrapperHandle>(
      new V8cWrapperHandle(wrapper_private));
}

bool WrapperFactory::DoesObjectImplementInterface(v8::Local<v8::Object> object,
                                                  base::TypeId type_id) const {
  // If the object doesn't have a wrapper private which means it is not a
  // platform object, so the object doesn't implement the interface.
  if (!WrapperPrivate::HasWrapperPrivate(object)) {
    return false;
  }

  auto it = wrappable_type_functions_.find(type_id);
  DCHECK(it != wrappable_type_functions_.end());

  v8::Local<v8::FunctionTemplate> function_template =
      it->second.prototype_class.Run(isolate_);
  return function_template->HasInstance(object);
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
