// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/network/url_request_context_getter.h"

#include "cobalt/network/url_request_context.h"

namespace cobalt {
namespace network {

URLRequestContextGetter::URLRequestContextGetter(
    URLRequestContext* url_request_context, base::Thread* io_thread)
    : url_request_context_(url_request_context) {
  network_task_runner_ = io_thread->task_runner();
  DCHECK(network_task_runner_);
}

URLRequestContextGetter::~URLRequestContextGetter() {}

net::URLRequestContext* URLRequestContextGetter::GetURLRequestContext() {
  return url_request_context_;
}

scoped_refptr<base::SingleThreadTaskRunner>
URLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

}  // namespace network
}  // namespace cobalt
