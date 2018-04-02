// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_V8C_V8C_ARRAY_BUFFER_H_
#define COBALT_SCRIPT_V8C_V8C_ARRAY_BUFFER_H_

#include "base/logging.h"
#include "cobalt/script/array_buffer.h"
#include "cobalt/script/v8c/entry_scope.h"
#include "cobalt/script/v8c/scoped_persistent.h"
#include "cobalt/script/v8c/type_traits.h"
#include "cobalt/script/v8c/v8c_exception_state.h"
#include "cobalt/script/v8c/v8c_user_object_holder.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

class V8cArrayBuffer final : public ScopedPersistent<v8::Value>,
                             public ArrayBuffer {
 public:
  using BaseType = ArrayBuffer;

  V8cArrayBuffer(v8::Isolate* isolate, v8::Local<v8::Value> value)
      : isolate_(isolate), ScopedPersistent(isolate, value) {
    DCHECK(value->IsArrayBuffer());
  }

  size_t ByteLength() const override {
    EntryScope entry_scope(isolate_);
    return GetInternal()->ByteLength();
  }

  void* Data() const override {
    EntryScope entry_scope(isolate_);
    return GetInternal()->GetContents().Data();
  }

 private:
  v8::Local<v8::ArrayBuffer> GetInternal() const {
    return this->NewLocal(isolate_).As<v8::ArrayBuffer>();
  }

  v8::Isolate* isolate_;
};

template <>
struct TypeTraits<ArrayBuffer> {
  using ConversionType = V8cUserObjectHolder<V8cArrayBuffer>;
  using ReturnType = const ScriptValue<ArrayBuffer>*;
};

inline void ToJSValue(v8::Isolate* isolate,
                      const ScriptValue<ArrayBuffer>* array_buffer_value,
                      v8::Local<v8::Value>* out_value) {
  if (!array_buffer_value) {
    *out_value = v8::Null(isolate);
    return;
  }

  const auto* v8c_array_buffer_value =
      base::polymorphic_downcast<const V8cUserObjectHolder<V8cArrayBuffer>*>(
          array_buffer_value);
  *out_value = v8c_array_buffer_value->v8_value();
}

inline void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                        int conversion_flags, ExceptionState* exception_state,
                        V8cUserObjectHolder<V8cArrayBuffer>* out_array_buffer) {
  DCHECK_EQ(0, conversion_flags);
  DCHECK(out_array_buffer);

  if (!value->IsObject()) {
    exception_state->SetSimpleException(kNotObjectType);
    return;
  }

  if (!value->IsArrayBuffer()) {
    exception_state->SetSimpleException(kTypeError,
                                        "Expected object of type ArrayBuffer");
    return;
  }

  *out_array_buffer = V8cUserObjectHolder<V8cArrayBuffer>(isolate, value);
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_ARRAY_BUFFER_H_
