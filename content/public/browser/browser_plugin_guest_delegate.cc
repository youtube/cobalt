// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_plugin_guest_delegate.h"

namespace content {

std::unique_ptr<WebContents> BrowserPluginGuestDelegate::CreateNewGuestWindow(
    const WebContents::CreateParams& create_params) {
  NOTREACHED();
  return nullptr;
}

WebContents* BrowserPluginGuestDelegate::GetOwnerWebContents() {
  return nullptr;
}

RenderFrameHost* BrowserPluginGuestDelegate::GetProspectiveOuterDocument() {
  return nullptr;
}

base::WeakPtr<BrowserPluginGuestDelegate>
BrowserPluginGuestDelegate::GetGuestDelegateWeakPtr() {
  NOTREACHED();
  return nullptr;
}

}  // namespace content
