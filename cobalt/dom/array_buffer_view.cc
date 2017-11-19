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

#include "cobalt/dom/array_buffer_view.h"

namespace cobalt {
namespace dom {

const char* ArrayBufferView::kWrongByteOffsetMultipleErrorFormat =
    "Byte offset should be a multiple of %d.";
const char* ArrayBufferView::kWrongByteLengthMultipleErrorFormat =
    "Byte length should be a multiple of %d.";

ArrayBufferView::ArrayBufferView(const scoped_refptr<ArrayBuffer>& buffer)
    : buffer_(buffer), byte_offset_(0), byte_length_(buffer->byte_length()) {}

ArrayBufferView::ArrayBufferView(const scoped_refptr<ArrayBuffer>& buffer,
                                 uint32 byte_offset, uint32 byte_length)
    : buffer_(buffer), byte_offset_(byte_offset), byte_length_(byte_length) {}

void* ArrayBufferView::base_address() { return buffer_->data() + byte_offset_; }

const void* ArrayBufferView::base_address() const {
  return buffer_->data() + byte_offset_;
}

void ArrayBufferView::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(buffer_);
}

ArrayBufferView::~ArrayBufferView() {}

}  // namespace dom
}  // namespace cobalt
