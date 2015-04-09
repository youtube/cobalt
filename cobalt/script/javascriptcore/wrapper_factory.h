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
#ifndef SCRIPT_JAVASCRIPTCORE_WRAPPER_FACTORY_H_
#define SCRIPT_JAVASCRIPTCORE_WRAPPER_FACTORY_H_

#include "config.h"
#undef LOG  // Defined by WTF, also redefined by chromium. Unneeded by cobalt.

#include "base/bind.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "cobalt/script/script_object_handle.h"
#include "cobalt/script/wrappable.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSObject.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

class JSCGlobalObject;

class WrapperFactory {
 public:
  explicit WrapperFactory(JSCGlobalObject* global_object)
      : global_object_(global_object) {}
  typedef base::Callback<JSC::JSObject*(
      JSCGlobalObject*, const scoped_refptr<Wrappable>&)> CreateWrapperFunction;
  void RegisterCreateWrapperMethod(
      Wrappable::Type wrappable_type,
      const CreateWrapperFunction& create_function);
  JSC::JSObject* GetWrapper(const scoped_refptr<Wrappable>& wrappable) const;

 private:
  scoped_ptr<ScriptObjectHandle> CreateWrapper(
      const scoped_refptr<Wrappable>& wrappable) const;

#if defined(__LB_LINUX__)
  typedef base::hash_map<Wrappable::Type, CreateWrapperFunction,
                         Wrappable::Type::hash_function>
      CreateWrapperFunctionMap;
#else
  typedef base::hash_map<Wrappable::Type, CreateWrapperFunction,
                         Wrappable::Type::hash_compare>
      CreateWrapperFunctionMap;
#endif
  CreateWrapperFunctionMap create_functions_;
  JSCGlobalObject* global_object_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // SCRIPT_JAVASCRIPTCORE_WRAPPER_FACTORY_H_
