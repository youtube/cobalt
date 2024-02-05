// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LOADER_FETCH_INTERCEPTOR_COORDINATOR_H_
#define COBALT_LOADER_FETCH_INTERCEPTOR_COORDINATOR_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace cobalt {
namespace loader {

class FetchInterceptor {
 public:
  virtual void StartFetch(
      const GURL& url, bool main_resource,
      const net::HttpRequestHeaders& request_headers,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(std::unique_ptr<std::string>)> callback,
      base::OnceCallback<void(const net::LoadTimingInfo&)>
          report_load_timing_info,
      base::OnceClosure fallback) = 0;
};

// NetFetcher is for fetching data from the network.
class FetchInterceptorCoordinator {
 public:
  static FetchInterceptorCoordinator* GetInstance();

  void Add(FetchInterceptor* fetch_interceptor) {
    fetch_interceptor_ = fetch_interceptor;
  }

  void Clear() { fetch_interceptor_ = nullptr; }

  void TryIntercept(
      const GURL& url, bool main_resource,
      const net::HttpRequestHeaders& request_headers,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::OnceCallback<void(std::unique_ptr<std::string>)> callback,
      base::OnceCallback<void(const net::LoadTimingInfo&)>
          report_load_timing_info,
      base::OnceClosure fallback);

 private:
  friend struct base::DefaultSingletonTraits<FetchInterceptorCoordinator>;
  FetchInterceptorCoordinator();

  FetchInterceptor* fetch_interceptor_;

  DISALLOW_COPY_AND_ASSIGN(FetchInterceptorCoordinator);
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_FETCH_INTERCEPTOR_COORDINATOR_H_
