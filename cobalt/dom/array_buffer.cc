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

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/stats.h"
#include "cobalt/script/javascript_engine.h"

namespace cobalt {
namespace dom {

ArrayBuffer::ArrayBuffer(script::EnvironmentSettings* settings, uint32 length)
    : data_(new uint8[length]), size_(length) {
  memset(data_.get(), 0, size_);
  // TODO(***REMOVED***): Once we can have a reliable way to pass the
  // EnvironmentSettings to HTMLMediaElement, we should make EnvironmentSettings
  // mandatory for creating ArrayBuffer in non-testing code.
  if (settings) {
    DOMSettings* dom_settings =
        base::polymorphic_downcast<dom::DOMSettings*>(settings);
    dom_settings->javascript_engine()->ReportExtraMemoryCost(size_);
  }
  Stats::GetInstance()->IncreaseArrayBufferMemoryUsage(size_);
}

ArrayBuffer::ArrayBuffer(script::EnvironmentSettings* settings,
                         scoped_array<uint8> data, uint32 length)
    : data_(data.Pass()), size_(length) {
  // TODO(***REMOVED***): Make EnvironmentSettings mandatory for creating
  // ArrayBuffer in non-testing code.
  if (settings) {
    DOMSettings* dom_settings =
        base::polymorphic_downcast<dom::DOMSettings*>(settings);
    dom_settings->javascript_engine()->ReportExtraMemoryCost(size_);
  }
  Stats::GetInstance()->IncreaseArrayBufferMemoryUsage(size_);
}

scoped_refptr<ArrayBuffer> ArrayBuffer::Slice(
    script::EnvironmentSettings* settings, int start, int end) {
  int clamped_start;
  int clamped_end;
  ClampRange(start, end, static_cast<int>(byte_length()), &clamped_start,
             &clamped_end);
  DCHECK_GE(clamped_end, clamped_start);
  size_t slice_size = static_cast<size_t>(clamped_end - clamped_start);
  scoped_array<uint8> slice(new uint8[slice_size]);
  memcpy(slice.get(), data() + clamped_start, slice_size);
  return new ArrayBuffer(settings, slice.Pass(),
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

ArrayBuffer::~ArrayBuffer() {
  Stats::GetInstance()->DecreaseArrayBufferMemoryUsage(size_);
}

}  // namespace dom
}  // namespace cobalt
