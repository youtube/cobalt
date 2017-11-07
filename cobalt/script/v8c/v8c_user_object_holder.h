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
//
// |V8cUserObjectType| should be a |ScopedPersistent<v8::Value>|.
template <typename V8cUserObjectType>
class V8cUserObjectHolder
    : public ScriptValue<typename V8cUserObjectType::BaseType> {
 public:
  typedef ScriptValue<typename V8cUserObjectType::BaseType> BaseClass;

  V8cUserObjectHolder()
      : isolate_(nullptr), prevent_garbage_collection_count_(0) {}

  V8cUserObjectHolder(v8::Isolate* isolate, v8::Local<v8::Value> value)
      : isolate_(isolate),
        handle_(isolate, value),
        prevent_garbage_collection_count_(0) {
    handle_.SetWeak();
  }

  V8cUserObjectHolder& operator=(const V8cUserObjectHolder& other) {
    isolate_ = other.isolate_;
    handle_.Set(isolate_, other.v8_value());
    return *this;
  }

  void RegisterOwner(Wrappable* owner) override {
    V8cGlobalEnvironment* global_environment =
        V8cGlobalEnvironment::GetFromIsolate(isolate_);
    v8::HandleScope handle_scope(isolate_);
    global_environment->AddReferencedObject(owner, v8_value());
  }

  void DeregisterOwner(Wrappable* owner) override {
    V8cGlobalEnvironment* global_environment =
        V8cGlobalEnvironment::GetFromIsolate(isolate_);
    if (!global_environment) {
      // TODO: This will get hit when finalization callbacks get run during
      // shut down, in between the time in which an isolate and context exist.
      // It might be safe to just no-op, but we might have to do something
      // different, such as stopping early in the callback if some flag that
      // lives on the isolate is set.
      LOG(WARNING) << "DeregisterOwner after global environment destroyed.";
      return;
    }
    v8::HandleScope handle_scope(isolate_);
    global_environment->RemoveReferencedObject(owner, v8_value());
  }

  void PreventGarbageCollection() override {
    if (prevent_garbage_collection_count_++ == 0 && !handle_.IsEmpty()) {
      handle_.ClearWeak();
    }
  }

  void AllowGarbageCollection() override {
    DCHECK_GT(prevent_garbage_collection_count_, 0);
    if (--prevent_garbage_collection_count_ == 0 && !handle_.IsEmpty()) {
      handle_.SetWeak();
    }
  }

  scoped_ptr<BaseClass> MakeCopy() const override {
    v8::HandleScope handle_scope(isolate_);
    return make_scoped_ptr<BaseClass>(
        new V8cUserObjectHolder(isolate_, v8_value()));
  }

  bool EqualTo(const BaseClass& other) const override {
    v8::HandleScope handle_scope(isolate_);
    const V8cUserObjectHolder* v8c_other =
        base::polymorphic_downcast<const V8cUserObjectHolder*>(&other);
    return v8_value() == v8c_other->v8_value();
  }

  const typename V8cUserObjectType::BaseType* GetScriptValue() const override {
    return handle_.IsEmpty() ? nullptr : &handle_;
  }

  v8::Local<v8::Value> v8_value() const { return handle_.NewLocal(isolate_); }

 private:
  v8::Isolate* isolate_;
  V8cUserObjectType handle_;
  int prevent_garbage_collection_count_;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_USER_OBJECT_HOLDER_H_
