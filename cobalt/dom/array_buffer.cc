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

#include "base/logging.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/script/javascript_engine.h"

namespace cobalt {
namespace dom {

ArrayBuffer::Data::Data(script::EnvironmentSettings* settings, size_t size)
    : allocator_(NULL), data_(NULL), size_(0) {
  Initialize(settings, size);
  if (data_) {
    memset(data_, 0, size);
  }
}

ArrayBuffer::Data::Data(script::EnvironmentSettings* settings,
                        const uint8* data, size_t size)
    : allocator_(NULL), data_(NULL), size_(0) {
  Initialize(settings, size);
  if (data_) {
    memcpy(data_, data, size);
  }
}

ArrayBuffer::Data::Data(scoped_array<uint8> data, size_t size)
    : allocator_(NULL), data_(data.release()), size_(size) {
  DCHECK(data_);
}

ArrayBuffer::Data::~Data() {
  if (allocator_) {
    allocator_->Free(data_);
  } else {
    delete[] data_;
  }
}

void ArrayBuffer::Data::Initialize(script::EnvironmentSettings* settings,
                                   size_t size) {
  if (settings) {
    DOMSettings* dom_settings =
        base::polymorphic_downcast<dom::DOMSettings*>(settings);
    allocator_ = dom_settings->array_buffer_allocator();
  }
  if (allocator_) {
    data_ = reinterpret_cast<uint8*>(allocator_->Allocate(size));
  }
  if (data_ == NULL) {
    allocator_ = NULL;
    data_ = new uint8[size];
  }
  size_ = size;
  DCHECK(data_);
}

ArrayBuffer::ArrayBuffer(script::EnvironmentSettings* settings, uint32 length)
    : data_(settings, length) {
  // TODO(***REMOVED***): Once we can have a reliable way to pass the
  // EnvironmentSettings to HTMLMediaElement, we should make EnvironmentSettings
  // mandatory for creating ArrayBuffer in non-testing code.
  if (settings) {
    DOMSettings* dom_settings =
        base::polymorphic_downcast<dom::DOMSettings*>(settings);
    dom_settings->javascript_engine()->ReportExtraMemoryCost(data_.size());
  }
}

ArrayBuffer::ArrayBuffer(script::EnvironmentSettings* settings,
                         const uint8* data, uint32 length)
    : data_(settings, data, length) {
  // TODO(***REMOVED***): Make EnvironmentSettings mandatory for creating
  // ArrayBuffer in non-testing code.
  if (settings) {
    DOMSettings* dom_settings =
        base::polymorphic_downcast<dom::DOMSettings*>(settings);
    dom_settings->javascript_engine()->ReportExtraMemoryCost(data_.size());
  }
}

ArrayBuffer::ArrayBuffer(script::EnvironmentSettings* settings,
                         AllocationType allocation_type,
                         scoped_array<uint8> data, uint32 length)
    : data_(data.Pass(), length) {
  DCHECK_EQ(allocation_type, kFromHeap);
  // TODO(***REMOVED***): Make EnvironmentSettings mandatory for creating
  // ArrayBuffer in non-testing code.
  if (settings) {
    DOMSettings* dom_settings =
        base::polymorphic_downcast<dom::DOMSettings*>(settings);
    dom_settings->javascript_engine()->ReportExtraMemoryCost(data_.size());
  }
}

scoped_refptr<ArrayBuffer> ArrayBuffer::Slice(
    script::EnvironmentSettings* settings, int start, int end) const {
  int clamped_start;
  int clamped_end;
  ClampRange(start, end, static_cast<int>(byte_length()), &clamped_start,
             &clamped_end);
  DCHECK_GE(clamped_end, clamped_start);
  size_t slice_size = static_cast<size_t>(clamped_end - clamped_start);
  return new ArrayBuffer(settings, data() + clamped_start,
                         static_cast<uint32>(slice_size));
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
