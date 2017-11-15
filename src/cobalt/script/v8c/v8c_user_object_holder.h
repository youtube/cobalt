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

#ifndef COBALT_SCRIPT_V8C_V8C_USER_OBJECT_HOLDER_H_
#define COBALT_SCRIPT_V8C_V8C_USER_OBJECT_HOLDER_H_

#include "cobalt/script/script_value.h"
#include "cobalt/script/v8c/v8c_global_environment.h"
#include "cobalt/script/v8c/weak_heap_object.h"
#include "cobalt/script/v8c/wrapper_private.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

// Template class that implements the ScriptValue template class for lifetime
// management of User Objects passed from the bindings layer into Cobalt. See
// the definition of ScriptValue for further detail.
// This class does not root the underlying v8::Object that V8cUserObjectType
// holds a reference to. The object will be traced when any owning objects are
// traced.
template <typename V8cUserObjectType>
class V8cUserObjectHolder
    : public ScriptValue<typename V8cUserObjectType::BaseType> {
 public:
  typedef ScriptValue<typename V8cUserObjectType::BaseType> BaseClass;

  V8cUserObjectHolder() : env_(nullptr), prevent_garbage_collection_count_(0) {}

  V8cUserObjectHolder(V8cGlobalEnvironment* env, v8::Local<v8::Value> value)
      : env_(env),
        handle_(V8cUserObjectType(env, value)),
        prevent_garbage_collection_count_(0) {}

  void RegisterOwner(Wrappable* owner) override { NOTIMPLEMENTED(); }
  void DeregisterOwner(Wrappable* owner) override { NOTIMPLEMENTED(); }
  void PreventGarbageCollection() override { NOTIMPLEMENTED(); }
  void AllowGarbageCollection() override { NOTIMPLEMENTED(); }
  scoped_ptr<BaseClass> MakeCopy() const override {
    DCHECK(handle_);
    v8::Local<v8::Value> value = handle_->value();
    return make_scoped_ptr<BaseClass>(new V8cUserObjectHolder(env_, value));
  }
  bool EqualTo(const BaseClass& other) const override {
    NOTIMPLEMENTED();
    return false;
  }
  const typename V8cUserObjectType::BaseType* GetScriptValue() const override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  v8::Local<v8::Value> v8_value() const { return handle_->value(); }

 private:
  V8cGlobalEnvironment* env_;
  base::optional<V8cUserObjectType> handle_;
  int prevent_garbage_collection_count_;
  v8::Global<v8::Value> persistent_root_;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_USER_OBJECT_HOLDER_H_
