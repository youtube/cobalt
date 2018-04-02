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

#ifndef COBALT_SCRIPT_MOZJS_45_MOZJS_TYPED_ARRAYS_H_
#define COBALT_SCRIPT_MOZJS_45_MOZJS_TYPED_ARRAYS_H_

#include "base/memory/ref_counted.h"
#include "cobalt/script/array_buffer.h"
#include "cobalt/script/array_buffer_view.h"
#include "cobalt/script/exception_message.h"
#include "cobalt/script/mozjs-45/mozjs_array_buffer.h"
#include "cobalt/script/script_exception.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/typed_arrays.h"

namespace cobalt {
namespace script {
namespace mozjs {

class MozjsTypedArray final : public TypedArray {
 public:
  using BaseType = TypedArray;

  MozjsTypedArray(JSContext* context, JS::HandleValue value)
      : context_(context), weak_heap_object_(context, value) {
    DCHECK(value.isObject());
    DCHECK(JS_IsTypedArrayObject(&value.toObject()));
  }

  JSObject* handle() const { return weak_heap_object_.GetObject(); }
  const JS::Value& value() const { return weak_heap_object_.GetValue(); }
  bool WasCollected() const { return weak_heap_object_.WasCollected(); }

  Handle<ArrayBuffer> Buffer() const override {
    JSAutoRequest auto_request(context_);
    JS::RootedObject array_buffer_view(context_, weak_heap_object_.GetObject());
    bool is_shared_memory;
    JSObject* object = JS_GetArrayBufferViewBuffer(context_, array_buffer_view,
                                                   &is_shared_memory);
    JS::RootedValue value(context_);
    value.setObject(*object);
    return Handle<ArrayBuffer>(
        new MozjsUserObjectHolder<MozjsArrayBuffer>(context_, value));
  }

  size_t Length() const override {
    return JS_GetTypedArrayLength(weak_heap_object_.GetObject());
  }
  size_t ByteOffset() const override {
    return JS_GetTypedArrayByteOffset(weak_heap_object_.GetObject());
  }
  size_t ByteLength() const override {
    return JS_GetTypedArrayByteLength(weak_heap_object_.GetObject());
  }

  void* RawData() const override {
    bool is_shared_memory;
    JS::AutoCheckCannotGC no_gc;
    return JS_GetArrayBufferViewData(weak_heap_object_.GetObject(),
                                     &is_shared_memory, no_gc);
  }

 private:
  JSContext* context_;
  WeakHeapObject weak_heap_object_;
};

template <>
struct TypeTraits<TypedArray> {
  using ConversionType = MozjsUserObjectHolder<MozjsTypedArray>;
  using ReturnType = const ScriptValue<TypedArray>*;
};

inline void ToJSValue(JSContext* context,
                      const ScriptValue<TypedArray>* typed_array_value,
                      JS::MutableHandleValue out_value) {
  TRACK_MEMORY_SCOPE("Javascript");

  if (!typed_array_value) {
    out_value.set(JS::NullValue());
    return;
  }

  const auto* mozjs_typed_array_value =
      base::polymorphic_downcast<const MozjsUserObjectHolder<MozjsTypedArray>*>(
          typed_array_value);
  out_value.setObject(*mozjs_typed_array_value->js_object());
}

inline void FromJSValue(
    JSContext* context, JS::HandleValue value, int conversion_flags,
    ExceptionState* exception_state,
    MozjsUserObjectHolder<MozjsTypedArray>* out_typed_array) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK_EQ(0, conversion_flags);
  DCHECK(out_typed_array);

  if (!value.isObject()) {
    exception_state->SetSimpleException(kNotObjectType);
    return;
  }

  if (!JS_IsTypedArrayObject(&value.toObject())) {
    exception_state->SetSimpleException(kTypeError,
                                        "Expected object of type TypedArray");
    return;
  }

  *out_typed_array = MozjsUserObjectHolder<MozjsTypedArray>(context, value);
}

template <typename CType, bool IsClamped, typename BaseTypeName,
          bool (*JS_IsFunction)(JSObject*),
          void (*js_GetArrayLengthAndDataFunction)(JSObject*, uint32_t*, bool*,
                                                   CType**),
          CType* (*JS_GetArrayDataFunction)(JSObject*, bool*,
                                            const JS::AutoCheckCannotGC&)>
class MozjsTypedArrayImpl final : public BaseTypeName {
 public:
  using BaseType = BaseTypeName;

  MozjsTypedArrayImpl(JSContext* context, JS::HandleValue value)
      : context_(context), weak_heap_object_(context, value) {
    DCHECK(value.isObject());
    DCHECK(JS_IsFunction(&value.toObject()));
  }

  JSObject* handle() const { return weak_heap_object_.GetObject(); }
  const JS::Value& value() const { return weak_heap_object_.GetValue(); }
  bool WasCollected() const { return weak_heap_object_.WasCollected(); }

  Handle<ArrayBuffer> Buffer() const override {
    JSAutoRequest auto_request(context_);
    JS::RootedObject array_buffer_view(context_, weak_heap_object_.GetObject());
    bool is_shared_memory;
    JSObject* object = JS_GetArrayBufferViewBuffer(context_, array_buffer_view,
                                                   &is_shared_memory);
    JS::RootedValue value(context_);
    value.setObject(*object);
    return Handle<ArrayBuffer>(
        new MozjsUserObjectHolder<MozjsArrayBuffer>(context_, value));
  }

  size_t Length() const override {
    uint32_t length;
    bool is_shared_memory;
    CType* data;
    js_GetArrayLengthAndDataFunction(weak_heap_object_.GetObject(), &length,
                                     &is_shared_memory, &data);
    return length;
  }

  size_t ByteOffset() const override {
    return JS_GetTypedArrayByteOffset(weak_heap_object_.GetObject());
  }

  size_t ByteLength() const override {
    return JS_GetTypedArrayByteLength(weak_heap_object_.GetObject());
  }

  CType* Data() const override {
    bool is_shared_memory;
    JS::AutoCheckCannotGC no_gc;
    return JS_GetArrayDataFunction(weak_heap_object_.GetObject(),
                                   &is_shared_memory, no_gc);
  }

  void* RawData() const override { return static_cast<void*>(Data()); }

 private:
  JSContext* context_;
  WeakHeapObject weak_heap_object_;
};

#define COBALT_SCRIPT_USING_MOZJS_ARRAY(array, ctype)        \
  using Mozjs##array =                                       \
      MozjsTypedArrayImpl<ctype, false, array, JS_Is##array, \
                          js::Get##array##LengthAndData, JS_Get##array##Data>;
COBALT_SCRIPT_TYPED_ARRAY_LIST(COBALT_SCRIPT_USING_MOZJS_ARRAY)
#undef COBALT_SCRIPT_USING_MOZJS_ARRAY

#define COBALT_SCRIPT_CONVERSION_BOILERPLATE(array, ctype)                  \
  template <>                                                               \
  struct TypeTraits<array> {                                                \
    using ConversionType = MozjsUserObjectHolder<Mozjs##array>;             \
    using ReturnType = const ScriptValue<array>*;                           \
  };                                                                        \
                                                                            \
  inline void ToJSValue(JSContext* context,                                 \
                        const ScriptValue<array>* array_value,              \
                        JS::MutableHandleValue out_value) {                 \
    TRACK_MEMORY_SCOPE("Javascript");                                       \
    if (!array_value) {                                                     \
      out_value.set(JS::NullValue());                                       \
      return;                                                               \
    }                                                                       \
    const auto* mozjs_array_value = base::polymorphic_downcast<             \
        const MozjsUserObjectHolder<Mozjs##array>*>(array_value);           \
    out_value.setObject(*mozjs_array_value->js_object());                   \
  }                                                                         \
                                                                            \
  inline void FromJSValue(JSContext* context, JS::HandleValue value,        \
                          int conversion_flags,                             \
                          ExceptionState* exception_state,                  \
                          MozjsUserObjectHolder<Mozjs##array>* out_array) { \
    TRACK_MEMORY_SCOPE("Javascript");                                       \
    DCHECK_EQ(0, conversion_flags);                                         \
    DCHECK(out_array);                                                      \
    if (!value.isObject()) {                                                \
      exception_state->SetSimpleException(kNotObjectType);                  \
      return;                                                               \
    }                                                                       \
    if (!JS_Is##array(&value.toObject())) {                                 \
      exception_state->SetSimpleException(kTypeError,                       \
                                          "Expected object of type array"); \
      return;                                                               \
    }                                                                       \
    *out_array = MozjsUserObjectHolder<Mozjs##array>(context, value);       \
  }
COBALT_SCRIPT_TYPED_ARRAY_LIST(COBALT_SCRIPT_CONVERSION_BOILERPLATE)
#undef COBALT_SCRIPT_CONVERSION_BOILERPLATE

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_TYPED_ARRAYS_H_
