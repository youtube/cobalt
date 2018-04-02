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

#ifndef COBALT_SCRIPT_V8C_V8C_ARRAY_BUFFER_VIEW_H_
#define COBALT_SCRIPT_V8C_V8C_ARRAY_BUFFER_VIEW_H_

#include "base/logging.h"
#include "cobalt/script/array_buffer_view.h"
#include "cobalt/script/v8c/entry_scope.h"
#include "cobalt/script/v8c/type_traits.h"
#include "cobalt/script/v8c/v8c_exception_state.h"
#include "cobalt/script/v8c/v8c_user_object_holder.h"
#include "cobalt/script/v8c/weak_heap_object.h"

namespace cobalt {
namespace script {
namespace v8c {

class V8cArrayBufferView final : public ScopedPersistent<v8::Value>,
                                 public ArrayBufferView {
 public:
  using BaseType = ArrayBufferView;

  V8cArrayBufferView(v8::Isolate* isolate, JS::HandleValue value)
      : isolate_(isolate), weak_heap_object_(isolate, value) {
    DCHECK(value->IsArrayBufferView());
  }

  Handle<ArrayBuffer> Buffer() const override {
    return Handle<ArrayBuffer>(new V8cUserObjectHolder<V8cArrayBuffer>(
        isolate_, GetInternal()->Buffer()));
  }

  size_t ByteOffset() const override {
    EntryScope entry_scope(isolate_);
    return GetInternal()->ByteOffset();
  }

  size_t ByteLength() const override {
    EntryScope entry_scope(isolate_);
    return GetInternal()->ByteLength();
  }

  void* RawData() const override {
    EntryScope entry_scope(isolate_);
    auto self = GetInternal();
    void* data = self->Buffer()->GetContents().Data();
    uint8_t* incremented_data =
        static_cast<uint8_t*>(data) + self->ByteOffset();
    return static_cast<void*>(incremented_data);
  }

 private:
  v8::Local<v8::ArrayBufferView> GetInternal() const {
    return this->NewLocal(isolate_).As<v8::ArrayBufferView>();
  }

  v8::Isolate* isolate_;
};

template <>
struct TypeTraits<ArrayBufferView> {
  using ConversionType = V8cUserObjectHolder<V8cArrayBufferView>;
  using ReturnType = const ScriptValue<ArrayBufferView>*;
};

inline void ToJSValue(
    v8::Isolate* isolate,
    const ScriptValue<ArrayBufferView>* array_buffer_view_value,
    v8::Local<v8::Value>* out_value) {
  TRACK_MEMORY_SCOPE("Javascript");

  if (!array_buffer_view_value) {
    *out_value = v8::Null(isolate);
    return;
  }

  const auto* v8c_array_buffer_view_value = base::polymorphic_downcast<
      const V8cUserObjectHolder<V8cArrayBufferView>*>(array_buffer_view_value);
  *out_value = v8c_array_buffer_view_value->v8_value();
}

inline void FromJSValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, int conversion_flags,
    ExceptionState* exception_state,
    V8cUserObjectHolder<V8cArrayBufferView>* out_array_buffer_view) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK_EQ(0, conversion_flags);
  DCHECK(out_array_buffer_view);

  if (!value->IsObject()) {
    exception_state->SetSimpleException(kNotObjectType);
    return;
  }

  if (!value->IsArrayBufferView()) {
    exception_state->SetSimpleException(
        kTypeError, "Expected object of type ArrayBufferView");
    return;
  }

  *out_array_buffer_view =
      V8cUserObjectHolder<V8cArrayBufferView>(isolate, value);
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_ARRAY_BUFFER_VIEW_H_
