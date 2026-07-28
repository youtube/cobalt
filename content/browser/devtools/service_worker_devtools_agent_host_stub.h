// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_STUB_H_

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


#endif  // CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_STUB_H_
