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

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_STUB_H_

#include <string>
#include "content/common/content_export.h"
#include "content/public/browser/devtools_agent_host.h"

namespace content {

class CONTENT_EXPORT DevToolsAgentHostImpl : public DevToolsAgentHost {
 public:
  static scoped_refptr<DevToolsAgentHostImpl> GetForId(const std::string& id);
  static void GetOrCreateAll();
  bool Inspect();
};

}  // namespace content


#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_STUB_H_
