// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef CONTENT_BROWSER_DEVTOOLS_COBALT_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_COBALT_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_STUB_H_

#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"

namespace content {

class ServiceWorkerDevToolsAgentHost : public DevToolsAgentHostImpl {
 public:
  const base::UnguessableToken& devtools_worker_token() const {
    static const base::UnguessableToken token;
    return token;
  }
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_COBALT_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_STUB_H_
