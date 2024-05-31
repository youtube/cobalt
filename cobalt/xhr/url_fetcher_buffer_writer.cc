// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include <algorithm>

#include "base/logging.h"
#include "net/base/net_errors.h"
#include "starboard/memory.h"

namespace cobalt {
namespace xhr {

namespace {

// Allocate 64KB if the total size is unknown to avoid allocating small buffer
// too many times.
const int64_t kDefaultPreAllocateSizeInBytes = 64 * 1024;
// Set max allocate size to avoid erroneous size estimate.
const int64_t kMaxPreAllocateSizeInBytes = 10 * 1024 * 1024;
const uint8_t kResizingMultiplier = 2;

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

URLFetcherResponseWriter::Buffer::Buffer(
    Type type, const base::Optional<bool>& enable_try_lock_for_progress_check)
    : type_(type),
      enable_try_lock_for_progress_check_(
          enable_try_lock_for_progress_check.value_or(false)) {
  DCHECK_NE(type_, kBufferPool);
}

URLFetcherResponseWriter::Buffer::Buffer(
    Type type, const base::Optional<bool>& enable_try_lock_for_progress_check,
    const base::Optional<int>& fetch_buffer_size)
    : type_(type),
      enable_try_lock_for_progress_check_(
          enable_try_lock_for_progress_check.value_or(true)),
      data_as_buffer_pool_(fetch_buffer_size) {
  DCHECK_EQ(type_, kBufferPool);
}

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

bool URLFetcherResponseWriter::Buffer::HasProgressSinceLastGetAndReset(
    bool request_done) const {
  if (!enable_try_lock_for_progress_check_) {
    base::AutoLock auto_lock(lock_);
    return GetSize_Locked() > download_progress_;
  }

  // The |lock_| might be held on the network thread to copy data, which can be
  // slow sometimes.  Waiting for |lock_| may block the JS thread for extended
  // amount of time, so we only wait for it when the |request_done| is true,
  // where we absolutely have to know the correct progress.
  if (request_done) {
    base::AutoLock auto_lock(lock_);
    return GetSize_Locked() > download_progress_;
  }
  if (!lock_.Try()) {
    // Failed to acquire |lock_|, assume there is no progress.
    return false;
  }
  bool has_progress = GetSize_Locked() > download_progress_;
  lock_.Release();
  return has_progress;
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
  DCHECK_NE(type_, kBufferPool);

  base::AutoLock auto_lock(lock_);

  // This function can be further optimized by always return reference of
  // |data_as_string_|, and only make a copy when |data_as_string_| is extended.
  // It is not done as GetTemporaryReferenceOfString() is currently not
  // triggered.  It will only be called when JS app is retrieving responseText
  // while the request is still in progress.
  switch (type_) {
    case kString:
      copy_of_data_as_string_ = data_as_string_;
      break;
    case kArrayBuffer: {
      auto data = data_as_array_buffer_.data();
      copy_of_data_as_string_.assign(data, data + data_as_array_buffer_size_);
    } break;
    case kBufferPool:
      NOTREACHED();
      break;
  }

  return copy_of_data_as_string_;
}

void URLFetcherResponseWriter::Buffer::GetAndResetDataAndDownloadProgress(
    std::string* str) {
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

  // It is important to reset the |download_progress_| and return the data in
  // one function to avoid potential race condition that may prevent the last
  // bit of data of Fetcher from being downloaded, because the data download is
  // guarded by HasProgressSinceLastGetAndReset().
  download_progress_ = 0;
}

void URLFetcherResponseWriter::Buffer::GetAndResetDataAndDownloadProgress(
    bool request_done,
    std::vector<script::PreallocatedArrayBufferData>* buffers) {
  DCHECK(buffers);

  buffers->clear();

  base::AutoLock auto_lock(lock_);

  DCHECK_EQ(type_, kBufferPool);
  data_as_buffer_pool_.ResetAndReturnAsArrayBuffers(request_done, buffers);

  // It is important to reset the |download_progress_| and return the data in
  // one function to avoid potential race condition that may prevent the last
  // bit of data of Fetcher from being downloaded, because the data download is
  // guarded by HasProgressSinceLastGetAndReset().
  download_progress_ = 0;
}

void URLFetcherResponseWriter::Buffer::GetAndResetData(
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
  } else if (capacity > kMaxPreAllocateSizeInBytes) {
    LOG(WARNING) << "Allocation of " << capacity << " bytes is capped to "
                 << kMaxPreAllocateSizeInBytes;
    capacity = kMaxPreAllocateSizeInBytes;
  } else {
    capacity_known_ = true;
  }
  // Record the desired_capacity_ to avoid reserving unused memory during
  // resizing.
  desired_capacity_ = static_cast<size_t>(capacity);

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
    case kBufferPool:
      // FetchBufferPool don't need preallocate.
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

  const char* data = static_cast<const char*>(buffer);

  base::AutoLock auto_lock(lock_);

  DCHECK(allow_write_);

  if (!allow_write_) {
    return;
  }

  if (type_ == kString) {
    if (capacity_known_ &&
        num_bytes + data_as_string_.size() >= data_as_string_.capacity()) {
      LOG(WARNING) << "Data written is larger than the preset capacity "
                   << data_as_string_.capacity();
    }
    data_as_string_.append(data, num_bytes);
    return;
  }

  if (type_ == kArrayBuffer) {
    if (data_as_array_buffer_size_ + num_bytes >
        data_as_array_buffer_.byte_length()) {
      if (capacity_known_) {
        LOG(WARNING) << "Data written is larger than the preset capacity "
                     << data_as_array_buffer_.byte_length();
      }
      size_t new_size = std::max(
          std::min(data_as_array_buffer_.byte_length() * kResizingMultiplier,
                   desired_capacity_),
          data_as_array_buffer_size_ + num_bytes);
      if (new_size > desired_capacity_) {
        // Content-length is wrong, response size is completely unknown.
        // Double the capacity to avoid frequent resizing.
        new_size *= kResizingMultiplier;
      }
      data_as_array_buffer_.Resize(new_size);
    }

    auto destination = static_cast<uint8_t*>(data_as_array_buffer_.data()) +
                       data_as_array_buffer_size_;
    memcpy(destination, data, num_bytes);
    data_as_array_buffer_size_ += num_bytes;

    return;
  }

  DCHECK_EQ(type_, kBufferPool);
  data_as_buffer_pool_.Write(data, num_bytes);
}

size_t URLFetcherResponseWriter::Buffer::GetSize_Locked() const {
  lock_.AssertAcquired();

  switch (type_) {
    case kString:
      return data_as_string_.size();
    case kArrayBuffer:
      return data_as_array_buffer_size_;
    case kBufferPool:
      return data_as_buffer_pool_.GetSize();
  }
  NOTREACHED();
  return 0;
}

void URLFetcherResponseWriter::Buffer::UpdateType_Locked(Type type) {
  lock_.AssertAcquired();

  if (type_ == type) {
    return;
  }

  // kBufferPool cannot be converted to or from other types.
  DCHECK_NE(type_, kBufferPool);
  DCHECK_NE(type, kBufferPool);

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
    memcpy(data_as_array_buffer_.data(), data_as_string_.data(),
           data_as_array_buffer_size_);

    ReleaseMemory(&data_as_string_);
    ReleaseMemory(&copy_of_data_as_string_);
    return;
  }

  data_as_string_.reserve(data_as_array_buffer_.byte_length());
  data_as_string_.append(
      reinterpret_cast<const char*>(data_as_array_buffer_.data()),
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

int URLFetcherResponseWriter::Initialize(net::CompletionOnceCallback callback) {
  return net::OK;
}

void URLFetcherResponseWriter::OnResponseStarted(int64_t content_length) {
  buffer_->MaybePreallocate(content_length);
}

int URLFetcherResponseWriter::Write(net::IOBuffer* buffer, int num_bytes,
                                    net::CompletionOnceCallback callback) {
  buffer_->Write(buffer->data(), num_bytes);
  return num_bytes;
}

int URLFetcherResponseWriter::Finish(int net_error,
                                     net::CompletionOnceCallback callback) {
  return net::OK;
}

}  // namespace xhr
}  // namespace cobalt
