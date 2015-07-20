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

#include "cobalt/dom/array_buffer.h"

#include <algorithm>

namespace cobalt {
namespace dom {

ArrayBuffer::ArrayBuffer(uint32 length) : bytes_(length) {}

scoped_refptr<ArrayBuffer> ArrayBuffer::Slice(int start, int end) {
  int clamped_start;
  int clamped_end;
  ClampRange(start, end, static_cast<int>(byte_length()), &clamped_start,
             &clamped_end);
  scoped_refptr<ArrayBuffer> slice(
      new ArrayBuffer(static_cast<uint32>(clamped_end - clamped_start)));
  slice->bytes_.insert(slice->bytes_.begin(), bytes_.begin() + clamped_start,
                       bytes_.begin() + clamped_end);
  return slice;
}

void ArrayBuffer::ClampRange(int start, int end, int source_length,
                             int* clamped_start, int* clamped_end) {
  // Clamp out of range start/end to valid indices.
  if (start > source_length) {
    start = source_length;
  }
  if (end > source_length) {
    end = source_length;
  }

  // Wrap around negative indices.
  if (start < 0) {
    start = source_length + start;
  }
  if (end < 0) {
    end = source_length + end;
  }

  // Clamp the length of the new array to non-negative.
  if (end - start < 0) {
    start = 0;
    end = 0;
  }
  *clamped_start = start;
  *clamped_end = end;
}

ArrayBuffer::~ArrayBuffer() {}

}  // namespace dom
}  // namespace cobalt
