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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_WRAPPER_FACTORY_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_WRAPPER_FACTORY_H_

#include "base/bind.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/type_id.h"
#include "cobalt/script/wrappable.h"
#include "third_party/WebKit/Source/JavaScriptCore/config.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/ClassInfo.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSObject.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

class JSCGlobalObject;

class WrapperFactory : public Wrappable::CachedWrapperAccessor {
 public:
  typedef base::Callback<JSC::JSObject*(
      JSCGlobalObject*, const scoped_refptr<Wrappable>&)> CreateWrapperFunction;
  void RegisterWrappableType(base::TypeId wrappable_type,
                             const JSC::ClassInfo* class_info,
                             const CreateWrapperFunction& create_function);

  // Gets a Wrapper object for this Wrappable. It may create a new Wrapper.
  JSC::JSObject* GetWrapper(JSCGlobalObject* global_object,
                            const scoped_refptr<Wrappable>& wrappable) const;

  // Returns true if this JSObject is a Wrapper object.
  bool IsWrapper(JSC::JSObject* js_object) const;

  // Gets the JSC::ClassInfo that corresponds to this Wrappable type.
  const JSC::ClassInfo* GetClassInfo(base::TypeId wrappable_type) const;

 private:
  scoped_ptr<Wrappable::WeakWrapperHandle> CreateWrapper(
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
  typedef base::hash_set<const JSC::ClassInfo*> WrapperClassInfoSet;

  WrappableTypeInfoMap wrappable_type_infos_;
  WrapperClassInfoSet wrapper_class_infos_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_WRAPPER_FACTORY_H_
