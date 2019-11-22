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
#include "net/base/net_errors.h"

namespace cobalt {
namespace loader {

// URLFetcherStringWriter::URLFetcherStringWriter(OnWriteCallback
// callback, base::TaskRunner* consumer_task_runner) :
// on_write_callback_(callback), consumer_task_runner_(consumer_task_runner) {
//   DCHECK(consumer_task_runner);
// }

URLFetcherStringWriter::URLFetcherStringWriter() = default;

URLFetcherStringWriter::~URLFetcherStringWriter() = default;

int URLFetcherStringWriter::Initialize(
    net::CompletionOnceCallback /*callback*/) {
  return net::OK;
}

std::unique_ptr<std::string> URLFetcherStringWriter::data() {
  base::AutoLock auto_lock(lock_);
  if (!data_) {
    return std::make_unique<std::string>();
  }
  return std::move(data_);
}

int URLFetcherStringWriter::Write(net::IOBuffer* buffer, int num_bytes,
                                  net::CompletionOnceCallback /*callback*/) {
  base::AutoLock auto_lock(lock_);
  if (!data_) {
    data_ = std::make_unique<std::string>();
  }

  data_->append(buffer->data(), num_bytes);
  // consumer_task_runner_->PostTask(FROM_HERE,
  // base::Bind((on_write_callback_.Run), std::move(data)));
  return num_bytes;
}

int URLFetcherStringWriter::Finish(int /*net_error*/,
                                   net::CompletionOnceCallback /*callback*/) {
  return net::OK;
}

}  // namespace loader
}  // namespace cobalt
