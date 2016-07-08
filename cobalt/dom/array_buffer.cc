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
    : allocator_(NULL), cache_(NULL), offloaded_(false), data_(NULL), size_(0) {
  Initialize(settings, size);
  if (data_) {
    memset(data_, 0, size);
  }
  if (cache_) {
    cache_->Register(this);
  }
}

ArrayBuffer::Data::Data(script::EnvironmentSettings* settings,
                        const uint8* data, size_t size)
    : allocator_(NULL), cache_(NULL), offloaded_(false), data_(NULL), size_(0) {
  Initialize(settings, size);
  DCHECK(data_);
  memcpy(data_, data, size);
  // Register() has to be called after copying the data as Register() will call
  // TryToOffload() which may delete the |data_| passed in that belongs to
  // another ArrayBuffer.
  if (cache_) {
    cache_->Register(this);
  }
}

ArrayBuffer::Data::Data(scoped_array<uint8> data, size_t size)
    : allocator_(NULL),
      cache_(NULL),
      offloaded_(false),
      data_(data.release()),
      size_(size) {
  DCHECK(data_);
}

ArrayBuffer::Data::~Data() {
  if (offloaded_) {
    allocator_->Free(data_);
  } else {
    delete[] data_;
  }
  if (cache_) {
    cache_->Unregister(this);
  }
}

uint8* ArrayBuffer::Data::data() const {
  if (cache_) {
    cache_->ReportUsage(this);
  }
  return data_;
}

bool ArrayBuffer::Data::Offload() {
  if (offloaded_) {
    return true;
  }
  if (!allocator_) {
    return false;
  }
  uint8* data = reinterpret_cast<uint8*>(allocator_->Allocate(size()));
  if (data) {
    memcpy(data, data_, size());
    delete[] data_;
    data_ = data;
    offloaded_ = true;
  }
  return offloaded_;
}

void ArrayBuffer::Data::Initialize(script::EnvironmentSettings* settings,
                                   size_t size) {
  if (settings) {
    DOMSettings* dom_settings =
        base::polymorphic_downcast<dom::DOMSettings*>(settings);
    allocator_ = dom_settings->array_buffer_allocator();
    cache_ = dom_settings->array_buffer_cache();
    if (allocator_) {
      DCHECK(cache_);
    } else {
      DCHECK(!cache_);
    }
  }
  data_ = new uint8[size];
  size_ = size;
}

ArrayBuffer::Cache::Cache(size_t maximum_size_in_main_memory)
    : total_size_in_main_memory_(0),
      maximum_size_in_main_memory_(maximum_size_in_main_memory) {}

void ArrayBuffer::Cache::Register(Data* data) {
  total_size_in_main_memory_ += data->size();
  // Offload before push_back to ensure that the last one is always not
  // offloaded immediately.
  TryToOffload();
  datas_.push_back(data);
}

void ArrayBuffer::Cache::Unregister(Data* data) {
  DCHECK(std::find(datas_.begin(), datas_.end(), data) != datas_.end());
  datas_.erase(std::find(datas_.begin(), datas_.end(), data));
  if (!data->offloaded()) {
    DCHECK_GE(total_size_in_main_memory_, data->size());
    total_size_in_main_memory_ -= data->size();
  }
}

void ArrayBuffer::Cache::ReportUsage(const Data* data) {
  DCHECK(data);
  DCHECK(std::find(datas_.begin(), datas_.end(), data) != datas_.end());
  if (data->offloaded() || datas_.back() == data) {
    return;
  }
  // Move |data| to the end.
  datas_.erase(std::find(datas_.begin(), datas_.end(), data));
  datas_.push_back(const_cast<Data*>(data));
}

void ArrayBuffer::Cache::TryToOffload() {
  if (total_size_in_main_memory_ <= maximum_size_in_main_memory_) {
    return;
  }
  std::vector<Data*>::iterator iter = datas_.begin();
  while (iter != datas_.end() &&
         total_size_in_main_memory_ > maximum_size_in_main_memory_) {
    if (!(*iter)->offloaded() && (*iter)->Offload()) {
      total_size_in_main_memory_ -= (*iter)->size();
    }
    ++iter;
  }
  if (total_size_in_main_memory_ > maximum_size_in_main_memory_) {
    LOG(WARNING) << "ArrayBuffer takes " << total_size_in_main_memory_
                 << " of main memory and cannot be offloaded";
  }
}

ArrayBuffer::ArrayBuffer(script::EnvironmentSettings* settings, uint32 length)
    : data_(settings, length) {
  // TODO: Once we can have a reliable way to pass the
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
  // TODO: Make EnvironmentSettings mandatory for creating
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
  // TODO: Make EnvironmentSettings mandatory for creating
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
