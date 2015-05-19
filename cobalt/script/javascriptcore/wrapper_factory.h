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
#include "cobalt/base/type_id.h"
#include "cobalt/script/script_object_handle.h"
#include "cobalt/script/wrappable.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/ClassInfo.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSObject.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

class JSCGlobalObject;

class WrapperFactory {
 public:
  typedef base::Callback<JSC::JSObject*(
      JSCGlobalObject*, const scoped_refptr<Wrappable>&)> CreateWrapperFunction;
  void RegisterWrappableType(base::TypeId wrappable_type,
                             const JSC::ClassInfo* class_info,
                             const CreateWrapperFunction& create_function);
  JSC::JSObject* GetWrapper(JSCGlobalObject* global_object,
                            const scoped_refptr<Wrappable>& wrappable) const;
  const JSC::ClassInfo* GetClassInfo(base::TypeId wrappable_type) const;

 private:
  scoped_ptr<ScriptObjectHandle> CreateWrapper(
      JSCGlobalObject* global_object,
      const scoped_refptr<Wrappable>& wrappable) const;

  struct WrappableTypeInfo {
    WrappableTypeInfo(const JSC::ClassInfo* info,
                      const CreateWrapperFunction& function)
        : class_info(info), create_function(function) {}
    const JSC::ClassInfo* class_info;
    CreateWrapperFunction create_function;
  };
  typedef base::hash_map<base::TypeId, WrappableTypeInfo> WrappableTypeInfoMap;

  WrappableTypeInfoMap wrappable_type_infos_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // SCRIPT_JAVASCRIPTCORE_WRAPPER_FACTORY_H_
