// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_DELEGATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_DELEGATE_H_

#include <optional>

#include "base/functional/callback.h"
#include "components/guest_view/browser/guest_view_base.h"

class GURL;

namespace blink {
struct UserAgentOverride;
}  // namespace blink

namespace content {
class RenderFrameHost;
struct ContextMenuParams;
}  // namespace content

namespace extensions {

// A delegate class of WebViewGuest that are not a part of chrome.
class WebViewGuestDelegate {
 public :
  virtual ~WebViewGuestDelegate() {}

  // Called when context menu operation was handled.
  //
  // The `render_frame_host` represents the frame that requests the context menu
  // (typically this frame is focused, but this is not necessarily the case -
  // see https://crbug.com/1257907#c14).
  virtual bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                                 const content::ContextMenuParams& params) = 0;

  // Shows the context menu for the guest.
  virtual void OnShowContextMenu(int request_id) = 0;

  // Called during `LoadURLWithParams` to check whether delegates have more
  // scheme blocks in place.
  virtual bool NavigateToURLShouldBlock(const GURL& url) = 0;

  // Returns the default UserAgentOverride value for the guest, or null if the
  // guest does not have a special default.
  virtual std::optional<blink::UserAgentOverride>
  GetDefaultUserAgentOverride() = 0;

  // Toggles whether to include special client hints brand in the user agent
  // override.
  virtual void SetClientHintsEnabled(bool enable) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_GUEST_DELEGATE_H_
