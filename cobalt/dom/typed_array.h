// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_TYPED_ARRAY_H_
#define COBALT_DOM_TYPED_ARRAY_H_

#include "base/logging.h"
#include "base/stringprintf.h"
#include "cobalt/dom/array_buffer_view.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/exception_state.h"

#if defined(STARBOARD)
#include "starboard/memory.h"
#endif

namespace cobalt {
namespace dom {

// TypedArray serves as the base of all array interfaces defined in
// https://www.khronos.org/registry/typedarray/specs/latest/#7
// An array interfaces should inherit from TypedArray<ElementType>, for
// example, Uint8Array should inherit from TypedArray<uint8>.  A macro
// named DEFINE_TYPED_ARRAY is provided at the end of this file for
// convenience.
template <typename ElementType>
class TypedArray : public ArrayBufferView {
 public:
  enum { kBytesPerElement = sizeof(ElementType) };

  // Create a new TypedArray of the specified length.  Each element is
  // initialized to 0 inside the ctor of ArrayBuffer.
  TypedArray(script::EnvironmentSettings* settings, uint32 length)
      : ArrayBufferView(new ArrayBuffer(settings, length * kBytesPerElement)) {}

  // Create a new TypedArray of the specified length and initialize it with the
  // given data.
  TypedArray(script::EnvironmentSettings* settings, const ElementType* data,
             uint32 length)
      : ArrayBufferView(new ArrayBuffer(settings, length * kBytesPerElement)) {
    DCHECK_EQ(this->length(), length);
    SbMemoryCopy(this->data(), data, length * kBytesPerElement);
  }

  // Creates a new TypedArray and copies the elements of 'other' into this.
  TypedArray(script::EnvironmentSettings* settings,
             const scoped_refptr<TypedArray>& other)
      : ArrayBufferView(other->buffer()->Slice(settings, 0)) {}

  // TODO: Support constructors from Array types.
  // i.e. uint8[], float[], etc.

  // Create a view on top of the specified buffer.
  // Offset starts at byte_offset, length is byte_length.
  // This refers to the same underlying data as buffer.
  TypedArray(const scoped_refptr<ArrayBuffer>& buffer,
             script::ExceptionState* exception_state)
      : ArrayBufferView(buffer) {
    if (buffer->byte_length() % kBytesPerElement != 0) {
      exception_state->SetSimpleException(script::kRangeError,
                                          kWrongByteLengthMultipleErrorFormat,
                                          kBytesPerElement);
    }
  }

  TypedArray(const scoped_refptr<ArrayBuffer>& buffer, uint32 byte_offset,
             script::ExceptionState* exception_state)
      : ArrayBufferView(buffer, byte_offset,
                        buffer->byte_length() - byte_offset) {
    if (this->byte_offset() % kBytesPerElement != 0) {
      exception_state->SetSimpleException(script::kRangeError,
                                          kWrongByteOffsetMultipleErrorFormat,
                                          kBytesPerElement);
    } else if (buffer->byte_length() % kBytesPerElement != 0) {
      exception_state->SetSimpleException(script::kRangeError,
                                          kWrongByteLengthMultipleErrorFormat,
                                          kBytesPerElement);
    }
  }

  TypedArray(const scoped_refptr<ArrayBuffer>& buffer, uint32 byte_offset,
             uint32 length, script::ExceptionState* exception_state)
      : ArrayBufferView(buffer, byte_offset, length * kBytesPerElement) {
    if (this->byte_offset() % kBytesPerElement != 0) {
      exception_state->SetSimpleException(script::kRangeError,
                                          kWrongByteOffsetMultipleErrorFormat,
                                          kBytesPerElement);
    } else if (byte_offset + length * kBytesPerElement >
               buffer->byte_length()) {
      exception_state->SetSimpleException(script::kInvalidLength);
    }
  }

  // Create a new TypedArray that is a view into this existing array.
  template <typename SubarrayType>
  scoped_refptr<SubarrayType> SubarrayImpl(
      script::EnvironmentSettings* settings, int start, int end) {
    const int cur_length = static_cast<int>(length());
    int clamped_start;
    int clamped_end;
    ArrayBuffer::ClampRange(start, end, cur_length, &clamped_start,
                            &clamped_end);
    return new SubarrayType(
        settings, buffer(),
        static_cast<uint32>(byte_offset() + clamped_start * kBytesPerElement),
        static_cast<uint32>(clamped_end - clamped_start), NULL);
  }

  // Copy items from 'source' into this array. This array must already be at
  // least as large as 'source'.
  void Set(const scoped_refptr<TypedArray>& source, uint32 offset,
           script::ExceptionState* exception_state) {
    if (offset >= length() || length() - offset < source->length()) {
      exception_state->SetSimpleException(script::kInvalidLength);
      return;
    }
    uint32 source_offset = 0;
    while (source_offset < source->length()) {
      SbMemoryCopy(data() + offset, source->data() + source_offset,
             sizeof(ElementType));
      ++offset;
      ++source_offset;
    }
  }

  void Set(const scoped_refptr<TypedArray>& source,
           script::ExceptionState* exception_state) {
    Set(source, 0, exception_state);
  }

  // Write a single element of the array.
  void Set(uint32 index, ElementType val) {
    if (index < length()) {
      SbMemoryCopy(data() + index, &val, sizeof(ElementType));
    }
  }

  ElementType Get(uint32 index) const {
    if (index < length()) {
      ElementType val;
      SbMemoryCopy(&val, data() + index, sizeof(ElementType));
      return val;
    } else {
      // TODO: an out of bounds index should return undefined.
      DLOG(ERROR) << "index " << index << " out of range " << length();
      return 0;
    }
  }

  uint32 length() const { return byte_length() / kBytesPerElement; }

  const ElementType* data() const {
    return reinterpret_cast<const ElementType*>(base_address());
  }
  ElementType* data() { return reinterpret_cast<ElementType*>(base_address()); }

  DEFINE_WRAPPABLE_TYPE(TypedArray);

 private:
  DISALLOW_COPY_AND_ASSIGN(TypedArray);
};

}  // namespace dom
}  // namespace cobalt

// This macro is necessary as every SubarrayType should define its own wrappable
// type.
// Example usage:
//   DEFINE_TYPED_ARRAY(Uint8Array, uint8);
//   DEFINE_TYPED_ARRAY(Float32Array, float);
#define DEFINE_TYPED_ARRAY(SubarrayType, ElementType)                          \
  class SubarrayType : public TypedArray<ElementType> {                        \
   public:                                                                     \
    SubarrayType(script::EnvironmentSettings* settings, uint32 length,         \
                 script::ExceptionState* exception_state)                      \
        : TypedArray<ElementType>(settings, length) {                          \
      UNREFERENCED_PARAMETER(exception_state);                                 \
    }                                                                          \
    SubarrayType(script::EnvironmentSettings* settings,                        \
                 const ElementType* data, uint32 length,                       \
                 script::ExceptionState* exception_state)                      \
        : TypedArray<ElementType>(settings, data, length) {                    \
      UNREFERENCED_PARAMETER(exception_state);                                 \
    }                                                                          \
    SubarrayType(script::EnvironmentSettings* settings,                        \
                 const scoped_refptr<SubarrayType>& other,                     \
                 script::ExceptionState* exception_state)                      \
        : TypedArray<ElementType>(settings, other.get()) {                     \
      UNREFERENCED_PARAMETER(exception_state);                                 \
    }                                                                          \
    SubarrayType(script::EnvironmentSettings* settings,                        \
                 const scoped_refptr<ArrayBuffer>& buffer,                     \
                 script::ExceptionState* exception_state)                      \
        : TypedArray<ElementType>(buffer.get(), exception_state) {             \
      UNREFERENCED_PARAMETER(settings);                                        \
    }                                                                          \
    SubarrayType(script::EnvironmentSettings* settings,                        \
                 const scoped_refptr<ArrayBuffer>& buffer, uint32 byte_offset, \
                 script::ExceptionState* exception_state)                      \
        : TypedArray<ElementType>(buffer, byte_offset, exception_state) {      \
      UNREFERENCED_PARAMETER(settings);                                        \
    }                                                                          \
    SubarrayType(script::EnvironmentSettings* settings,                        \
                 const scoped_refptr<ArrayBuffer>& buffer, uint32 byte_offset, \
                 uint32 byte_length, script::ExceptionState* exception_state)  \
        : TypedArray<ElementType>(buffer, byte_offset, byte_length,            \
                                  exception_state) {                           \
      UNREFERENCED_PARAMETER(settings);                                        \
    }                                                                          \
                                                                               \
    scoped_refptr<SubarrayType> Subarray(                                      \
        script::EnvironmentSettings* settings, int start, int end) {           \
      return SubarrayImpl<SubarrayType>(settings, start, end);                 \
    }                                                                          \
    scoped_refptr<SubarrayType> Subarray(                                      \
        script::EnvironmentSettings* settings, int start) {                    \
      return SubarrayImpl<SubarrayType>(settings, start,                       \
                                        static_cast<int>(length()));           \
    }                                                                          \
                                                                               \
    DEFINE_WRAPPABLE_TYPE(SubarrayType);                                       \
                                                                               \
   private:                                                                    \
    DISALLOW_COPY_AND_ASSIGN(SubarrayType);                                    \
  };

#endif  // COBALT_DOM_TYPED_ARRAY_H_
