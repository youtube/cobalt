// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/request_service.h"

#include "content/browser/webid/metrics.h"
#include "content/browser/webid/request.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace content {

DOCUMENT_USER_DATA_KEY_IMPL(webid::RequestService);

namespace webid {

RequestService::RequestService(RenderFrameHost* rfh)
    : DocumentUserData<RequestService>(rfh) {}

RequestService::~RequestService() {
  if (num_requests_ > 0) {
    Metrics::RecordNumRequestsPerDocument(
        render_frame_host().GetPageUkmSourceId(), num_requests_);
  }
}

void RequestService::BindFederatedAuthRequest(
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver) {
  GetOrCreateActiveRequest()->BindReceiver(std::move(receiver));
}

void RequestService::BindFederatedRequestService(
    mojo::PendingReceiver<blink::mojom::FederatedRequestService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

Request& RequestService::CreateRequestForTesting(
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver,
    FederatedIdentityApiPermissionContextDelegate* api_permission_delegate,
    FederatedIdentityAutoReauthnPermissionContextDelegate*
        auto_reauthn_permission_delegate,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    IdentityRegistry* identity_registry) {
  active_request_ = std::make_unique<Request>(
      &render_frame_host(), this, api_permission_delegate,
      auto_reauthn_permission_delegate, permission_delegate, identity_registry);
  active_request_->BindReceiver(std::move(receiver));
  return *active_request_;
}

Request* RequestService::GetOrCreateActiveRequest() {
  if (!active_request_) {
    RenderFrameHost& rfh = render_frame_host();
    BrowserContext* browser_context = rfh.GetBrowserContext();
    active_request_ = std::make_unique<Request>(
        &rfh, this, browser_context->GetFederatedIdentityApiPermissionContext(),
        browser_context->GetFederatedIdentityAutoReauthnPermissionContext(),
        browser_context->GetFederatedIdentityPermissionContext(),
        IdentityRegistry::FromWebContents(
            WebContents::FromRenderFrameHost(&rfh)));
  }
  return active_request_.get();
}

void RequestService::OnRequestDestroyed(Request* request) {
  if (active_request_.get() == request) {
    active_request_.reset();
  }
}

}  // namespace webid
}  // namespace content
