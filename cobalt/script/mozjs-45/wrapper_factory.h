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

#ifndef COBALT_SCRIPT_MOZJS_45_WRAPPER_FACTORY_H_
#define COBALT_SCRIPT_MOZJS_45_WRAPPER_FACTORY_H_

#include "base/bind.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/type_id.h"
#include "cobalt/script/wrappable.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// Holds a mapping between Wrappable types and base::Callbacks that create
// new Wrapper objects corresponding to the Wrappable type.
// TODO: Investigate whether shared functionality with the JSC
// version can be refactored out.
class WrapperFactory : public Wrappable::CachedWrapperAccessor {
 public:
  // Callback to create a new JSObject that is a wrapper for the Wrappable.
  typedef base::Callback<JSObject*(JSContext*, const scoped_refptr<Wrappable>&)>
      CreateWrapperFunction;

  // Callback to get JSClass* of prototype.
  typedef base::Callback<const JSClass*(JSContext*)> PrototypeClassFunction;

  explicit WrapperFactory(JSContext* context) : context_(context) {}
  void RegisterWrappableType(base::TypeId wrappable_type,
                             const CreateWrapperFunction& create_function,
                             const PrototypeClassFunction& class_function);

  // Gets the Proxy for the Wrapper object for this Wrappable. It may create a
  // new Wrapper and Proxy.
  JSObject* GetWrapperProxy(const scoped_refptr<Wrappable>& wrappable) const;

  // Check whether the Proxy for the Wrapper object for this Wrappable exists
  // or not. This will NOT create a new Wrapper and Proxy if they do not
  // already exist.
  bool HasWrapperProxy(const scoped_refptr<Wrappable>& wrappable) const;

  // Returns true if this JSObject is a Wrapper object.
  bool IsWrapper(JS::HandleObject wrapper) const;

  bool DoesObjectImplementInterface(JS::HandleObject object,
                                    base::TypeId id) const;

 private:
  struct WrappableTypeFunctions {
    CreateWrapperFunction create_wrapper;
    PrototypeClassFunction prototype_class;
    WrappableTypeFunctions(const CreateWrapperFunction& create_wrapper,
                           const PrototypeClassFunction& prototype_class)
        : create_wrapper(create_wrapper), prototype_class(prototype_class) {}
  };

  scoped_ptr<Wrappable::WeakWrapperHandle> CreateWrapper(
      const scoped_refptr<Wrappable>& wrappable) const;

  typedef base::hash_map<base::TypeId, WrappableTypeFunctions>
      WrappableTypeFunctionsHashMap;

  JSContext* context_;
  WrappableTypeFunctionsHashMap wrappable_type_functions_;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_WRAPPER_FACTORY_H_
