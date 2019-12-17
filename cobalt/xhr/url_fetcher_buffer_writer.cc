// Copyright 2019 Google Inc. All Rights Reserved.
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

#include "cobalt/xhr/url_fetcher_buffer_writer.h"

#include "base/logging.h"
#include "net/base/net_errors.h"
#include "starboard/memory.h"

namespace cobalt {
namespace xhr {

namespace {

// Allocate 64KB if the total size is unknown to avoid allocating small buffer
// too many times.
const int64_t kDefaultPreAllocateSizeInBytes = 64 * 1024;

void ReleaseMemory(std::string* str) {
  DCHECK(str);
  std::string empty;
  str->swap(empty);
}

void ReleaseMemory(script::PreallocatedArrayBufferData* data) {
  DCHECK(data);
  script::PreallocatedArrayBufferData empty;
  data->Swap(&empty);
}

}  // namespace

URLFetcherResponseWriter::Buffer::Buffer(Type type) : type_(type) {}

void URLFetcherResponseWriter::Buffer::DisablePreallocate() {
  base::AutoLock auto_lock(lock_);

  DCHECK_EQ(GetSize_Locked(), 0u);
  allow_preallocate_ = false;
}

void URLFetcherResponseWriter::Buffer::Clear() {
  base::AutoLock auto_lock(lock_);

  ReleaseMemory(&data_as_string_);
  ReleaseMemory(&copy_of_data_as_string_);
  ReleaseMemory(&data_as_array_buffer_);

  download_progress_ = 0;
  data_as_array_buffer_size_ = 0;
}

int64_t URLFetcherResponseWriter::Buffer::GetAndResetDownloadProgress() {
  base::AutoLock auto_lock(lock_);
  download_progress_ = GetSize_Locked();
  return static_cast<int64_t>(download_progress_);
}

bool URLFetcherResponseWriter::Buffer::HasProgressSinceLastGetAndReset() const {
  base::AutoLock auto_lock(lock_);
  return GetSize_Locked() > download_progress_;
}

const std::string&
URLFetcherResponseWriter::Buffer::GetReferenceOfStringAndSeal() {
  base::AutoLock auto_lock(lock_);

  UpdateType_Locked(kString);
  allow_write_ = false;

  return data_as_string_;
}

const std::string&
URLFetcherResponseWriter::Buffer::GetTemporaryReferenceOfString() {
  base::AutoLock auto_lock(lock_);

  // This function can be further optimized by always return reference of
  // |data_as_string_|, and only make a copy when |data_as_string_| is extended.
  //  It is not done as GetTemporaryReferenceOfString() is currently not
  // triggered.  It will only be called when JS app is retrieving responseText
  // while the request is still in progress.

  if (type_ == kString) {
    copy_of_data_as_string_ = data_as_string_;
  } else {
    DCHECK_EQ(type_, kArrayBuffer);
    const char* begin = static_cast<const char*>(data_as_array_buffer_.data());
    copy_of_data_as_string_.assign(begin, begin + data_as_array_buffer_size_);
  }

  return copy_of_data_as_string_;
}

void URLFetcherResponseWriter::Buffer::GetAndReset(std::string* str) {
  DCHECK(str);

  ReleaseMemory(str);

  base::AutoLock auto_lock(lock_);

  UpdateType_Locked(kString);

  if (capacity_known_ && data_as_string_.size() != data_as_string_.capacity()) {
    DLOG(WARNING) << "String size " << data_as_string_.size()
                  << " is different than its preset capacity "
                  << data_as_string_.capacity();
  }

  data_as_string_.swap(*str);
}

void URLFetcherResponseWriter::Buffer::GetAndReset(
    PreallocatedArrayBufferData* data) {
  DCHECK(data);

  ReleaseMemory(data);

  base::AutoLock auto_lock(lock_);

  UpdateType_Locked(kArrayBuffer);

  if (data_as_array_buffer_.byte_length() != data_as_array_buffer_size_) {
    DCHECK_LT(data_as_array_buffer_size_, data_as_array_buffer_.byte_length());
    DLOG_IF(WARNING, capacity_known_)
        << "ArrayBuffer size " << data_as_array_buffer_size_
        << " is different than its preset capacity "
        << data_as_array_buffer_.byte_length();
    data_as_array_buffer_.Resize(data_as_array_buffer_size_);
  }
  data_as_array_buffer_.Swap(data);
}

void URLFetcherResponseWriter::Buffer::MaybePreallocate(int64_t capacity) {
  base::AutoLock auto_lock(lock_);

  if (!allow_preallocate_) {
    return;
  }

  if (capacity < 0) {
    capacity = kDefaultPreAllocateSizeInBytes;
  } else {
    capacity_known_ = true;
  }

  if (capacity == 0) {
    return;
  }

  switch (type_) {
    case kString:
      DCHECK_EQ(data_as_string_.size(), 0u);
      data_as_string_.reserve(capacity);
      return;
    case kArrayBuffer:
      DCHECK_EQ(data_as_array_buffer_size_, 0u);
      data_as_array_buffer_.Resize(capacity);
      return;
  }
  NOTREACHED();
}

void URLFetcherResponseWriter::Buffer::Write(const void* buffer,
                                             int num_bytes) {
  DCHECK_GE(num_bytes, 0);

  if (num_bytes <= 0) {
    return;
  }

  base::AutoLock auto_lock(lock_);

  DCHECK(allow_write_);

  if (!allow_write_) {
    return;
  }

  if (type_ == kString) {
    if (capacity_known_ &&
        num_bytes + data_as_string_.size() >= data_as_string_.capacity()) {
      SB_LOG(WARNING) << "Data written is larger than the preset capacity "
                      << data_as_string_.capacity();
    }
    data_as_string_.append(static_cast<const char*>(buffer), num_bytes);
    return;
  }

  DCHECK_EQ(type_, kArrayBuffer);
  if (data_as_array_buffer_size_ + num_bytes >
      data_as_array_buffer_.byte_length()) {
    if (capacity_known_) {
      SB_LOG(WARNING) << "Data written is larger than the preset capacity "
                      << data_as_array_buffer_.byte_length();
    }
    data_as_array_buffer_.Resize(data_as_array_buffer_size_ + num_bytes);
  }

  auto destination = static_cast<uint8_t*>(data_as_array_buffer_.data()) +
                     data_as_array_buffer_size_;
  SbMemoryCopy(destination, buffer, num_bytes);
  data_as_array_buffer_size_ += num_bytes;
}

size_t URLFetcherResponseWriter::Buffer::GetSize_Locked() const {
  lock_.AssertAcquired();

  switch (type_) {
    case kString:
      return data_as_string_.size();
    case kArrayBuffer:
      return data_as_array_buffer_size_;
  }
  NOTREACHED();
  return 0;
}

void URLFetcherResponseWriter::Buffer::UpdateType_Locked(Type type) {
  lock_.AssertAcquired();

  if (type_ == type) {
    return;
  }

  DCHECK(allow_write_);

  DLOG_IF(WARNING, GetSize_Locked() > 0)
      << "Change response type from " << type_ << " to " << type
      << " after response is started, which is less efficient.";

  if (type_ == kString) {
    DCHECK_EQ(type, kArrayBuffer);
    DCHECK_EQ(data_as_array_buffer_size_, 0u);
    DCHECK_EQ(data_as_array_buffer_.byte_length(), 0u);
  } else {
    DCHECK_EQ(type_, kArrayBuffer);
    DCHECK_EQ(type, kString);
    DCHECK_EQ(data_as_string_.size(), 0u);
  }

  type_ = type;

  if (type == kArrayBuffer) {
    data_as_array_buffer_.Resize(data_as_string_.capacity());
    data_as_array_buffer_size_ = data_as_string_.size();
    SbMemoryCopy(data_as_array_buffer_.data(), data_as_string_.data(),
                 data_as_array_buffer_size_);

    ReleaseMemory(&data_as_string_);
    ReleaseMemory(&copy_of_data_as_string_);
    return;
  }

  data_as_string_.reserve(data_as_array_buffer_.byte_length());
  data_as_string_.append(static_cast<const char*>(data_as_array_buffer_.data()),
                         data_as_array_buffer_size_);

  ReleaseMemory(&data_as_array_buffer_);
  data_as_array_buffer_size_ = 0;
}

URLFetcherResponseWriter::URLFetcherResponseWriter(
    const scoped_refptr<Buffer>& buffer)
    : buffer_(buffer) {
  DCHECK(buffer_);
}

URLFetcherResponseWriter::~URLFetcherResponseWriter() = default;

int URLFetcherResponseWriter::Initialize(
    net::CompletionOnceCallback /*callback*/) {
  return net::OK;
}

void URLFetcherResponseWriter::OnResponseStarted(int64_t content_length) {
  buffer_->MaybePreallocate(content_length);
}

int URLFetcherResponseWriter::Write(net::IOBuffer* buffer, int num_bytes,
                                    net::CompletionOnceCallback /*callback*/) {
  buffer_->Write(buffer->data(), num_bytes);
  return num_bytes;
}

int URLFetcherResponseWriter::Finish(int /*net_error*/,
                                     net::CompletionOnceCallback /*callback*/) {
  return net::OK;
}

}  // namespace xhr
}  // namespace cobalt
