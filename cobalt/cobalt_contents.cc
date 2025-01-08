// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cobalt_contents.h"

namespace cobalt {

// static
CobaltContents* CobaltContents::FromWebContents(WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return AwContentsUserData::GetContents(web_contents);
}

CobaltContents::CobaltContents(std::unique_ptr<WebContents> web_contents)
    : content::WebContentsObserver(web_contents.get()),
      web_contents_(std::move(web_contents)) {
  base::subtle::NoBarrier_AtomicIncrement(&g_instance_count, 1);
  icon_helper_ = std::make_unique<IconHelper>(web_contents_.get());
  icon_helper_->SetListener(this);
  web_contents_->SetUserData(android_webview::kAwContentsUserDataKey,
                             std::make_unique<AwContentsUserData>(this));
  browser_view_renderer_.RegisterWithWebContents(web_contents_.get());

  viz::FrameSinkId frame_sink_id;
  if (web_contents_->GetRenderViewHost()) {
    frame_sink_id =
        web_contents_->GetRenderViewHost()->GetWidget()->GetFrameSinkId();
  }

  browser_view_renderer_.SetActiveFrameSinkId(frame_sink_id);
  render_view_host_ext_ =
      std::make_unique<AwRenderViewHostExt>(this, web_contents_.get());

  InitializePageLoadMetricsForWebContents(web_contents_.get());

  permission_request_handler_ =
      std::make_unique<PermissionRequestHandler>(this, web_contents_.get());

  AwAutofillClient* browser_autofill_manager_delegate =
      AwAutofillClient::FromWebContents(web_contents_.get());
  if (browser_autofill_manager_delegate) {
    InitAutofillIfNecessary(
        browser_autofill_manager_delegate->GetSaveFormData());
  }
  content::SynchronousCompositor::SetClientForWebContents(
      web_contents_.get(), &browser_view_renderer_);
  AwContentsLifecycleNotifier::GetInstance().OnWebViewCreated(this);
  AwBrowserProcess::GetInstance()->visibility_metrics_logger()->AddClient(this);
}

}  // namespace cobalt
