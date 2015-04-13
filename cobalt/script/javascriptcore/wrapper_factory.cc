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

void WrapperFactory::RegisterCreateWrapperMethod(
    base::TypeId wrappable_type,
    const CreateWrapperFunction& create_function) {
  std::pair<CreateWrapperFunctionMap::iterator, bool> pib;
  pib =
      create_functions_.insert(std::make_pair(wrappable_type, create_function));
  DCHECK(pib.second)
      << "CreateWrapperFunction registered for type more than once.";
}


JSC::JSObject* WrapperFactory::GetWrapper(
    const scoped_refptr<Wrappable>& wrappable) const {
  if (!wrappable) {
    return NULL;
  }

  return JSCObjectHandle::GetJSObject(wrappable->GetWrapperHandle(
      base::Bind(&WrapperFactory::CreateWrapper, base::Unretained(this))));
}

scoped_ptr<ScriptObjectHandle> WrapperFactory::CreateWrapper(
    const scoped_refptr<Wrappable>& wrappable) const {
  CreateWrapperFunctionMap::const_iterator it =
      create_functions_.find(wrappable->GetWrappableType());
  if (it == create_functions_.end()) {
    NOTREACHED();
    return scoped_ptr<ScriptObjectHandle>();
  }
  return make_scoped_ptr<ScriptObjectHandle>(new JSCObjectHandle(
      JSC::PassWeak<JSC::JSObject>(it->second.Run(global_object_, wrappable))));
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
