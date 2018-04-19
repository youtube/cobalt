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

#ifndef COBALT_SCRIPT_V8C_V8C_DATA_VIEW_H_
#define COBALT_SCRIPT_V8C_V8C_DATA_VIEW_H_

#include "base/logging.h"
#include "cobalt/script/data_view.h"
#include "cobalt/script/v8c/entry_scope.h"
#include "cobalt/script/v8c/scoped_persistent.h"
#include "cobalt/script/v8c/type_traits.h"
#include "cobalt/script/v8c/v8c_array_buffer.h"
#include "cobalt/script/v8c/v8c_user_object_holder.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

class V8cDataView final : public ScopedPersistent<v8::Value>, public DataView {
 public:
  using BaseType = DataView;

  V8cDataView(v8::Isolate* isolate, v8::Local<v8::Value> value)
      : isolate_(isolate), ScopedPersistent(isolate, value) {
    DCHECK(value->IsDataView());
  }

  Handle<ArrayBuffer> Buffer() const override {
    EntryScope entry_scope(isolate_);
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
  v8::Local<v8::DataView> GetInternal() const {
    return this->NewLocal(isolate_).As<v8::DataView>();
  }

  v8::Isolate* isolate_;
};

template <>
struct TypeTraits<DataView> {
  using ConversionType = V8cUserObjectHolder<V8cDataView>;
  using ReturnType = const ScriptValue<DataView>*;
};

inline void ToJSValue(v8::Isolate* isolate,
                      const ScriptValue<DataView>* array_buffer_view_value,
                      v8::Local<v8::Value>* out_value) {
  if (!array_buffer_view_value) {
    *out_value = v8::Null(isolate);
    return;
  }

  const auto* v8c_array_buffer_view_value =
      base::polymorphic_downcast<const V8cUserObjectHolder<V8cDataView>*>(
          array_buffer_view_value);
  *out_value = v8c_array_buffer_view_value->v8_value();
}

inline void FromJSValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, int conversion_flags,
    ExceptionState* exception_state,
    V8cUserObjectHolder<V8cDataView>* out_array_buffer_view) {
  DCHECK_EQ(0, conversion_flags);
  DCHECK(out_array_buffer_view);

  if (!value->IsObject()) {
    exception_state->SetSimpleException(kNotObjectType);
    return;
  }

  if (!value->IsDataView()) {
    exception_state->SetSimpleException(kTypeError,
                                        "Expected object of type DataView");
    return;
  }

  *out_array_buffer_view = V8cUserObjectHolder<V8cDataView>(isolate, value);
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_DATA_VIEW_H_
