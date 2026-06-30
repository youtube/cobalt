#include "content/public/browser/devtools_agent_host.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/protocol/page_handler.h"

namespace content {

// static
bool DevToolsAgentHost::IsDebuggerAttached(WebContents* web_contents) {
  return false;
}

// static
void RenderFrameDevToolsAgentHost::AttachToWebContents(WebContents* web_contents) {
  // no-op
}

namespace protocol {
// static
std::vector<PageHandler*> PageHandler::EnabledForWebContents(WebContentsImpl* contents) {
  return {};
}
} // namespace protocol

} // namespace content
