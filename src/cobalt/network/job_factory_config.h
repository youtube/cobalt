// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_NETWORK_JOB_FACTORY_CONFIG_H_
#define COBALT_NETWORK_JOB_FACTORY_CONFIG_H_

#include "net/url_request/url_request_job_factory.h"

namespace cobalt {
namespace network {

// May add interceptors, protocol handlers, or set other options on the given
// request_factory.  Caller retains ownership of request_factory.
void ConfigureRequestJobFactory(net::URLRequestJobFactory* job_factory);

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_JOB_FACTORY_CONFIG_H_
