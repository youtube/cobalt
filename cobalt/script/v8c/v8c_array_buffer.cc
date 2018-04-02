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

#include "cobalt/script/v8c/v8c_array_buffer.h"

#include "cobalt/base/polymorphic_downcast.h"

namespace cobalt {
namespace script {

PreallocatedArrayBufferData::PreallocatedArrayBufferData(size_t byte_length) {
  data_ = SbMemoryAllocate(byte_length);
  byte_length_ = byte_length;
}

PreallocatedArrayBufferData::~PreallocatedArrayBufferData() {
  if (data_) {
    SbMemoryDeallocate(data_);
    data_ = nullptr;
    byte_length_ = 0;
  }
}

// static
Handle<ArrayBuffer> ArrayBuffer::New(GlobalEnvironment* global_environment,
                                     size_t byte_length) {
  auto* v8c_global_environment =
      base::polymorphic_downcast<v8c::V8cGlobalEnvironment*>(
          global_environment);
  v8::Isolate* isolate = v8c_global_environment->isolate();
  v8c::EntryScope entry_scope(isolate);
  v8::Local<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(isolate, byte_length);
  return Handle<ArrayBuffer>(
      new v8c::V8cUserObjectHolder<v8c::V8cArrayBuffer>(isolate, array_buffer));
}

// static
Handle<ArrayBuffer> ArrayBuffer::New(GlobalEnvironment* global_environment,
                                     PreallocatedArrayBufferData* data) {
  auto* v8c_global_environment =
      base::polymorphic_downcast<v8c::V8cGlobalEnvironment*>(
          global_environment);
  v8::Isolate* isolate = v8c_global_environment->isolate();
  v8c::EntryScope entry_scope(isolate);
  v8::Local<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(isolate, data->data(), data->byte_length(),
                           v8::ArrayBufferCreationMode::kInternalized);
  data->Release();
  return Handle<ArrayBuffer>(
      new v8c::V8cUserObjectHolder<v8c::V8cArrayBuffer>(isolate, array_buffer));
}

}  // namespace script
}  // namespace cobalt
