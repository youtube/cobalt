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

#ifndef COBALT_SCRIPT_V8C_WRAPPER_FACTORY_H_
#define COBALT_SCRIPT_V8C_WRAPPER_FACTORY_H_

#include "base/hash_tables.h"
#include "cobalt/script/wrappable.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

class V8cGlobalEnvironment;
class WrapperPrivate;

// Holds a mapping between Wrappable types and base::Callbacks that create new
// Wrapper objects corresponding to the Wrappable type.
class WrapperFactory : public Wrappable::CachedWrapperAccessor {
 public:
  // Callback to create a new v8::Object that is a wrapper for the Wrappable.
  typedef base::Callback<v8::Local<v8::Object>(v8::Isolate*,
                                               const scoped_refptr<Wrappable>&)>
      CreateWrapperFunction;

  // Callback to get v8::FunctionTemplate of an interface.
  typedef base::Callback<v8::Local<v8::FunctionTemplate>(v8::Isolate*)>
      GetFunctionTemplate;

  explicit WrapperFactory(v8::Isolate* isolate) : isolate_(isolate) {}

  void RegisterWrappableType(base::TypeId wrappable_type,
                             const CreateWrapperFunction& create_function,
                             const GetFunctionTemplate& class_function);

  v8::Local<v8::Object> GetWrapper(const scoped_refptr<Wrappable>& wrappable);

  // Attempt to get the |WrapperPrivate| associated with |wrappable|.  Returns
  // |nullptr| if no |WrapperPrivate| was found.
  WrapperPrivate* MaybeGetWrapperPrivate(Wrappable* wrappable);

  bool DoesObjectImplementInterface(v8::Local<v8::Object> object,
                                    base::TypeId id) const;

 private:
  struct WrappableTypeFunctions {
    CreateWrapperFunction create_wrapper;
    GetFunctionTemplate get_function_template;
    WrappableTypeFunctions(const CreateWrapperFunction& create_wrapper,
                           const GetFunctionTemplate& get_function_template)
        : create_wrapper(create_wrapper),
          get_function_template(get_function_template) {}
  };

  scoped_ptr<Wrappable::WeakWrapperHandle> CreateWrapper(
      const scoped_refptr<Wrappable>& wrappable) const;

  typedef base::hash_map<base::TypeId, WrappableTypeFunctions>
      WrappableTypeFunctionsHashMap;

  v8::Isolate* isolate_;
  WrappableTypeFunctionsHashMap wrappable_type_functions_;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_WRAPPER_FACTORY_H_
