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

#ifndef COBALT_SCRIPT_TYPED_ARRAYS_H_
#define COBALT_SCRIPT_TYPED_ARRAYS_H_

#include "cobalt/script/array_buffer.h"
#include "cobalt/script/array_buffer_view.h"
#include "cobalt/script/script_value.h"

namespace cobalt {
namespace script {

class TypedArray : public ArrayBufferView {
 public:
  // http://www.ecma-international.org/ecma-262/6.0/#sec-%typedarray%-length
  virtual size_t Length() const = 0;
};

template <typename CType, bool IsClamped = false>
class TypedArrayImpl : public TypedArray {
 public:
  // http://www.ecma-international.org/ecma-262/6.0/#sec-%typedarray%-buffer-byteoffset-length
  static Handle<TypedArrayImpl> New(GlobalEnvironment* global_environment,
                                    Handle<ArrayBuffer> array_buffer,
                                    size_t byte_offset, size_t length);

  // http://www.ecma-international.org/ecma-262/6.0/#sec-%typedarray%-length
  static Handle<TypedArrayImpl> New(GlobalEnvironment* global_environment,
                                    size_t length) {
    Handle<ArrayBuffer> array_buffer =
        ArrayBuffer::New(global_environment, length * sizeof(CType));
    return New(global_environment, array_buffer, 0, length);
  }

  virtual CType* Data() const = 0;
};

using Int8Array = TypedArrayImpl<int8_t>;
using Uint8Array = TypedArrayImpl<uint8_t>;
using Uint8ClampedArray = TypedArrayImpl<uint8_t, true>;
using Int16Array = TypedArrayImpl<int16_t>;
using Uint16Array = TypedArrayImpl<uint16_t>;
using Int32Array = TypedArrayImpl<int32_t>;
using Uint32Array = TypedArrayImpl<uint32_t>;
using Float32Array = TypedArrayImpl<float>;
using Float64Array = TypedArrayImpl<double>;

#define COBALT_SCRIPT_TYPED_ARRAY_LIST(F) \
  F(Int8Array, int8_t)                    \
  F(Uint8Array, uint8_t)                  \
  F(Uint8ClampedArray, uint8_t)           \
  F(Int16Array, int16_t)                  \
  F(Uint16Array, uint16_t)                \
  F(Int32Array, int32_t)                  \
  F(Uint32Array, uint32_t)                \
  F(Float32Array, float)                  \
  F(Float64Array, double)

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_TYPED_ARRAYS_H_
