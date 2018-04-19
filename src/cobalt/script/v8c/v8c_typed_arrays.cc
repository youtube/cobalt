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

#include "cobalt/script/v8c/v8c_typed_arrays.h"

#include "cobalt/base/polymorphic_downcast.h"

namespace cobalt {
namespace script {

namespace {

template <typename T>
struct InterfaceToV8c;

#define COBALT_SCRIPT_DEFINE_INTERFACE_TO_V8C(array, ctype) \
  template <>                                               \
  struct InterfaceToV8c<array> {                            \
    using Result = v8c::V8c##array;                         \
    using V8Type = v8::array;                               \
  };
COBALT_SCRIPT_TYPED_ARRAY_LIST(COBALT_SCRIPT_DEFINE_INTERFACE_TO_V8C)
#undef COBALT_SCRIPT_DEFINE_INTERFACE_TO_V8C

}  // namespace

// static
template <typename CType, bool IsClamped>
Handle<TypedArrayImpl<CType, IsClamped>> TypedArrayImpl<CType, IsClamped>::New(
    GlobalEnvironment* global_environment, Handle<ArrayBuffer> array_buffer,
    size_t byte_offset, size_t length) {
  using TypedArrayImplType = TypedArrayImpl<CType, IsClamped>;
  using V8cTypedArrayImplType =
      typename InterfaceToV8c<TypedArrayImplType>::Result;
  using V8Type = typename InterfaceToV8c<TypedArrayImplType>::V8Type;

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
  v8::Local<V8Type> typed_array =
      V8Type::New(array_buffer_value, byte_offset, length);
  return Handle<TypedArrayImplType>(
      new v8c::V8cUserObjectHolder<V8cTypedArrayImplType>(isolate,
                                                          typed_array));
}

#define COBALT_SCRIPT_INSTANTIATE(array, ctype)                            \
  template Handle<array> array::New(GlobalEnvironment* global_environment, \
                                    Handle<ArrayBuffer> array_buffer,      \
                                    size_t byte_offset, size_t length);
COBALT_SCRIPT_TYPED_ARRAY_LIST(COBALT_SCRIPT_INSTANTIATE)
#undef COBALT_SCRIPT_INSTANTIATE

}  // namespace script
}  // namespace cobalt
