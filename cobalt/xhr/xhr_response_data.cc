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

#include <algorithm>

#include "cobalt/dom/stats.h"

namespace cobalt {
namespace xhr {

namespace {

// When we don't have any data, we still want to return a non-null pointer to a
// valid memory location.  Because even it will never be accessed, a null
// pointer may trigger undefined behavior in functions like memcpy.  So we
// create this dummy value here and return its address when we don't have any
// data.
uint8 s_dummy;

}  // namespace

XhrResponseData::XhrResponseData() {}

XhrResponseData::~XhrResponseData() {}

void XhrResponseData::Clear() {
  dom::Stats::GetInstance()->DecreaseXHRMemoryUsage(capacity());
  // Use swap to force free the memory allocated.
  std::vector<uint8> dummy;
  data_.swap(dummy);
}

void XhrResponseData::Reserve(size_t new_capacity_bytes) {
  dom::Stats::GetInstance()->DecreaseXHRMemoryUsage(capacity());
  data_.reserve(new_capacity_bytes);
  dom::Stats::GetInstance()->IncreaseXHRMemoryUsage(capacity());
}

void XhrResponseData::Append(const uint8* source_data, size_t size_bytes) {
  dom::Stats::GetInstance()->DecreaseXHRMemoryUsage(capacity());
  data_.insert(data_.end(), source_data, source_data + size_bytes);
  dom::Stats::GetInstance()->IncreaseXHRMemoryUsage(capacity());
}

const uint8* XhrResponseData::data() const {
  return data_.empty() ? &s_dummy : &data_[0];
}

uint8* XhrResponseData::data() { return data_.empty() ? &s_dummy : &data_[0]; }

}  // namespace xhr
}  // namespace cobalt
