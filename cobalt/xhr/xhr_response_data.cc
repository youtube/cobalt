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

#include "cobalt/xhr/xhr_response_data.h"

#include "cobalt/dom/stats.h"

namespace cobalt {
namespace xhr {

XhrResponseData::XhrResponseData() : size_(0), capacity_(0) {}
XhrResponseData::~XhrResponseData() { Clear(); }

void XhrResponseData::Clear() {
  dom::Stats::GetInstance()->DecreaseXHRMemoryUsage(capacity_);
  data_.reset(NULL);
  size_ = 0;
  capacity_ = 0;
}

void XhrResponseData::Reserve(size_t new_capacity) {
  if (capacity_ < new_capacity) {
    dom::Stats::GetInstance()->IncreaseXHRMemoryUsage(new_capacity - capacity_);
    // Ideally, we could realloc here instead of having two buffers alive at
    // once, but using a scoped_array makes that more difficult.
    uint8* new_data = new uint8[new_capacity];
    memcpy(new_data, data_.get(), size_);
    data_.reset(new_data);
    capacity_ = new_capacity;
  }
}

void XhrResponseData::Append(const uint8* source_data, size_t size_bytes) {
  Reserve(size_ + size_bytes);
  memcpy(data() + size_, source_data, size_bytes);
  size_ += size_bytes;
}

scoped_array<uint8> XhrResponseData::Pass() {
  dom::Stats::GetInstance()->DecreaseXHRMemoryUsage(capacity_);
  size_ = 0;
  capacity_ = 0;
  return data_.Pass();
}
}  // namespace xhr
}  // namespace cobalt
