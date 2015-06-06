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

#ifndef DOM_ARRAY_BUFFER_H_
#define DOM_ARRAY_BUFFER_H_

#include <string>
#include <vector>

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class ArrayBuffer : public script::Wrappable {
 public:
  explicit ArrayBuffer(uint32 length);

  uint32 byte_length() const { return static_cast<uint32>(bytes_.size()); }
  scoped_refptr<ArrayBuffer> Slice(int begin) {
    return Slice(begin, static_cast<int>(byte_length()));
  }
  scoped_refptr<ArrayBuffer> Slice(int begin, int end);

  std::vector<uint8>& bytes() { return bytes_; }
  const std::vector<uint8>& bytes() const { return bytes_; }

  // Utility function for restricting begin/end offsets to an appropriate
  // range as defined by the spec. Negative start or end values refer
  // to offsets from the end of the array rather than the beginning.
  // If the length of the resulting range would be < 0, the length
  // is clamped to 0.
  static void ClampRange(int start, int end, int source_length,
                         int* clamped_start, int* clamped_end);

  DEFINE_WRAPPABLE_TYPE(ArrayBuffer);

 private:
  ~ArrayBuffer();

  std::vector<uint8> bytes_;

  DISALLOW_COPY_AND_ASSIGN(ArrayBuffer);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_ARRAY_BUFFER_H_
