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

#ifndef COBALT_SCRIPT_ARRAY_BUFFER_H_
#define COBALT_SCRIPT_ARRAY_BUFFER_H_

#include "base/memory/ref_counted.h"
#include "cobalt/script/exception_message.h"
#include "cobalt/script/script_exception.h"
#include "cobalt/script/script_value.h"

namespace cobalt {
namespace script {

class GlobalEnvironment;
class PreallocatedArrayBufferData;

// http://www.ecma-international.org/ecma-262/6.0/#sec-arraybuffer-objects
class ArrayBuffer {
 public:
  // http://www.ecma-international.org/ecma-262/6.0/#sec-arraybuffer-length
  static Handle<ArrayBuffer> New(GlobalEnvironment* global_environment,
                                 size_t byte_length);

  // A convenience constructor that will allocate a new |ArrayBuffer| of size
  // |byte_length|, and then copy |data| (which must be an allocation of size
  // |byte_length|) into the array buffer.
  static Handle<ArrayBuffer> New(GlobalEnvironment* global_environment,
                                 void* data, size_t byte_length) {
    Handle<ArrayBuffer> array_buffer = New(global_environment, byte_length);
    SbMemoryCopy(array_buffer->Data(), data, byte_length);
    return array_buffer;
  }

  // Create a new |ArrayBuffer| from existing block of memory.  See
  // |PreallocatedArrayBufferData| for details.
  static Handle<ArrayBuffer> New(GlobalEnvironment* global_environment,
                                 PreallocatedArrayBufferData* data);

  virtual ~ArrayBuffer() {}

  // http://www.ecma-international.org/ecma-262/6.0/#sec-get-arraybuffer.prototype.bytelength
  virtual size_t ByteLength() const = 0;

  // Access the raw underlying data held by the |ArrayBuffer|.
  virtual void* Data() const = 0;
};

// A scoped RAII wrapper to facilitate creating |ArrayBuffer|s from
// preallocated data (via moving the data into the an array buffer, NOT
// copying it).
//
// Usage looks something like:
//
//   // Allocate the data by constructing a |PreallocatedArrayBufferData|
//   PreallocatedArrayBufferData data(16);
//
//   // Manipulate the data however you want.
//   static_cast<uint8_t*>(data.data())[0] = 0xFF;
//
//   // Create a new |ArrayBuffer| using |data|.
//   auto array_buffer = ArrayBuffer::New(env, &data);
//
//   // |PreallocatedData| now no longer holds anything, data should now be
//   // accessed from the ArrayBuffer itself.
//   DCHECK_EQ(data.data(), nullptr);
class PreallocatedArrayBufferData {
 public:
  explicit PreallocatedArrayBufferData(size_t byte_length);
  ~PreallocatedArrayBufferData();

  PreallocatedArrayBufferData(PreallocatedArrayBufferData&& other) = default;
  PreallocatedArrayBufferData& operator=(PreallocatedArrayBufferData&& other) =
      default;

  void* data() const { return data_; }
  size_t byte_length() const { return byte_length_; }

 private:
  PreallocatedArrayBufferData(const PreallocatedArrayBufferData&) = delete;
  void operator=(const PreallocatedArrayBufferData&) = delete;

  void Release() {
    data_ = nullptr;
    byte_length_ = 0u;
  }

  void* data_;
  size_t byte_length_;

  friend ArrayBuffer;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_ARRAY_BUFFER_H_
