// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_AGENT_HOST_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_AGENT_HOST_STUB_H_

#include "content/browser/devtools/devtools_agent_host_impl.h"

namespace content {

class SharedWorkerHost;

class SharedWorkerDevToolsAgentHost : public DevToolsAgentHostImpl {
 public:
  static SharedWorkerDevToolsAgentHost* GetFor(SharedWorkerHost* worker_host);
};

}  // namespace content


#endif  // CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_AGENT_HOST_STUB_H_
