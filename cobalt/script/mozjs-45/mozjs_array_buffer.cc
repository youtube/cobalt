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

#include "cobalt/script/mozjs-45/mozjs_array_buffer.h"

#include "cobalt/base/polymorphic_downcast.h"

namespace cobalt {
namespace script {

// TODO: These can be merged into common code (using SbMemoryAllocate
// directly) once AllocationMetadata is removed.
PreallocatedArrayBufferData::PreallocatedArrayBufferData(size_t byte_length) {
  data_ = js_malloc(byte_length);
  byte_length_ = byte_length;
}

PreallocatedArrayBufferData::~PreallocatedArrayBufferData() {
  if (data_) {
    js_free(data_);
    data_ = nullptr;
    byte_length_ = 0;
  }
}

// static
Handle<ArrayBuffer> ArrayBuffer::New(GlobalEnvironment* global_environment,
                                     size_t byte_length) {
  auto* mozjs_global_environment =
      base::polymorphic_downcast<mozjs::MozjsGlobalEnvironment*>(
          global_environment);
  JSContext* context = mozjs_global_environment->context();
  JS::RootedObject global_object(context,
                                 mozjs_global_environment->global_object());
  JSAutoRequest auto_request(context);
  JSAutoCompartment auto_compartment(context, global_object);
  JS::RootedValue array_buffer(context);
  array_buffer.setObjectOrNull(JS_NewArrayBuffer(context, byte_length));
  DCHECK(array_buffer.isObject());
  return Handle<ArrayBuffer>(
      new mozjs::MozjsUserObjectHolder<mozjs::MozjsArrayBuffer>(context,
                                                                array_buffer));
}

// static
Handle<ArrayBuffer> ArrayBuffer::New(GlobalEnvironment* global_environment,
                                     PreallocatedArrayBufferData* data) {
  auto* mozjs_global_environment =
      base::polymorphic_downcast<mozjs::MozjsGlobalEnvironment*>(
          global_environment);
  JSContext* context = mozjs_global_environment->context();
  JS::RootedObject global_object(context,
                                 mozjs_global_environment->global_object());
  JSAutoRequest auto_request(context);
  JSAutoCompartment auto_compartment(context, global_object);

  JS::RootedValue array_buffer(context);
  array_buffer.setObjectOrNull(JS_NewArrayBufferWithContents(
      context, data->byte_length(), data->data()));
  DCHECK(array_buffer.isObject());
  data->Release();
  return Handle<ArrayBuffer>(
      new mozjs::MozjsUserObjectHolder<mozjs::MozjsArrayBuffer>(context,
                                                                array_buffer));
}

}  // namespace script
}  // namespace cobalt
