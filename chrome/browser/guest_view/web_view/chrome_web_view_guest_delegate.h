// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_GUEST_DELEGATE_H_
#define CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_GUEST_DELEGATE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/api/web_view/chrome_web_view_internal_api.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_guest_delegate.h"

class GURL;
class RenderViewContextMenuBase;

namespace blink {
struct UserAgentOverride;
}  // namespace blink

namespace extensions {

class ChromeWebViewGuestDelegate : public WebViewGuestDelegate {
 public :
  explicit ChromeWebViewGuestDelegate(WebViewGuest* web_view_guest);

  ChromeWebViewGuestDelegate(const ChromeWebViewGuestDelegate&) = delete;
  ChromeWebViewGuestDelegate& operator=(const ChromeWebViewGuestDelegate&) =
      delete;

  ~ChromeWebViewGuestDelegate() override;

  // WebViewGuestDelegate implementation.
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  void OnShowContextMenu(int request_id) override;
  bool NavigateToURLShouldBlock(const GURL& url) override;
  std::optional<blink::UserAgentOverride> GetDefaultUserAgentOverride()
      override;
  void SetClientHintsEnabled(bool enable) override;

  WebViewGuest* web_view_guest() const { return web_view_guest_; }

 private:
  // A counter to generate a unique request id for a context menu request.
  // We only need the ids to be unique for a given WebViewGuest.
  int pending_context_menu_request_id_;

  // Holds the RenderViewContextMenuBase that has been built but yet to be
  // shown. This is .reset() after ShowContextMenu().
  std::unique_ptr<RenderViewContextMenuBase> pending_menu_;

  const raw_ptr<WebViewGuest> web_view_guest_;

  bool enable_client_hints_brand_ = true;

  // This is used to ensure pending tasks will not fire after this object is
  // destroyed.
  base::WeakPtrFactory<ChromeWebViewGuestDelegate> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_GUEST_DELEGATE_H_
