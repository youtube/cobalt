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

#ifndef COBALT_SCRIPT_V8C_WEAK_HEAP_OBJECT_H_
#define COBALT_SCRIPT_V8C_WEAK_HEAP_OBJECT_H_

#include "cobalt/script/v8c/v8c_global_environment.h"
#include "cobalt/script/v8c/weak_heap_object_manager.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

// This class implements a weak reference to a v8::Value. The v8::Value that
// an instance of this class is created with may get garbage collected. In
// that case, subsequent calls to WeakHeapObject::Get() will return a NULL
// pointer.
class WeakHeapObject {
 public:
  WeakHeapObject(V8cGlobalEnvironment* env, v8::Local<v8::Value> value)
      : env_(env),
        weak_object_manager_(env->weak_object_manager()),
        value_(env->isolate(), value),
        was_collected_(false) {
    weak_object_manager_->StartTracking(this);
  }

  WeakHeapObject(const WeakHeapObject& other)
      : env_(other.env_),
        was_collected_(other.was_collected_),
        weak_object_manager_(other.weak_object_manager_),
        value_(env_->isolate(), other.value_.Get(env_->isolate())) {
    weak_object_manager_->StartTracking(this);
  }

  WeakHeapObject& operator=(const WeakHeapObject& rhs) {
    env_ = rhs.env_;
    was_collected_ = rhs.was_collected_;
    weak_object_manager_ = rhs.weak_object_manager_;
    value_.Reset(env_->isolate(), rhs.value_.Get(env_->isolate()));
    weak_object_manager_->StartTracking(this);
    return *this;
  }

  ~WeakHeapObject() { weak_object_manager_->StopTracking(this); }

  v8::Local<v8::Value> GetValue() const { return value_.Get(env_->isolate()); }

 private:
  V8cGlobalEnvironment* env_;
  WeakHeapObjectManager* weak_object_manager_;
  v8::Persistent<v8::Value, v8::CopyablePersistentTraits<v8::Value>> value_;
  bool was_collected_;

  friend class WeakHeapObjectManager;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_WEAK_HEAP_OBJECT_H_
