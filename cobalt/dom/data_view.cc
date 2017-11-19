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

#include "cobalt/dom/data_view.h"

namespace cobalt {
namespace dom {

DataView::DataView(const scoped_refptr<ArrayBuffer>& buffer,
                   script::ExceptionState* exception_state)
    : buffer_(buffer),
      byte_offset_(0),
      byte_length_(buffer ? buffer->byte_length() : 0) {
  if (!buffer) {
    exception_state->SetSimpleException(script::kNotAnArrayBuffer);
  }
}

DataView::DataView(const scoped_refptr<ArrayBuffer>& buffer, uint32 byte_offset,
                   script::ExceptionState* exception_state)
    : buffer_(buffer),
      byte_offset_(byte_offset),
      byte_length_(buffer ? buffer->byte_length() - byte_offset : 0) {
  if (!buffer) {
    exception_state->SetSimpleException(script::kNotAnArrayBuffer);
  } else if (byte_offset_ > buffer_->byte_length()) {
    exception_state->SetSimpleException(script::kOutsideBounds);
  }
}

DataView::DataView(const scoped_refptr<ArrayBuffer>& buffer, uint32 byte_offset,
                   uint32 byte_length, script::ExceptionState* exception_state)
    : buffer_(buffer), byte_offset_(byte_offset), byte_length_(byte_length) {
  if (!buffer) {
    exception_state->SetSimpleException(script::kNotAnArrayBuffer);
  } else if (byte_offset_ > buffer_->byte_length()) {
    exception_state->SetSimpleException(script::kOutsideBounds);
  } else if (byte_offset_ + byte_length_ > buffer_->byte_length()) {
    exception_state->SetSimpleException(script::kInvalidLength);
  }
}

void DataView::TraceMembers(script::Tracer* tracer) { tracer->Trace(buffer_); }

}  // namespace dom
}  // namespace cobalt
