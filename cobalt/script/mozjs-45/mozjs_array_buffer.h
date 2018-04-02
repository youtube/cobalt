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

#ifndef COBALT_SCRIPT_MOZJS_45_MOZJS_ARRAY_BUFFER_H_
#define COBALT_SCRIPT_MOZJS_45_MOZJS_ARRAY_BUFFER_H_

#include "base/logging.h"
#include "cobalt/script/array_buffer.h"
#include "cobalt/script/mozjs-45/mozjs_exception_state.h"
#include "cobalt/script/mozjs-45/mozjs_user_object_holder.h"
#include "cobalt/script/mozjs-45/type_traits.h"
#include "cobalt/script/mozjs-45/weak_heap_object.h"
#include "third_party/mozjs-45/js/src/jsapi.h"
#include "third_party/mozjs-45/js/src/jsfriendapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

class MozjsArrayBuffer final : public ArrayBuffer {
 public:
  using BaseType = ArrayBuffer;

  MozjsArrayBuffer(JSContext* context, JS::HandleValue value)
      : context_(context), weak_heap_object_(context, value) {
    DCHECK(value.isObject());
    DCHECK(JS_IsArrayBufferObject(&value.toObject()));
  }

  JSObject* handle() const { return weak_heap_object_.GetObject(); }
  const JS::Value& value() const { return weak_heap_object_.GetValue(); }
  bool WasCollected() const { return weak_heap_object_.WasCollected(); }

  size_t ByteLength() const override {
    return JS_GetArrayBufferByteLength(weak_heap_object_.GetObject());
  }

  void* Data() const override {
    bool is_shared_memory;
    JS::AutoCheckCannotGC no_gc;
    return JS_GetArrayBufferData(weak_heap_object_.GetObject(),
                                 &is_shared_memory, no_gc);
  }

 private:
  JSContext* context_;
  WeakHeapObject weak_heap_object_;
};

template <>
struct TypeTraits<ArrayBuffer> {
  using ConversionType = MozjsUserObjectHolder<MozjsArrayBuffer>;
  using ReturnType = const ScriptValue<ArrayBuffer>*;
};

inline void ToJSValue(JSContext* context,
                      const ScriptValue<ArrayBuffer>* array_buffer_value,
                      JS::MutableHandleValue out_value) {
  TRACK_MEMORY_SCOPE("Javascript");

  if (!array_buffer_value) {
    out_value.set(JS::NullValue());
    return;
  }

  const auto* mozjs_array_buffer_value = base::polymorphic_downcast<
      const MozjsUserObjectHolder<MozjsArrayBuffer>*>(array_buffer_value);
  out_value.setObject(*mozjs_array_buffer_value->js_object());
}

inline void FromJSValue(
    JSContext* context, JS::HandleValue value, int conversion_flags,
    ExceptionState* exception_state,
    MozjsUserObjectHolder<MozjsArrayBuffer>* out_array_buffer) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK_EQ(0, conversion_flags);
  DCHECK(out_array_buffer);

  if (!value.isObject()) {
    exception_state->SetSimpleException(kNotObjectType);
    return;
  }

  if (!JS_IsArrayBufferObject(&value.toObject())) {
    exception_state->SetSimpleException(kTypeError,
                                        "Expected object of type ArrayBuffer");
    return;
  }

  *out_array_buffer = MozjsUserObjectHolder<MozjsArrayBuffer>(context, value);
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_ARRAY_BUFFER_H_
