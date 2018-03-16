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

#include "cobalt/xhr/xhr_response_data.h"

#include <algorithm>

#include "cobalt/dom/global_stats.h"

namespace cobalt {
namespace xhr {

namespace {

// When we don't have any data, we still want to return a non-null pointer to a
// valid memory location.  Because even it will never be accessed, a null
// pointer may trigger undefined behavior in functions like memcpy.  So we
// create this dummy value here and return its address when we don't have any
// data.
uint8 s_dummy;

// We are using std::string to store binary data so we want to ensure that char
// occupies one byte.
COMPILE_ASSERT(sizeof(char) == 1, char_should_occupy_one_byte);

}  // namespace

XhrResponseData::XhrResponseData() { IncreaseMemoryUsage(); }

XhrResponseData::~XhrResponseData() { DecreaseMemoryUsage(); }

void XhrResponseData::Clear() {
  DecreaseMemoryUsage();
  // Use swap to force free the memory allocated.
  std::string dummy;
  data_.swap(dummy);
  IncreaseMemoryUsage();
}

void XhrResponseData::Reserve(size_t new_capacity_bytes) {
  DecreaseMemoryUsage();
  data_.reserve(new_capacity_bytes);
  IncreaseMemoryUsage();
}

void XhrResponseData::Append(const uint8* source_data, size_t size_bytes) {
  if (size_bytes == 0) {
    return;
  }
  DecreaseMemoryUsage();
  data_.resize(data_.size() + size_bytes);
  memcpy(&data_[data_.size() - size_bytes], source_data, size_bytes);
  IncreaseMemoryUsage();
}

const uint8* XhrResponseData::data() const {
  return data_.empty() ? &s_dummy : reinterpret_cast<const uint8*>(&data_[0]);
}

uint8* XhrResponseData::data() {
  return data_.empty() ? &s_dummy : reinterpret_cast<uint8*>(&data_[0]);
}

void XhrResponseData::IncreaseMemoryUsage() {
  dom::GlobalStats::GetInstance()->IncreaseXHRMemoryUsage(capacity());
}

void XhrResponseData::DecreaseMemoryUsage() {
  dom::GlobalStats::GetInstance()->DecreaseXHRMemoryUsage(capacity());
}

}  // namespace xhr
}  // namespace cobalt
