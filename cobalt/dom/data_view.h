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

#ifndef COBALT_DOM_DATA_VIEW_H_
#define COBALT_DOM_DATA_VIEW_H_

#include <algorithm>
#include <iterator>

#include "build/build_config.h"
#include "cobalt/dom/array_buffer.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// DataView is used to access the underlying ArrayBuffer with support of common
// types and endianness.
//   https://www.khronos.org/registry/typedarray/specs/latest/#8
class DataView : public script::Wrappable {
 public:
  // Web API: DataView
  //
  DataView(const scoped_refptr<ArrayBuffer>& buffer,
           script::ExceptionState* exception_state);
  DataView(const scoped_refptr<ArrayBuffer>& buffer, uint32 byte_offset,
           script::ExceptionState* exception_state);
  DataView(const scoped_refptr<ArrayBuffer>& buffer, uint32 byte_offset,
           uint32 byte_length, script::ExceptionState* exception_state);

// C++ macro to generate accessor for each supported types.
#define DATA_VIEW_ACCESSOR_FOR_EACH(MacroOp)                                \
  MacroOp(Int8, int8) MacroOp(Uint8, uint8) MacroOp(Int16, int16)           \
      MacroOp(Uint16, uint16) MacroOp(Int32, int32) MacroOp(Uint32, uint32) \
      MacroOp(Float32, float) MacroOp(Float64, double)

// The following macro generates the accessors for each types listed above.
// According to the spec, For getter without an endianness parameter it is
// default to big endian.  Note that it may generate extra accessors for
// int8/uint8 which has an 'little_endian' parameter, this is redundant but
// should work as int8/uint8 behalf the same when accessed in any endian mode.
#define DEFINE_DATA_VIEW_ACCESSOR(DomType, CppType)                          \
  CppType Get##DomType(uint32 byte_offset,                                   \
                       script::ExceptionState* exception_state) const {      \
    return Get##DomType(byte_offset, false, exception_state);                \
  }                                                                          \
  CppType Get##DomType(uint32 byte_offset, bool little_endian,               \
                       script::ExceptionState* exception_state) const {      \
    return GetElement<CppType>(byte_offset, little_endian, exception_state); \
  }                                                                          \
  void Set##DomType(uint32 byte_offset, CppType value,                       \
                    script::ExceptionState* exception_state) {               \
    Set##DomType(byte_offset, value, false, exception_state);                \
  }                                                                          \
  void Set##DomType(uint32 byte_offset, CppType value, bool little_endian,   \
                    script::ExceptionState* exception_state) {               \
    SetElement<CppType>(byte_offset, value, little_endian, exception_state); \
  }

  DATA_VIEW_ACCESSOR_FOR_EACH(DEFINE_DATA_VIEW_ACCESSOR)
#undef DEFINE_DATA_VIEW_ACCESSOR

  // Web API: ArrayBufferView (implements)
  //
  const scoped_refptr<ArrayBuffer>& buffer() { return buffer_; }
  uint32 byte_offset() const { return byte_offset_; }
  uint32 byte_length() const { return byte_length_; }

  DEFINE_WRAPPABLE_TYPE(DataView);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  static void CopyBytes(const uint8* src, size_t size, bool little_endian,
                        uint8* dest) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
    bool need_reverse = !little_endian;
#else   // defined(ARCH_CPU_LITTLE_ENDIAN)
    bool need_reverse = little_endian;
#endif  // defined(ARCH_CPU_LITTLE_ENDIAN)
    if (need_reverse) {
#if defined(COMPILER_MSVC)
      std::reverse_copy(src, src + size,
                        stdext::checked_array_iterator<uint8*>(dest, size));
#else  // defined(COMPILER_MSVC)
      std::reverse_copy(src, src + size, dest);
#endif  // defined(COMPILER_MSVC)
    } else {
      memcpy(dest, src, size);
    }
  }

  template <typename ElementType>
  ElementType GetElement(uint32 byte_offset, bool little_endian,
                         script::ExceptionState* exception_state) const {
    if (byte_offset + sizeof(ElementType) > byte_length_) {
      exception_state->SetSimpleException(script::kOutsideBounds);
      // The return value will be ignored.
      return ElementType();
    }
    ElementType value;
    CopyBytes(buffer_->data() + byte_offset_ + byte_offset, sizeof(value),
              little_endian, reinterpret_cast<uint8*>(&value));
    return value;
  }

  template <typename ElementType>
  void SetElement(uint32 byte_offset, ElementType value, bool little_endian,
                  script::ExceptionState* exception_state) {
    if (byte_offset + sizeof(ElementType) > byte_length_) {
      exception_state->SetSimpleException(script::kOutsideBounds);
      return;
    }
    CopyBytes(reinterpret_cast<uint8*>(&value), sizeof(value), little_endian,
              buffer_->data() + byte_offset_ + byte_offset);
  }

  const scoped_refptr<ArrayBuffer> buffer_;
  const uint32 byte_offset_;
  const uint32 byte_length_;

  DISALLOW_COPY_AND_ASSIGN(DataView);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DATA_VIEW_H_
