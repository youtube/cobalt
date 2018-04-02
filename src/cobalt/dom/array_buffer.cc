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

#include "cobalt/dom/array_buffer.h"

#include <algorithm>

#include "base/logging.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/script/javascript_engine.h"
#include "nb/memory_scope.h"

namespace cobalt {
namespace dom {

namespace {

int ClampIndex(int index, int length) {
  if (index < 0) {
    index = length + index;
  }
  index = std::max(index, 0);
  return std::min(index, length);
}

}  // namespace

ArrayBuffer::Data::Data(size_t size) : data_(NULL), size_(0) {
  Initialize(size);
  if (data_) {
    memset(data_, 0, size);
  }
}

ArrayBuffer::Data::Data(const uint8* data, size_t size)
    : data_(NULL), size_(0) {
  Initialize(size);
  DCHECK(data_);
  memcpy(data_, data, size);
}

ArrayBuffer::Data::Data(scoped_array<uint8> data, size_t size)
    : data_(data.release()), size_(size) {
  DCHECK(data_);
}

ArrayBuffer::Data::~Data() { delete[] data_; }

uint8* ArrayBuffer::Data::data() const {
  return data_;
}

void ArrayBuffer::Data::Initialize(size_t size) {
  TRACK_MEMORY_SCOPE("DOM");
  data_ = new uint8[size];
  size_ = size;
}

ArrayBuffer::ArrayBuffer(script::EnvironmentSettings* settings, uint32 length)
    : data_(length) {
  // TODO: Once we can have a reliable way to pass the
  // EnvironmentSettings to HTMLMediaElement, we should make EnvironmentSettings
  // mandatory for creating ArrayBuffer in non-testing code.
  if (settings) {
    DOMSettings* dom_settings =
        base::polymorphic_downcast<dom::DOMSettings*>(settings);
    javascript_engine_ = dom_settings->javascript_engine();
    javascript_engine_->AdjustAmountOfExternalAllocatedMemory(
        static_cast<int64_t>(data_.size()));
  }
}

ArrayBuffer::ArrayBuffer(script::EnvironmentSettings* settings,
                         const uint8* data, uint32 length)
    : data_(data, length) {
  // TODO: Make EnvironmentSettings mandatory for creating
  // ArrayBuffer in non-testing code.
  if (settings) {
    DOMSettings* dom_settings =
        base::polymorphic_downcast<dom::DOMSettings*>(settings);
    javascript_engine_ = dom_settings->javascript_engine();
    javascript_engine_->AdjustAmountOfExternalAllocatedMemory(
        static_cast<int64_t>(data_.size()));
  }
}

ArrayBuffer::ArrayBuffer(script::EnvironmentSettings* settings,
                         scoped_array<uint8> data, uint32 length)
    : data_(data.Pass(), length) {
  // TODO: Make EnvironmentSettings mandatory for creating
  // ArrayBuffer in non-testing code.
  if (settings) {
    DOMSettings* dom_settings =
        base::polymorphic_downcast<dom::DOMSettings*>(settings);
    javascript_engine_ = dom_settings->javascript_engine();
    javascript_engine_->AdjustAmountOfExternalAllocatedMemory(
        static_cast<int64_t>(data_.size()));
  }
}

scoped_refptr<ArrayBuffer> ArrayBuffer::Slice(
    script::EnvironmentSettings* settings, int start, int end) const {
  TRACK_MEMORY_SCOPE("DOM");
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
  start = ClampIndex(start, source_length);
  end = ClampIndex(end, source_length);

  // Clamp the length of the new array to non-negative.
  if (end - start < 0) {
    end = start;
  }
  *clamped_start = start;
  *clamped_end = end;
}

ArrayBuffer::~ArrayBuffer() {
  if (javascript_engine_) {
    javascript_engine_->AdjustAmountOfExternalAllocatedMemory(
        -static_cast<int64_t>(data_.size()));
  }
}

}  // namespace dom
}  // namespace cobalt
