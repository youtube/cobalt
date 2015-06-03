/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DOM_UINT8_ARRAY_H_
#define DOM_UINT8_ARRAY_H_

#include <string>

#include "cobalt/dom/array_buffer_view.h"

namespace cobalt {
namespace dom {

class Uint8Array : public ArrayBufferView {
 public:
  enum { kBytesPerElement = 1 };

  // Create a new Uint8Array of the specified length.
  // Each element is initialized to 0.
  explicit Uint8Array(uint32 length);

  // Creates a new Uint8Array and copies the elements of 'other' into this.
  explicit Uint8Array(const scoped_refptr<Uint8Array>& other);

  // TODO(***REMOVED***): Support a constructor from an Array type.
  // i.e. uint8[]

  // Create a view on top of the specified buffer.
  // Offset starts at byte_offset, length is byte_length.
  // This refers to the same underlying data as buffer.
  explicit Uint8Array(const scoped_refptr<ArrayBuffer>& buffer);
  Uint8Array(const scoped_refptr<ArrayBuffer>& buffer, uint32 byte_offset);
  Uint8Array(const scoped_refptr<ArrayBuffer>& buffer, uint32 byte_offset,
             uint32 byte_length);

  // Create a new uint8array that is a view into this existing array.
  scoped_refptr<Uint8Array> Subarray(int start, int end);

  // An unspecified end defaults to array length.
  scoped_refptr<Uint8Array> Subarray(int start) {
    return Subarray(start, length());
  }

  // Copy items from 'other' into this array. This array must already
  // be at least as large as other.
  void Set(const scoped_refptr<Uint8Array>& other, uint32 offset);
  void Set(const scoped_refptr<Uint8Array>& other) { Set(other, 0); }

  // Write a single element of the array.
  void Set(uint32 index, uint8 val) {
    if (index < length()) {
      data()[index] = val;
    }
  }

  uint8 Get(uint32 index) const {
    // TODO(***REMOVED***): an out of bounds index should return undefined.
    if (index < length()) {
      return data()[index];
    } else {
      DLOG(ERROR) << "index " << index << " out of range " << length();
      return 0;
    }
  }
  uint32 length() const { return byte_length() / kBytesPerElement; }

  DEFINE_WRAPPABLE_TYPE(Uint8Array);

 private:
  uint8* data() const { return reinterpret_cast<uint8*>(base_address()); }

  ~Uint8Array();

  DISALLOW_COPY_AND_ASSIGN(Uint8Array);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_UINT8_ARRAY_H_
