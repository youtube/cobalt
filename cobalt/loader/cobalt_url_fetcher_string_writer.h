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

#ifndef COBALT_URL_FETCHER_RESPONSE_WRITER_H_
#define COBALT_URL_FETCHER_RESPONSE_WRITER_H_

#include <memory>

#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_fetcher_response_writer.h"

namespace cobalt {

class CobaltURLFetcherStringWriter : public net::URLFetcherResponseWriter {
 public:
  // typedef base::RepeatingCallback<void(std::unique_ptr<std::string>)>
  // OnWriteCallback; CobaltURLFetcherStringWriter(OnWriteCallback callback,
  // base::TaskRunner* consumer_task_runner);
  CobaltURLFetcherStringWriter();
  ~CobaltURLFetcherStringWriter() override;

  std::unique_ptr<std::string> data();

  // URLFetcherResponseWriter overrides:
  int Initialize(net::CompletionOnceCallback callback) override;
  int Write(net::IOBuffer* buffer, int num_bytes,
            net::CompletionOnceCallback callback) override;
  int Finish(int net_error, net::CompletionOnceCallback callback) override;

 private:
  // This class can be accessed by both network thread and MainWebModule
  // thread.
  base::Lock lock_;
  std::unique_ptr<std::string> data_;
  // OnWriteCallback on_write_callback_;
  // base::TaskRunner* consumer_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(CobaltURLFetcherStringWriter);
};

}  // namespace cobalt

#endif  // COBALT_URL_FETCHER_RESPONSE_WRITER_H_