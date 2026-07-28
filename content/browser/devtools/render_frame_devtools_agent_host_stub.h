// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_STUB_H_

#include "content/common/content_export.h"
#include "content/public/browser/devtools_agent_host.h"

namespace content {

class DevToolsAgentHostImpl;
class FrameTreeNode;
class RenderFrameHost;
class RenderFrameHostImpl;
class WebContents;

class CONTENT_EXPORT RenderFrameDevToolsAgentHost {
 public:
  static bool WasEverAttachedToAnyFrame();
  static bool IsDebuggerAttached(WebContents* web_contents);
  static DevToolsAgentHostImpl* GetFor(FrameTreeNode* frame_tree_node);
  static DevToolsAgentHostImpl* GetFor(RenderFrameHostImpl* rfh);
  static scoped_refptr<DevToolsAgentHost> GetOrCreateFor(
      FrameTreeNode* frame_tree_node);
  static bool ShouldCreateDevToolsForHost(RenderFrameHostImpl* rfh);
  static void AttachToWebContents(WebContents* web_contents);
};

}  // namespace content


#endif  // CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_STUB_H_
