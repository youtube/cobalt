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

#ifndef COBALT_SCRIPT_V8C_V8C_VALUE_HANDLE_H_
#define COBALT_SCRIPT_V8C_V8C_VALUE_HANDLE_H_

#include "base/optional.h"
#include "cobalt/script/v8c/type_traits.h"
#include "cobalt/script/v8c/v8c_user_object_holder.h"
#include "cobalt/script/v8c/weak_heap_object.h"
#include "cobalt/script/value_handle.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

// A wrapper around a v8::Value that can be passed into Cobalt as a script
// value object.
//
// An ValueHandle is never passed into Cobalt as-is, but only when wrapped as a
// ScriptValue<ValueHandle>.
class V8cValueHandle : public ValueHandle {
 public:
  typedef ValueHandle BaseType;

  v8::Local<v8::Value> value() const { return handle_.GetValue(); }

 private:
  V8cValueHandle(V8cGlobalEnvironment* env, v8::Local<v8::Value> value)
      : handle_(env, value) {}

  WeakHeapObject handle_;

  friend class V8cUserObjectHolder<V8cValueHandle>;
  friend class base::optional<V8cValueHandle>;
};

typedef V8cUserObjectHolder<V8cValueHandle> V8cValueHandleHolder;

template <>
struct TypeTraits<ValueHandle> {
  typedef V8cValueHandleHolder ConversionType;
  typedef const ScriptValue<ValueHandle>* ReturnType;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_VALUE_HANDLE_H_
