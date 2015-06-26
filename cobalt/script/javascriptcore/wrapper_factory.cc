/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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
#include "cobalt/script/javascriptcore/wrapper_factory.h"

#include "cobalt/script/javascriptcore/jsc_object_handle.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

void WrapperFactory::RegisterWrappableType(
    base::TypeId wrappable_type, const JSC::ClassInfo* class_info,
    const CreateWrapperFunction& create_function) {
  std::pair<WrappableTypeInfoMap::iterator, bool> pib =
      wrappable_type_infos_.insert(std::make_pair(
          wrappable_type, WrappableTypeInfo(class_info, create_function)));
  DCHECK(pib.second)
      << "RegisterWrappableType registered for type more than once.";
}


JSC::JSObject* WrapperFactory::GetWrapper(
    JSCGlobalObject* global_object,
    const scoped_refptr<Wrappable>& wrappable) const {
  if (!wrappable) {
    return NULL;
  }

  JSC::JSObject* wrapper =
      JSCObjectHandle::GetJSObject(GetCachedWrapper(wrappable.get()));
  if (!wrapper) {
    scoped_ptr<ScriptObjectHandle> object_handle =
        WrapperFactory::CreateWrapper(global_object, wrappable);
    SetCachedWrapper(wrappable.get(), object_handle.Pass());
    wrapper = JSCObjectHandle::GetJSObject(GetCachedWrapper(wrappable.get()));
  }
  DCHECK(wrapper);
  return wrapper;
}

const JSC::ClassInfo* WrapperFactory::GetClassInfo(
    base::TypeId wrappable_type) const {
  WrappableTypeInfoMap::const_iterator it =
      wrappable_type_infos_.find(wrappable_type);
  if (it == wrappable_type_infos_.end()) {
    NOTREACHED();
    return NULL;
  }
  return it->second.class_info;
}


scoped_ptr<ScriptObjectHandle> WrapperFactory::CreateWrapper(
    JSCGlobalObject* global_object,
    const scoped_refptr<Wrappable>& wrappable) const {
  WrappableTypeInfoMap::const_iterator it =
      wrappable_type_infos_.find(wrappable->GetWrappableType());
  if (it == wrappable_type_infos_.end()) {
    NOTREACHED();
    return scoped_ptr<ScriptObjectHandle>();
  }
  return make_scoped_ptr<ScriptObjectHandle>(
      new JSCObjectHandle(JSC::PassWeak<JSC::JSObject>(
          it->second.create_function.Run(global_object, wrappable))));
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
