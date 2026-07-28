// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
