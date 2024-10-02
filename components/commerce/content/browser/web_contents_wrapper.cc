// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/content/browser/web_contents_wrapper.h"

#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"

namespace commerce {

WebContentsWrapper::WebContentsWrapper(content::WebContents* web_contents,
                                       int32_t js_world_id)
    : web_contents_(web_contents), js_world_id_(js_world_id) {}

const GURL& WebContentsWrapper::GetLastCommittedURL() {
  if (!web_contents_)
    return GURL::EmptyGURL();

  return web_contents_->GetLastCommittedURL();
}

bool WebContentsWrapper::IsOffTheRecord() {
  if (!web_contents_ || !web_contents_->GetBrowserContext())
    return false;

  return web_contents_->GetBrowserContext()->IsOffTheRecord();
}

void WebContentsWrapper::RunJavascript(
    const std::u16string& script,
    base::OnceCallback<void(const base::Value)> callback) {
  if (!web_contents_ && web_contents_->GetPrimaryMainFrame()) {
    std::move(callback).Run(base::Value());
    return;
  }

  web_contents_->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
      script, std::move(callback), js_world_id_);
}

void WebContentsWrapper::ClearWebContentsPointer() {
  web_contents_ = nullptr;
}

}  // namespace commerce
