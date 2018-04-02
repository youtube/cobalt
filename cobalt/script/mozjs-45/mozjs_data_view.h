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

#ifndef COBALT_SCRIPT_MOZJS_45_MOZJS_DATA_VIEW_H_
#define COBALT_SCRIPT_MOZJS_45_MOZJS_DATA_VIEW_H_

#include "base/logging.h"
#include "cobalt/script/data_view.h"
#include "cobalt/script/mozjs-45/mozjs_array_buffer.h"
#include "cobalt/script/mozjs-45/mozjs_user_object_holder.h"
#include "cobalt/script/mozjs-45/type_traits.h"
#include "cobalt/script/mozjs-45/weak_heap_object.h"
#include "third_party/mozjs-45/js/src/jsapi.h"
#include "third_party/mozjs-45/js/src/jsfriendapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

class MozjsDataView final : public DataView {
 public:
  using BaseType = DataView;

  MozjsDataView(JSContext* context, JS::HandleValue value)
      : context_(context), weak_heap_object_(context, value) {
    DCHECK(value.isObject());
    DCHECK(JS_IsDataViewObject(&value.toObject()));
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
    return JS_GetDataViewByteOffset(weak_heap_object_.GetObject());
  }

  size_t ByteLength() const override {
    return JS_GetDataViewByteLength(weak_heap_object_.GetObject());
  }

  void* RawData() const override {
    JS::AutoCheckCannotGC no_gc;
    return JS_GetDataViewData(weak_heap_object_.GetObject(), no_gc);
  }

 protected:
  JSContext* context_;
  WeakHeapObject weak_heap_object_;
};

template <>
struct TypeTraits<DataView> {
  using ConversionType = MozjsUserObjectHolder<MozjsDataView>;
  using ReturnType = const ScriptValue<DataView>*;
};

inline void ToJSValue(JSContext* context,
                      const ScriptValue<DataView>* array_buffer_view_value,
                      JS::MutableHandleValue out_value) {
  TRACK_MEMORY_SCOPE("Javascript");

  if (!array_buffer_view_value) {
    out_value.set(JS::NullValue());
    return;
  }

  const auto* mozjs_array_buffer_view_value =
      base::polymorphic_downcast<const MozjsUserObjectHolder<MozjsDataView>*>(
          array_buffer_view_value);
  out_value.setObject(*mozjs_array_buffer_view_value->js_object());
}

inline void FromJSValue(
    JSContext* context, JS::HandleValue value, int conversion_flags,
    ExceptionState* exception_state,
    MozjsUserObjectHolder<MozjsDataView>* out_array_buffer_view) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK_EQ(0, conversion_flags);
  DCHECK(out_array_buffer_view);

  if (!value.isObject()) {
    exception_state->SetSimpleException(kNotObjectType);
    return;
  }

  if (!JS_IsDataViewObject(&value.toObject())) {
    exception_state->SetSimpleException(kTypeError,
                                        "Expected object of type DataView");
    return;
  }

  *out_array_buffer_view = MozjsUserObjectHolder<MozjsDataView>(context, value);
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_DATA_VIEW_H_
