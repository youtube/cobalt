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
#include "cobalt/script/v8c/v8c_engine.h"
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

  V8cUserObjectHolder() = default;
  V8cUserObjectHolder(v8::Isolate* isolate, v8::Local<v8::Value> value)
      : isolate_(isolate),
        handle_(isolate, value),
        prevent_garbage_collection_count_(0) {
    handle_.SetWeak();
  }
  V8cUserObjectHolder(const V8cUserObjectHolder&) = delete;
  V8cUserObjectHolder& operator=(const V8cUserObjectHolder&) = delete;
  V8cUserObjectHolder(V8cUserObjectHolder&& other)
      : isolate_(other.isolate_),
        handle_(other.isolate_, other.v8_value()),
        prevent_garbage_collection_count_(
            other.prevent_garbage_collection_count_) {
    if (prevent_garbage_collection_count_ == 0) {
      handle_.SetWeak();
    }
    other.isolate_ = nullptr;
    other.handle_.Clear();
    other.prevent_garbage_collection_count_ = 0;
  }
  V8cUserObjectHolder& operator=(V8cUserObjectHolder&& other) {
    isolate_ = other.isolate_;
    handle_.Set(isolate_, other.v8_value());
    prevent_garbage_collection_count_ = other.prevent_garbage_collection_count_;
    if (prevent_garbage_collection_count_ == 0) {
      handle_.SetWeak();
    }
    other.isolate_ = nullptr;
    other.handle_.Clear();
    other.prevent_garbage_collection_count_ = 0;
    return *this;
  }

  bool EqualTo(const BaseClass& other) const override {
    v8::HandleScope handle_scope(isolate_);
    const V8cUserObjectHolder* v8c_other =
        base::polymorphic_downcast<const V8cUserObjectHolder*>(&other);
    return v8_value() == v8c_other->v8_value();
  }

  void TraceMembers(Tracer* tracer) override {
    if (!handle_.IsEmpty()) {
      handle_.Get().RegisterExternalReference(isolate_);
    }
  }

  void RegisterOwner(Wrappable* owner) override {
    V8cEngine::GetFromIsolate(isolate_)->heap_tracer()->AddReferencedObject(
        owner, &handle_);
  }

  void DeregisterOwner(Wrappable* owner) override {
    // This will get called in destructors caused by finalizers caused by the
    // final shutdown GC.  In this case, the entire heap tracer will have
    // already been removed, so we don't need to bother removing ourselves.
    V8cHeapTracer* tracer = V8cEngine::GetFromIsolate(isolate_)->heap_tracer();
    if (tracer) {
      tracer->RemoveReferencedObject(owner, &handle_);
    }
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

  const typename V8cUserObjectType::BaseType* GetScriptValue() const override {
    return handle_.IsEmpty() ? nullptr : &handle_;
  }

  scoped_ptr<BaseClass> MakeCopy() const override {
    v8::HandleScope handle_scope(isolate_);
    return make_scoped_ptr<BaseClass>(
        new V8cUserObjectHolder(isolate_, v8_value()));
  }

  v8::Local<v8::Value> v8_value() const { return handle_.NewLocal(isolate_); }

 private:
  v8::Isolate* isolate_ = nullptr;
  V8cUserObjectType handle_{};
  int prevent_garbage_collection_count_ = 0;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_USER_OBJECT_HOLDER_H_
