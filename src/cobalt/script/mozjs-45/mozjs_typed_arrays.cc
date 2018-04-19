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

#include "cobalt/script/mozjs-45/mozjs_typed_arrays.h"

#include "cobalt/base/polymorphic_downcast.h"

namespace cobalt {
namespace script {

namespace {

template <typename T>
struct InterfaceToMozjs;

#define COBALT_SCRIPT_DEFINE_INTERFACE_TO_MOZJS(array, ctype)                 \
  template <>                                                                 \
  struct InterfaceToMozjs<array> {                                            \
    using Result = mozjs::Mozjs##array;                                       \
    static constexpr auto JS_NewArrayWithBuffer = &JS_New##array##WithBuffer; \
  };
COBALT_SCRIPT_TYPED_ARRAY_LIST(COBALT_SCRIPT_DEFINE_INTERFACE_TO_MOZJS)
#undef COBALT_SCRIPT_DEFINE_INTERFACE_TO_MOZJS

}  // namespace

// static
template <typename CType, bool IsClamped>
Handle<TypedArrayImpl<CType, IsClamped>> TypedArrayImpl<CType, IsClamped>::New(
    GlobalEnvironment* global_environment, Handle<ArrayBuffer> array_buffer,
    size_t byte_offset, size_t length) {
  using TypedArrayImplType = TypedArrayImpl<CType, IsClamped>;
  using MozjsTypedArrayImplType =
      typename InterfaceToMozjs<TypedArrayImplType>::Result;
  static constexpr auto JS_NewArrayWithBuffer =
      InterfaceToMozjs<TypedArrayImplType>::JS_NewArrayWithBuffer;

  auto* mozjs_global_environment =
      base::polymorphic_downcast<mozjs::MozjsGlobalEnvironment*>(
          global_environment);
  JSContext* context = mozjs_global_environment->context();
  JS::RootedObject global_object(context,
                                 mozjs_global_environment->global_object());
  JSAutoRequest auto_request(context);
  JSAutoCompartment auto_compartment(context, global_object);

  const auto* mozjs_array_buffer = base::polymorphic_downcast<
      const mozjs::MozjsUserObjectHolder<mozjs::MozjsArrayBuffer>*>(
      array_buffer.GetScriptValue());
  DCHECK(mozjs_array_buffer->js_value().isObject());
  JS::RootedObject rooted_array_buffer(context,
                                       mozjs_array_buffer->js_object());
  JS::RootedValue typed_array(context);
  typed_array.setObjectOrNull(
      JS_NewArrayWithBuffer(context, rooted_array_buffer, byte_offset, length));
  DCHECK(typed_array.isObject());
  return Handle<TypedArrayImplType>(
      new mozjs::MozjsUserObjectHolder<MozjsTypedArrayImplType>(context,
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
