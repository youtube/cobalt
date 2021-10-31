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

#include "cobalt/loader/url_fetcher_string_writer.h"

#include "base/logging.h"
#include "net/base/net_errors.h"

    namespace cobalt {
namespace loader {

// URLFetcherStringWriter::URLFetcherStringWriter(OnWriteCallback
// callback, base::TaskRunner* consumer_task_runner) :
// on_write_callback_(callback), consumer_task_runner_(consumer_task_runner) {
//   DCHECK(consumer_task_runner);
// }

namespace {
const int64_t kPreAllocateThreshold = 64 * 1024;
}  // namespace

URLFetcherStringWriter::URLFetcherStringWriter() = default;

URLFetcherStringWriter::~URLFetcherStringWriter() = default;

int URLFetcherStringWriter::Initialize(net::CompletionOnceCallback callback) {
  return net::OK;
}

void URLFetcherStringWriter::OnResponseStarted(int64_t content_length) {
  base::AutoLock auto_lock(lock_);

  if (content_length >= 0) {
    content_length_ = content_length;
  }
}

bool URLFetcherStringWriter::HasData() const {
  base::AutoLock auto_lock(lock_);
  return !data_.empty();
}

void URLFetcherStringWriter::GetAndResetData(std::string* data) {
  DCHECK(data);

  std::string empty;
  data->swap(empty);

  base::AutoLock auto_lock(lock_);
  data_.swap(*data);
}

int URLFetcherStringWriter::Write(net::IOBuffer* buffer, int num_bytes,
                                  net::CompletionOnceCallback callback) {
  base::AutoLock auto_lock(lock_);

  if (content_offset_ == 0 && num_bytes <= content_length_) {
    // Pre-allocate the whole buffer for small downloads, hope that all data can
    // be downloaded before GetAndResetData() is called.
    if (content_length_ <= kPreAllocateThreshold) {
      data_.reserve(content_length_);
    } else {
      data_.reserve(kPreAllocateThreshold);
    }
  }

  if (content_length_ > 0 && content_length_ > content_offset_ &&
      data_.size() + num_bytes > data_.capacity()) {
    // There is not enough memory allocated, and std::string is going to double
    // the allocation.  So a content in "1M + 1" bytes may end up allocating 2M
    // bytes.  Try to reserve the proper size to avoid this.
    auto content_remaining = content_length_ - content_offset_;
    if (data_.size() + content_remaining < data_.capacity() * 2) {
      data_.reserve(data_.size() + content_remaining);
    }
  }

  data_.append(buffer->data(), num_bytes);
  content_offset_ += num_bytes;
  // consumer_task_runner_->PostTask(FROM_HERE,
  // base::Bind((on_write_callback_.Run), std::move(data)));
  return num_bytes;
}

int URLFetcherStringWriter::Finish(int net_error,
                                   net::CompletionOnceCallback callback) {
  return net::OK;
}

}  // namespace loader
}  // namespace cobalt
