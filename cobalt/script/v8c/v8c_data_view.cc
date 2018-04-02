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

#include "cobalt/script/v8c/v8c_data_view.h"

#include "cobalt/base/polymorphic_downcast.h"

namespace cobalt {
namespace script {

// static
Handle<DataView> DataView::New(GlobalEnvironment* global_environment,
                               Handle<ArrayBuffer> array_buffer,
                               size_t byte_offset, size_t byte_length) {
  auto* v8c_global_environment =
      base::polymorphic_downcast<v8c::V8cGlobalEnvironment*>(
          global_environment);
  v8::Isolate* isolate = v8c_global_environment->isolate();

  v8c::EntryScope entry_scope(isolate);

  const auto* v8c_array_buffer = base::polymorphic_downcast<
      const v8c::V8cUserObjectHolder<v8c::V8cArrayBuffer>*>(
      array_buffer.GetScriptValue());

  v8::Local<v8::ArrayBuffer> array_buffer_value =
      v8c_array_buffer->v8_value().As<v8::ArrayBuffer>();
  v8::Local<v8::DataView> data_view =
      v8::DataView::New(array_buffer_value, byte_offset, byte_length);

  return Handle<DataView>(
      new v8c::V8cUserObjectHolder<v8c::V8cDataView>(isolate, data_view));
}

}  // namespace script
}  // namespace cobalt
