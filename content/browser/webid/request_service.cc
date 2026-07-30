// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/request_service.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/disconnect_request.h"
#include "content/browser/webid/fake_identity_request_dialog_controller.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/idp_registration_handler.h"
#include "content/browser/webid/metrics.h"
#include "content/browser/webid/request.h"
#include "content/browser/webid/request_page_data.h"
#include "content/browser/webid/user_info_request.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webid/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/webid/federated_identity_auto_reauthn_permission_context_delegate.h"
#include "content/public/browser/webid/federated_identity_permission_context_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/message.h"

namespace content {

DOCUMENT_USER_DATA_KEY_IMPL(webid::RequestService);

namespace webid {

using blink::mojom::RegisterIdpStatus;

RequestService::RequestService(RenderFrameHost* rfh)
    : DocumentUserData<RequestService>(rfh),
      api_permission_delegate_(
          rfh->GetBrowserContext()->GetFederatedIdentityApiPermissionContext()),
      auto_reauthn_permission_delegate_(
          rfh->GetBrowserContext()
              ->GetFederatedIdentityAutoReauthnPermissionContext()),
      permission_delegate_(
          rfh->GetBrowserContext()->GetFederatedIdentityPermissionContext()) {
  CHECK(api_permission_delegate_);
  CHECK(auto_reauthn_permission_delegate_);
  CHECK(permission_delegate_);
}

RequestService::~RequestService() {
  // Invalidate weak pointers before clearing `user_info_requests_` to prevent
  // the destroying UserInfoRequests from calling back re-entrantly into
  // CompleteUserInfoRequest (which would cause container corruption during
  // clear).
  weak_ptr_factory_.InvalidateWeakPtrs();
  user_info_requests_.clear();
  disconnect_request_.reset();
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
  api_permission_delegate_ = api_permission_delegate;
  auto_reauthn_permission_delegate_ = auto_reauthn_permission_delegate;
  permission_delegate_ = permission_delegate;
  active_request_ = std::make_unique<Request>(
      &render_frame_host(), *this, api_permission_delegate,
      auto_reauthn_permission_delegate, permission_delegate, identity_registry);
  active_request_->BindReceiver(std::move(receiver));
  return *active_request_;
}

Request* RequestService::GetOrCreateActiveRequest() {
  if (!active_request_) {
    RenderFrameHost& rfh = render_frame_host();
    BrowserContext* browser_context = rfh.GetBrowserContext();
    active_request_ = std::make_unique<Request>(
        &rfh, *this,
        browser_context->GetFederatedIdentityApiPermissionContext(),
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

void RequestService::SetNetworkManagerForTests(
    std::unique_ptr<IdpNetworkRequestManager> manager) {
  mock_network_manager_ = std::move(manager);
}

std::unique_ptr<IdpNetworkRequestManager>
RequestService::CreateNetworkManager() {
  if (mock_network_manager_) {
    return std::move(mock_network_manager_);
  }
  return IdpNetworkRequestManager::Create(
      static_cast<RenderFrameHostImpl*>(&render_frame_host()));
}

void RequestService::RegisterIdP(const GURL& idp,
                                 RegisterIdPCallback callback) {
  if (!IsIdPRegistrationEnabled()) {
    std::move(callback).Run(RegisterIdpStatus::kErrorFeatureDisabled);
    return;
  }

  if (!render_frame_host().GetLastCommittedOrigin().IsSameOriginWith(
          url::Origin::Create(idp))) {
    std::move(callback).Run(RegisterIdpStatus::kErrorCrossOriginConfig);
    return;
  }

  // TODO(crbug.com/519217823): Determine whether having a single registration
  // handler and network manager for all registrations is sufficient.
  if (!registration_network_manager_) {
    registration_network_manager_ = CreateNetworkManager();
  }
  fedcm_idp_registration_handler_ = std::make_unique<IdpRegistrationHandler>(
      render_frame_host(), registration_network_manager_.get(), idp);

  fedcm_idp_registration_handler_->FetchConfig(
      base::BindOnce(&RequestService::OnIdpRegistrationConfigFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback), idp));
}

void RequestService::OnIdpRegistrationConfigFetched(
    RegisterIdPCallback callback,
    const GURL& idp,
    std::vector<ConfigFetcher::FetchResult> fetch_results) {
  CHECK_EQ(fetch_results.size(), 1u);
  fedcm_idp_registration_handler_.reset();
  registration_network_manager_.reset();
  if (fetch_results[0].error) {
    std::move(callback).Run(RegisterIdpStatus::kErrorInvalidConfig);
    return;
  }

  permission_delegate_->RegisterIdP(idp);
  std::move(callback).Run(RegisterIdpStatus::kSuccess);
}

void RequestService::UnregisterIdP(const GURL& idp,
                                   UnregisterIdPCallback callback) {
  if (!IsIdPRegistrationEnabled()) {
    std::move(callback).Run(false);
    return;
  }
  if (!render_frame_host().GetLastCommittedOrigin().IsSameOriginWith(
          url::Origin::Create(idp))) {
    std::move(callback).Run(false);
    return;
  }
  permission_delegate_->UnregisterIdP(idp);
  std::move(callback).Run(true);
}

void RequestService::PreventSilentAccess(PreventSilentAccessCallback callback) {
  SetRequiresUserMediation(true, std::move(callback));

  if (permission_delegate_->HasSharingPermission(
          render_frame_host().GetMainFrame()->GetLastCommittedOrigin())) {
    // Ensure the lifecycle state as GetPageUkmSourceId doesn't support the
    // prerendering page. As FederatedAuthRequest runs behind the
    // BrowserInterfaceBinders, the service doesn't receive any request while
    // prerendering, and the CHECK should always meet the condition.
    CHECK(!render_frame_host().IsInLifecycleState(
        RenderFrameHost::LifecycleState::kPrerendering));
    RecordPreventSilentAccess(
        ComputeRequesterFrameType(
            render_frame_host(), render_frame_host().GetLastCommittedOrigin(),
            render_frame_host().GetMainFrame()->GetLastCommittedOrigin()),
        render_frame_host().GetPageUkmSourceId());
  }
}

void RequestService::SetRequiresUserMediation(bool requires_user_mediation,
                                              base::OnceClosure callback) {
  auto_reauthn_permission_delegate_->SetRequiresUserMediation(
      render_frame_host().GetLastCommittedOrigin(), requires_user_mediation);
  if (permission_delegate_) {
    permission_delegate_->OnSetRequiresUserMediation(
        render_frame_host().GetLastCommittedOrigin(), std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void RequestService::RequestUserInfo(
    blink::mojom::IdentityProviderConfigPtr provider,
    RequestUserInfoCallback callback) {
  // Enforce identity-credentials-get Permissions Policy browser-side.
  if (!render_frame_host().IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kIdentityCredentialsGet)) {
    mojo::ReportBadMessage(
        "identity-credentials-get permissions policy not enabled");
    std::move(callback).Run(blink::mojom::RequestUserInfoResult::NewStatus(
        blink::mojom::RequestUserInfoStatus::kError));
    return;
  }

  if (!render_frame_host().GetPage().IsPrimary()) {
    mojo::ReportBadMessage(
        "FedCM should not be allowed in nested frame trees.");
    std::move(callback).Run(blink::mojom::RequestUserInfoResult::NewStatus(
        blink::mojom::RequestUserInfoStatus::kError));
    return;
  }
  // FedCmMetrics class is currently not used for UserInfo API. If we log UKM
  // metrics later on, we should call CreateFedCmMetrics() here.

  auto user_info_request = UserInfoRequest::Create(
      CreateNetworkManager(), permission_delegate_, api_permission_delegate_,
      &render_frame_host(), std::move(provider));
  UserInfoRequest* user_info_request_ptr = user_info_request.get();
  user_info_requests_.insert(std::move(user_info_request));

  user_info_request_ptr->SetCallbackAndStart(base::BindOnce(
      &RequestService::CompleteUserInfoRequest, weak_ptr_factory_.GetWeakPtr(),
      user_info_request_ptr, std::move(callback)));
}

void RequestService::CompleteUserInfoRequest(
    UserInfoRequest* request,
    RequestUserInfoCallback callback,
    blink::mojom::RequestUserInfoStatus status,
    std::optional<std::vector<blink::mojom::IdentityUserInfoPtr>> user_info) {
  auto it = user_info_requests_.find(request);
  // The request may not be found if the completion is invoked from the
  // RequestService destructor. The destructor clears `user_info_requests_`,
  // which destroys the UserInfoRequests it contains. The
  // UserInfoRequest destructor invokes this callback.
  if (it == user_info_requests_.end() &&
      status == blink::mojom::RequestUserInfoStatus::kSuccess) {
    NOTREACHED() << "The successful user info request is nowhere to be found";
  }
  // Extract the request from the set first to prevent UAF if the callback
  // synchronously destroys `this` (RequestService).
  std::unique_ptr<UserInfoRequest> holder;
  if (it != user_info_requests_.end()) {
    holder = std::move(const_cast<std::unique_ptr<UserInfoRequest>&>(*it));
    user_info_requests_.erase(it);
  }

  if (status == blink::mojom::RequestUserInfoStatus::kSuccess) {
    DCHECK(user_info.has_value());
    std::move(callback).Run(blink::mojom::RequestUserInfoResult::NewUserInfo(
        std::move(user_info.value())));
  } else {
    std::move(callback).Run(
        blink::mojom::RequestUserInfoResult::NewStatus(status));
  }
}

void RequestService::Disconnect(
    blink::mojom::IdentityCredentialDisconnectOptionsPtr options,
    DisconnectCallback callback) {
  // Enforce identity-credentials-get Permissions Policy browser-side.
  // The renderer checks this, but a compromised renderer can bypass it.
  if (!render_frame_host().IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kIdentityCredentialsGet)) {
    mojo::ReportBadMessage(
        "identity-credentials-get permissions policy not enabled");
    std::move(callback).Run(blink::mojom::DisconnectStatus::kError);
    return;
  }

  std::unique_ptr<Metrics> disconnect_metrics = CreateFedCmMetrics();
  if (disconnect_request_) {
    // Since we do not send any fetches in this case, consider the request to be
    // instant, e.g. duration is 0.
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        GetDisconnectConsoleErrorMessage(DisconnectStatus::kTooManyRequests));
    disconnect_metrics->RecordDisconnectMetrics(
        DisconnectStatus::kTooManyRequests, std::nullopt,
        ComputeRequesterFrameType(
            render_frame_host(), render_frame_host().GetLastCommittedOrigin(),
            render_frame_host().GetMainFrame()->GetLastCommittedOrigin()),
        options->config->config_url);
    std::move(callback).Run(
        blink::mojom::DisconnectStatus::kErrorTooManyRequests);
    return;
  }

  bool intercept = false;
  bool should_complete_request_immediately = false;
  devtools_instrumentation::WillSendFedCmRequest(
      render_frame_host(), &intercept, &should_complete_request_immediately);

  auto network_manager = CreateNetworkManager();

  disconnect_request_ = DisconnectRequest::Create(
      std::move(network_manager), permission_delegate_, &render_frame_host(),
      std::move(disconnect_metrics), std::move(options));
  DisconnectRequest* disconnect_request_ptr = disconnect_request_.get();

  disconnect_request_ptr->SetCallbackAndStart(
      base::BindOnce(&RequestService::CompleteDisconnectRequest,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      api_permission_delegate_);
}

void RequestService::CompleteDisconnectRequest(
    DisconnectCallback callback,
    blink::mojom::DisconnectStatus status) {
  CHECK(disconnect_request_);
  std::move(callback).Run(status);
  disconnect_request_.reset();
}

std::unique_ptr<Metrics> RequestService::CreateFedCmMetrics() {
  // Ensure the lifecycle state as GetPageUkmSourceId doesn't support the
  // prerendering page. As FederatedRequestService runs behind the
  // BrowserInterfaceBinders, the service doesn't receive any request while
  // prerendering, and the CHECK should always meet the condition.
  CHECK(!render_frame_host().IsInLifecycleState(
      RenderFrameHost::LifecycleState::kPrerendering));
  return std::make_unique<Metrics>(render_frame_host().GetPageUkmSourceId());
}

}  // namespace webid
}  // namespace content
