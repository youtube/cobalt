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

#ifndef COBALT_SCRIPT_V8C_V8C_TYPED_ARRAYS_H_
#define COBALT_SCRIPT_V8C_V8C_TYPED_ARRAYS_H_

#include "base/memory/ref_counted.h"
#include "cobalt/script/array_buffer.h"
#include "cobalt/script/array_buffer_view.h"
#include "cobalt/script/exception_message.h"
#include "cobalt/script/script_exception.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/typed_arrays.h"
#include "cobalt/script/v8c/entry_scope.h"
#include "cobalt/script/v8c/v8c_array_buffer.h"

namespace cobalt {
namespace script {
namespace v8c {

class V8cTypedArray final : public ScopedPersistent<v8::Value>,
                            public TypedArray {
 public:
  using BaseType = TypedArray;

  V8cTypedArray(v8::Isolate* isolate, v8::Local<v8::Value> value)
      : isolate_(isolate), ScopedPersistent(isolate, value) {
    DCHECK(value->IsTypedArray());
  }

  Handle<ArrayBuffer> Buffer() const override {
    EntryScope entry_scope(isolate_);
    v8::Local<v8::ArrayBuffer> array_buffer = GetInternal()->Buffer();
    return Handle<ArrayBuffer>(
        new V8cUserObjectHolder<V8cArrayBuffer>(isolate_, array_buffer));
  }

  size_t Length() const override {
    EntryScope entry_scope(isolate_);
    return GetInternal()->Length();
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
  v8::Local<v8::TypedArray> GetInternal() const {
    return this->NewLocal(isolate_).As<v8::TypedArray>();
  }

  v8::Isolate* isolate_;
};

template <>
struct TypeTraits<TypedArray> {
  using ConversionType = V8cUserObjectHolder<V8cTypedArray>;
  using ReturnType = const ScriptValue<TypedArray>*;
};

inline void ToJSValue(v8::Isolate* isolate,
                      const ScriptValue<TypedArray>* typed_array_value,
                      v8::Local<v8::Value>* out_value) {
  if (!typed_array_value) {
    *out_value = v8::Null(isolate);
    return;
  }

  const auto* v8c_typed_array_value =
      base::polymorphic_downcast<const V8cUserObjectHolder<V8cTypedArray>*>(
          typed_array_value);
  *out_value = v8c_typed_array_value->v8_value();
}

inline void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                        int conversion_flags, ExceptionState* exception_state,
                        V8cUserObjectHolder<V8cTypedArray>* out_typed_array) {
  DCHECK_EQ(0, conversion_flags);
  DCHECK(out_typed_array);

  if (!value->IsObject()) {
    exception_state->SetSimpleException(kNotObjectType);
    return;
  }

  if (!value->IsTypedArray()) {
    exception_state->SetSimpleException(kTypeError,
                                        "Expected object of type TypedArray");
    return;
  }

  *out_typed_array =
      V8cUserObjectHolder<V8cTypedArray>(isolate, value.As<v8::TypedArray>());
}

template <typename CType, typename BaseTypeName, typename V8Type,
          bool (v8::Value::*V8ValueIsTypeFunction)() const>
class V8cTypedArrayImpl final : public ScopedPersistent<v8::Value>,
                                public BaseTypeName {
 public:
  using BaseType = BaseTypeName;

  V8cTypedArrayImpl(v8::Isolate* isolate, v8::Local<v8::Value> value)
      : isolate_(isolate), ScopedPersistent(isolate, value) {
    DCHECK(((**value).*(V8ValueIsTypeFunction))());
  }

  Handle<ArrayBuffer> Buffer() const override {
    EntryScope entry_scope(isolate_);
    v8::Local<v8::ArrayBuffer> array_buffer = GetInternal()->Buffer();
    return Handle<ArrayBuffer>(
        new V8cUserObjectHolder<V8cArrayBuffer>(isolate_, array_buffer));
  }

  size_t Length() const override {
    EntryScope entry_scope(isolate_);
    return GetInternal()->Length();
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

  CType* Data() const override { return static_cast<CType*>(RawData()); }

 private:
  v8::Local<V8Type> GetInternal() const {
    return this->NewLocal(isolate_).template As<V8Type>();
  }

  v8::Isolate* isolate_;
};

#define COBALT_SCRIPT_USING_V8C_ARRAY(array, ctype) \
  using V8c##array =                                \
      V8cTypedArrayImpl<ctype, array, v8::array, &v8::Value::Is##array>;
COBALT_SCRIPT_TYPED_ARRAY_LIST(COBALT_SCRIPT_USING_V8C_ARRAY)
#undef COBALT_SCRIPT_USING_V8C_ARRAY

#define COBALT_SCRIPT_CONVERSION_BOILERPLATE(array, ctype)                  \
  template <>                                                               \
  struct TypeTraits<array> {                                                \
    using ConversionType = V8cUserObjectHolder<V8c##array>;                 \
    using ReturnType = const ScriptValue<array>*;                           \
  };                                                                        \
                                                                            \
  inline void ToJSValue(v8::Isolate* isolate,                               \
                        const ScriptValue<array>* array_value,              \
                        v8::Local<v8::Value>* out_value) {                  \
    if (!array_value) {                                                     \
      *out_value = v8::Null(isolate);                                       \
      return;                                                               \
    }                                                                       \
    const auto* v8c_array_value =                                           \
        base::polymorphic_downcast<const V8cUserObjectHolder<V8c##array>*>( \
            array_value);                                                   \
    *out_value = v8c_array_value->v8_value();                               \
  }                                                                         \
                                                                            \
  inline void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value, \
                          int conversion_flags,                             \
                          ExceptionState* exception_state,                  \
                          V8cUserObjectHolder<V8c##array>* out_array) {     \
    DCHECK_EQ(0, conversion_flags);                                         \
    DCHECK(out_array);                                                      \
    if (!value->IsObject()) {                                               \
      exception_state->SetSimpleException(kNotObjectType);                  \
      return;                                                               \
    }                                                                       \
    if (!value->Is##array()) {                                              \
      exception_state->SetSimpleException(kTypeError,                       \
                                          "Expected object of type array"); \
      return;                                                               \
    }                                                                       \
    *out_array = V8cUserObjectHolder<V8c##array>(isolate, value);           \
  }
COBALT_SCRIPT_TYPED_ARRAY_LIST(COBALT_SCRIPT_CONVERSION_BOILERPLATE)
#undef COBALT_SCRIPT_CONVERSION_BOILERPLATE

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_TYPED_ARRAYS_H_
