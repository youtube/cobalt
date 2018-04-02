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

#ifndef COBALT_SCRIPT_MOZJS_45_MOZJS_ARRAY_BUFFER_VIEW_H_
#define COBALT_SCRIPT_MOZJS_45_MOZJS_ARRAY_BUFFER_VIEW_H_

#include "base/logging.h"
#include "cobalt/script/array_buffer_view.h"
#include "cobalt/script/mozjs-45/mozjs_exception_state.h"
#include "cobalt/script/mozjs-45/mozjs_user_object_holder.h"
#include "cobalt/script/mozjs-45/type_traits.h"
#include "cobalt/script/mozjs-45/weak_heap_object.h"
#include "third_party/mozjs-45/js/src/jsapi.h"
#include "third_party/mozjs-45/js/src/jsfriendapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

class MozjsArrayBufferView final : public ArrayBufferView {
 public:
  using BaseType = ArrayBufferView;

  MozjsArrayBufferView(JSContext* context, JS::HandleValue value)
      : context_(context), weak_heap_object_(context, value) {
    DCHECK(value.isObject());
    DCHECK(JS_IsArrayBufferViewObject(&value.toObject()));
  }

  JSObject* handle() const { return weak_heap_object_.GetObject(); }
  const JS::Value& value() const { return weak_heap_object_.GetValue(); }
  bool WasCollected() const { return weak_heap_object_.WasCollected(); }
  void Trace(JSTracer* js_tracer) { weak_heap_object_.Trace(js_tracer); }

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

  size_t ByteOffset() const override {
    JSObject* object = weak_heap_object_.GetObject();
    if (JS_IsTypedArrayObject(object)) {
      return JS_GetTypedArrayByteOffset(object);
    } else if (JS_IsDataViewObject(object)) {
      return JS_GetDataViewByteOffset(object);
    }

    // Typed arrays and DataView are the only classes (in the JavaScript
    // sense) that implement ArrayBufferView.
    NOTREACHED();
    return 0u;
  }

  size_t ByteLength() const override {
    return JS_GetArrayBufferViewByteLength(weak_heap_object_.GetObject());
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
struct TypeTraits<ArrayBufferView> {
  using ConversionType = MozjsUserObjectHolder<MozjsArrayBufferView>;
  using ReturnType = const ScriptValue<ArrayBufferView>*;
};

inline void ToJSValue(
    JSContext* context,
    const ScriptValue<ArrayBufferView>* array_buffer_view_value,
    JS::MutableHandleValue out_value) {
  TRACK_MEMORY_SCOPE("Javascript");

  if (!array_buffer_view_value) {
    out_value.set(JS::NullValue());
    return;
  }

  const auto* mozjs_array_buffer_view_value = base::polymorphic_downcast<
      const MozjsUserObjectHolder<MozjsArrayBufferView>*>(
      array_buffer_view_value);
  out_value.setObject(*mozjs_array_buffer_view_value->js_object());
}

inline void FromJSValue(
    JSContext* context, JS::HandleValue value, int conversion_flags,
    ExceptionState* exception_state,
    MozjsUserObjectHolder<MozjsArrayBufferView>* out_array_buffer_view) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK_EQ(0, conversion_flags);
  DCHECK(out_array_buffer_view);

  if (!value.isObject()) {
    exception_state->SetSimpleException(kNotObjectType);
    return;
  }

  if (!JS_IsArrayBufferViewObject(&value.toObject())) {
    exception_state->SetSimpleException(
        kTypeError, "Expected object of type ArrayBufferView");
    return;
  }

  *out_array_buffer_view =
      MozjsUserObjectHolder<MozjsArrayBufferView>(context, value);
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_ARRAY_BUFFER_VIEW_H_
