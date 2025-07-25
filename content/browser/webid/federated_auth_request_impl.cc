// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include <random>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/bad_message.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webid/digital_credentials/digital_credential_provider.h"
#include "content/browser/webid/fake_identity_request_dialog_controller.h"
#include "content/browser/webid/federated_auth_request_page_data.h"
#include "content/browser/webid/federated_auth_revoke_request.h"
#include "content/browser/webid/federated_auth_user_info_request.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/identity_registry.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/federated_identity_auto_reauthn_permission_context_delegate.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_visibility_state.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

using base::Value;
using blink::mojom::FederatedAuthRequestResult;
using blink::mojom::IdentityProviderConfig;
using blink::mojom::IdentityProviderConfigPtr;
using blink::mojom::IdentityProviderGetParametersPtr;
using blink::mojom::IdentityProviderRequestOptions;
using blink::mojom::IdentityProviderRequestOptionsPtr;
using blink::mojom::LogoutRpsStatus;
using blink::mojom::RequestTokenStatus;
using blink::mojom::RequestUserInfoStatus;
using blink::mojom::RevokeStatus;
using FederatedApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using RevokeStatusForMetrics = content::FedCmRevokeStatus;
using TokenStatus = content::FedCmRequestIdTokenStatus;
using SignInStateMatchStatus = content::FedCmSignInStateMatchStatus;
using TokenResponseType =
    content::IdpNetworkRequestManager::FedCmTokenResponseType;
using ErrorDialogType = content::IdpNetworkRequestManager::FedCmErrorDialogType;
using ErrorUrlType = content::IdpNetworkRequestManager::FedCmErrorUrlType;
using LoginState = content::IdentityRequestAccount::LoginState;
using SignInMode = content::IdentityRequestAccount::SignInMode;
using ErrorDialogResult = content::FedCmErrorDialogResult;
using CompleteRequestWithErrorCallback =
    base::OnceCallback<void(blink::mojom::FederatedAuthRequestResult,
                            absl::optional<content::FedCmRequestIdTokenStatus>,
                            absl::optional<TokenError> token_error,
                            bool)>;

namespace content {

namespace {
static constexpr base::TimeDelta kDefaultTokenRequestDelay = base::Seconds(3);
static constexpr base::TimeDelta kMaxRejectionTime = base::Seconds(60);

// Users spend less time on Android to dismiss the UI. Given the difference, we
// use two set of values. The values are calculated based on UMA data to follow
// lognormal distribution.
#if BUILDFLAG(IS_ANDROID)
static constexpr double kRejectionLogNormalMu = 7.4;
static constexpr double kRejectionLogNormalSigma = 1.24;
#else
static constexpr double kRejectionLogNormalMu = 8.6;
static constexpr double kRejectionLogNormalSigma = 1.4;
#endif  // BUILDFLAG(IS_ANDROID)

std::string ComputeUrlEncodedTokenPostData(
    const std::string& client_id,
    const std::string& nonce,
    const std::string& account_id,
    bool is_sign_in,
    bool is_auto_reauthn,
    const std::vector<std::string>& scope,
    const std::vector<std::string>& responseType,
    const base::flat_map<std::string, std::string>& params) {
  std::string query;
  if (!client_id.empty()) {
    query +=
        "client_id=" + base::EscapeUrlEncodedData(client_id, /*use_plus=*/true);
  }

  if (!nonce.empty()) {
    if (!query.empty()) {
      query += "&";
    }
    query += "nonce=" + base::EscapeUrlEncodedData(nonce, /*use_plus=*/true);
  }

  if (!account_id.empty()) {
    if (!query.empty()) {
      query += "&";
    }
    query += "account_id=" +
             base::EscapeUrlEncodedData(account_id, /*use_plus=*/true);
  }
  // For new users signing up, we show some disclosure text to remind them about
  // data sharing between IDP and RP. For returning users signing in, such
  // disclosure text is not necessary. This field indicates in the request
  // whether the user has been shown such disclosure text.
  std::string disclosure_text_shown = is_sign_in ? "false" : "true";
  if (!query.empty()) {
    query += "&";
  }
  query += "disclosure_text_shown=" + disclosure_text_shown;

  if (IsFedCmAutoSelectedFlagEnabled()) {
    // Shares with IdP that whether the identity credential was automatically
    // selected. This could help developers to better comprehend the token
    // request and segment metrics accordingly.
    std::string is_auto_selected = is_auto_reauthn ? "true" : "false";
    if (!query.empty()) {
      query += "&";
    }
    query += "is_auto_selected=" + is_auto_selected;
  }

  if (IsFedCmAuthzEnabled()) {
    // We keep the scope and response_type parameters consistenct with the OIDC
    // spec [1] to the extent that we can:
    //
    // - They are an arrays of strings, separated by spaces
    // - We use the singular (e.g. "scope") as opposed to the plural
    //   (e.g. "scopes")
    //
    // We do, however, use a different escaping character for spaces: "+"
    // rather than the "%20" to make it consitent with the other
    // parameters in the FedCM spec.
    //
    // [1] https://openid.net/specs/openid-connect-core-1_0.html#ScopeClaims
    if (!scope.empty()) {
      query += "&scope=" + base::EscapeUrlEncodedData(
                               base::JoinString(scope, " "), /*use_plus=*/true);
    }
    if (!responseType.empty()) {
      query += "&response_type=" +
               base::EscapeUrlEncodedData(base::JoinString(responseType, " "),
                                          /*use_plus=*/true);
    }
    for (const auto& pair : params) {
      // TODO(crbug.com/1429083): Should we use a prefix with these custom
      // parameters so that they don't collide with the standard ones?
      query += "&" + base::EscapeUrlEncodedData(pair.first, /*use_plus=*/true) +
               "=" + base::EscapeUrlEncodedData(pair.second, /*use_plus=*/true);
    }
  }

  return query;
}

RequestTokenStatus FederatedAuthRequestResultToRequestTokenStatus(
    FederatedAuthRequestResult result) {
  // Avoids exposing to renderer detailed error messages which may leak cross
  // site information to the API call site.
  switch (result) {
    case FederatedAuthRequestResult::kSuccess: {
      return RequestTokenStatus::kSuccess;
    }
    case FederatedAuthRequestResult::kErrorTooManyRequests: {
      return RequestTokenStatus::kErrorTooManyRequests;
    }
    case FederatedAuthRequestResult::kErrorCanceled: {
      return RequestTokenStatus::kErrorCanceled;
    }
    case FederatedAuthRequestResult::kShouldEmbargo:
    case FederatedAuthRequestResult::kErrorDisabledInSettings:
    case FederatedAuthRequestResult::kErrorFetchingWellKnownHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingWellKnownNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingWellKnownInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingWellKnownListEmpty:
    case FederatedAuthRequestResult::kErrorFetchingWellKnownInvalidContentType:
    case FederatedAuthRequestResult::kErrorConfigNotInWellKnown:
    case FederatedAuthRequestResult::kErrorWellKnownTooBig:
    case FederatedAuthRequestResult::kErrorFetchingConfigHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingConfigNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingConfigInvalidContentType:
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataNoResponse:
    case FederatedAuthRequestResult::
        kErrorFetchingClientMetadataInvalidResponse:
    case FederatedAuthRequestResult::
        kErrorFetchingClientMetadataInvalidContentType:
    case FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingAccountsListEmpty:
    case FederatedAuthRequestResult::kErrorFetchingAccountsInvalidContentType:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenIdpErrorResponse:
    case FederatedAuthRequestResult::
        kErrorFetchingIdTokenCrossSiteIdpErrorResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidContentType:
    case FederatedAuthRequestResult::kErrorRpPageNotVisible:
    case FederatedAuthRequestResult::kErrorSilentMediationFailure:
    case FederatedAuthRequestResult::kErrorThirdPartyCookiesBlocked:
    case FederatedAuthRequestResult::kErrorNotSignedInWithIdp:
    case FederatedAuthRequestResult::kError: {
      return RequestTokenStatus::kError;
    }
  }
}

IdpNetworkRequestManager::MetricsEndpointErrorCode
FederatedAuthRequestResultToMetricsEndpointErrorCode(
    blink::mojom::FederatedAuthRequestResult result) {
  switch (result) {
    case FederatedAuthRequestResult::kSuccess: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kNone;
    }
    case FederatedAuthRequestResult::kErrorTooManyRequests:
    case FederatedAuthRequestResult::kErrorCanceled: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kRpFailure;
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingAccountsListEmpty:
    case FederatedAuthRequestResult::kErrorFetchingAccountsInvalidContentType: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kAccountsEndpointInvalidResponse;
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenIdpErrorResponse:
    case FederatedAuthRequestResult::
        kErrorFetchingIdTokenCrossSiteIdpErrorResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidContentType: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kTokenEndpointInvalidResponse;
    }
    case FederatedAuthRequestResult::kShouldEmbargo:
    case FederatedAuthRequestResult::kErrorDisabledInSettings:
    case FederatedAuthRequestResult::kErrorThirdPartyCookiesBlocked:
    case FederatedAuthRequestResult::kErrorRpPageNotVisible:
    case FederatedAuthRequestResult::kErrorNotSignedInWithIdp: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kUserFailure;
    }
    case FederatedAuthRequestResult::kErrorFetchingWellKnownHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingWellKnownNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingConfigHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingConfigNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kIdpServerUnavailable;
    }
    case FederatedAuthRequestResult::kErrorConfigNotInWellKnown:
    case FederatedAuthRequestResult::kErrorWellKnownTooBig: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kManifestError;
    }
    case FederatedAuthRequestResult::kErrorFetchingWellKnownListEmpty:
    case FederatedAuthRequestResult::kErrorFetchingWellKnownInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse:
    case FederatedAuthRequestResult::
        kErrorFetchingClientMetadataInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingWellKnownInvalidContentType:
    case FederatedAuthRequestResult::kErrorFetchingConfigInvalidContentType:
    case FederatedAuthRequestResult::
        kErrorFetchingClientMetadataInvalidContentType: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::
          kIdpServerInvalidResponse;
    }
    case FederatedAuthRequestResult::kError:
    case FederatedAuthRequestResult::kErrorSilentMediationFailure: {
      return IdpNetworkRequestManager::MetricsEndpointErrorCode::kOther;
    }
  }
}

// The time from when the accounts dialog is shown to when a user explicitly
// closes it follows normal distribution. To make the random failures
// indistinguishable from user declines, we use lognormal distribution to
// generate the random number.
base::TimeDelta GetRandomRejectionTime() {
  base::RandomBitGenerator generator;
  std::lognormal_distribution<double> distribution(kRejectionLogNormalMu,
                                                   kRejectionLogNormalSigma);

  base::TimeDelta rejection_time =
      base::Seconds(distribution(generator) / 1000);

  return std::min(kMaxRejectionTime, rejection_time);
}

std::string FormatOriginForDisplay(const url::Origin& origin) {
  return webid::FormatUrlWithDomain(origin.GetURL(),
                                    /*for_display=*/true);
}

bool ShouldSuppressIdpSigninFailureDialog(
    absl::optional<TokenStatus> token_status) {
  if (!token_status) {
    return false;
  }

  return token_status == TokenStatus::kAborted ||
         token_status == TokenStatus::kUnhandledRequest;
}

FederatedAuthRequestPageData* GetPageData(RenderFrameHost* render_frame_host) {
  return FederatedAuthRequestPageData::GetOrCreateForPage(
      render_frame_host->GetPage());
}

void FilterAccountsWithLoginHint(
    const std::string& login_hint,
    IdpNetworkRequestManager::AccountList& accounts) {
  if (login_hint.empty()) {
    return;
  }

  // Remove all accounts whose ID and whose email do not match the login hint.
  // Note that it is technically possible for us to end up with more than one
  // account afterwards, in which case the multiple account chooser would be
  // shown.
  auto filter = [&login_hint](const IdentityRequestAccount& account) {
    return !base::Contains(account.login_hints, login_hint);
  };
  base::EraseIf(accounts, filter);
  FedCmMetrics::NumAccounts num_matching = FedCmMetrics::NumAccounts::kZero;
  if (accounts.size() == 1u) {
    num_matching = FedCmMetrics::NumAccounts::kOne;
  } else if (accounts.size() > 1u) {
    num_matching = FedCmMetrics::NumAccounts::kMultiple;
  }
  base::UmaHistogramEnumeration("Blink.FedCm.LoginHint.NumMatchingAccounts",
                                num_matching);
}

void FilterAccountsWithDomainHint(
    const std::string& domain_hint,
    IdpNetworkRequestManager::AccountList& accounts) {
  if (domain_hint.empty()) {
    return;
  }

  if (domain_hint == FederatedAuthRequestImpl::kWildcardDomainHint) {
    auto filter = [](const IdentityRequestAccount& account) {
      return account.domain_hints.empty();
    };
    base::EraseIf(accounts, filter);
  } else {
    auto filter = [&domain_hint](const IdentityRequestAccount& account) {
      return !base::Contains(account.domain_hints, domain_hint);
    };
    base::EraseIf(accounts, filter);
  }
}

std::unique_ptr<FedCmMetrics> CreateFedCmMetrics(
    const GURL& provider_config_url,
    const ukm::SourceId& source_id,
    bool is_disabled) {
  return std::make_unique<FedCmMetrics>(provider_config_url, source_id,
                                        base::RandInt(1, 1 << 30), is_disabled);
}

std::string GetTopFrameOriginForDisplay(const url::Origin& top_frame_origin) {
  return FormatOriginForDisplay(top_frame_origin);
}

absl::optional<std::string> GetIframeOriginForDisplay(
    const url::Origin& top_frame_origin,
    const url::Origin& iframe_origin,
    CompleteRequestWithErrorCallback callback) {
  // TOOD(crbug.com/1448566): clean up the logic to allow 3 domains.
  bool exclude_iframe = true;
  absl::optional<std::string> iframe_for_display = absl::nullopt;

  if (!exclude_iframe) {
    iframe_for_display = FormatOriginForDisplay(iframe_origin);

    // TODO(crbug.com/1422040): Decide what to do if we want to include iframe
    // domain in the dialog but iframe_for_display is opaque.
    if (iframe_origin.opaque()) {
      std::move(callback).Run(FederatedAuthRequestResult::kError,
                              /*token_status=*/absl::nullopt,
                              /*token_error=*/absl::nullopt,
                              /*should_delay_callback=*/false);
    }
  }

  return iframe_for_display;
}

bool IsFrameVisible(RenderFrameHost* frame) {
  return frame && frame->IsActive() &&
         frame->GetVisibilityState() == content::PageVisibilityState::kVisible;
}

}  // namespace

FederatedAuthRequestImpl::FetchData::FetchData() = default;
FederatedAuthRequestImpl::FetchData::~FetchData() = default;

FederatedAuthRequestImpl::IdentityProviderGetInfo::IdentityProviderGetInfo(
    blink::mojom::IdentityProviderRequestOptionsPtr provider,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode)
    : provider(std::move(provider)), rp_context(rp_context), rp_mode(rp_mode) {}

FederatedAuthRequestImpl::IdentityProviderGetInfo::~IdentityProviderGetInfo() =
    default;
FederatedAuthRequestImpl::IdentityProviderGetInfo::IdentityProviderGetInfo(
    const IdentityProviderGetInfo& other) {
  *this = other;
}

FederatedAuthRequestImpl::IdentityProviderGetInfo&
FederatedAuthRequestImpl::IdentityProviderGetInfo::operator=(
    const IdentityProviderGetInfo& other) {
  provider = other.provider->Clone();
  rp_context = other.rp_context;
  rp_mode = other.rp_mode;
  return *this;
}

FederatedAuthRequestImpl::IdentityProviderInfo::IdentityProviderInfo(
    const blink::mojom::IdentityProviderRequestOptionsPtr& provider,
    IdpNetworkRequestManager::Endpoints endpoints,
    IdentityProviderMetadata metadata,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode)
    : provider(provider->Clone()),
      endpoints(std::move(endpoints)),
      metadata(std::move(metadata)),
      rp_context(rp_context),
      rp_mode(rp_mode) {}

FederatedAuthRequestImpl::IdentityProviderInfo::~IdentityProviderInfo() =
    default;
FederatedAuthRequestImpl::IdentityProviderInfo::IdentityProviderInfo(
    const IdentityProviderInfo& other) {
  provider = other.provider->Clone();
  endpoints = other.endpoints;
  metadata = other.metadata;
  has_failing_idp_signin_status = other.has_failing_idp_signin_status;
  rp_context = other.rp_context;
  rp_mode = other.rp_mode;
  data = other.data;
}

FederatedAuthRequestImpl::FederatedAuthRequestImpl(
    RenderFrameHost& host,
    FederatedIdentityApiPermissionContextDelegate* api_permission_context,
    FederatedIdentityAutoReauthnPermissionContextDelegate*
        auto_reauthn_permission_context,
    FederatedIdentityPermissionContextDelegate* permission_context,
    IdentityRegistry* identity_registry,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver)
    : DocumentService(host, std::move(receiver)),
      api_permission_delegate_(api_permission_context),
      auto_reauthn_permission_delegate_(auto_reauthn_permission_context),
      permission_delegate_(permission_context),
      identity_registry_(identity_registry),
      token_request_delay_(kDefaultTokenRequestDelay) {}

FederatedAuthRequestImpl::~FederatedAuthRequestImpl() {
  // Ensures key data members are destructed in proper order and resolves any
  // pending promise.
  if (auth_request_token_callback_) {
    DCHECK(!logout_callback_);
    CompleteRequestWithError(FederatedAuthRequestResult::kError,
                             TokenStatus::kUnhandledRequest,
                             /*token_error=*/absl::nullopt,
                             /*should_delay_callback=*/false);
  }
  // Calls |FederatedAuthUserInfoRequest|'s destructor to complete the user
  // info request. This is needed because otherwise some resources like
  // `fedcm_metrics_` may no longer be usable when the destructor get invoked
  // naturally.
  user_info_requests_.clear();

  // Calls |FederatedAuthRevokeRequest|'s destructor to complete the revocation
  // request. This is needed because otherwise some resources like
  // `fedcm_metrics_` may no longer be usable when the destructor get invoked
  // naturally.
  revoke_request_.reset();

  if (logout_callback_) {
    // We do not complete the logout request, so unset the
    // PendingWebIdentityRequest on the Page so that other frames in the
    // same Page may still trigger new requests after the current
    // RenderFrameHost is destroyed.
    GetPageData(&render_frame_host())->SetPendingWebIdentityRequest(nullptr);
  }

  // Since FederatedAuthRequestImpl is a subclass of
  // DocumentService<blink::mojom::FederatedAuthRequest>, it only lives as long
  // as the current document.
  if (num_requests_ > 0 && fedcm_metrics_) {
    fedcm_metrics_->RecordNumRequestsPerDocument(num_requests_);
  }
}

// static
void FederatedAuthRequestImpl::Create(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver) {
  CHECK(host);

  BrowserContext* browser_context = host->GetBrowserContext();
  raw_ptr<FederatedIdentityApiPermissionContextDelegate>
      api_permission_context =
          browser_context->GetFederatedIdentityApiPermissionContext();
  raw_ptr<FederatedIdentityAutoReauthnPermissionContextDelegate>
      auto_reauthn_permission_context =
          browser_context->GetFederatedIdentityAutoReauthnPermissionContext();
  raw_ptr<FederatedIdentityPermissionContextDelegate> permission_context =
      browser_context->GetFederatedIdentityPermissionContext();
  raw_ptr<IdentityRegistry> identity_registry =
      IdentityRegistry::FromWebContents(WebContents::FromRenderFrameHost(host));

  if (!api_permission_context || !auto_reauthn_permission_context ||
      !permission_context) {
    return;
  }

  // FederatedAuthRequestImpl owns itself. It will self-destruct when a mojo
  // interface error occurs, the RenderFrameHost is deleted, or the
  // RenderFrameHost navigates to a new document.
  new FederatedAuthRequestImpl(
      *host, api_permission_context, auto_reauthn_permission_context,
      permission_context, identity_registry, std::move(receiver));
}

FederatedAuthRequestImpl& FederatedAuthRequestImpl::CreateForTesting(
    RenderFrameHost& host,
    FederatedIdentityApiPermissionContextDelegate* api_permission_context,
    FederatedIdentityAutoReauthnPermissionContextDelegate*
        auto_reauthn_permission_context,
    FederatedIdentityPermissionContextDelegate* permission_context,
    IdentityRegistry* identity_registry,
    mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver) {
  return *new FederatedAuthRequestImpl(
      host, api_permission_context, auto_reauthn_permission_context,
      permission_context, identity_registry, std::move(receiver));
}

void FederatedAuthRequestImpl::CompleteDigitalCredentialRequest(
    std::string response) {
  if (!digital_credential_provider_) {
    std::move(digital_credential_request_callback_)
        .Run(RequestTokenStatus::kError, absl::nullopt, "", /*error=*/nullptr,
             /*is_auto_selected=*/false);
    return;
  }

  if (!response.empty()) {
    std::move(digital_credential_request_callback_)
        .Run(RequestTokenStatus::kSuccess, absl::nullopt, response,
             /*error=*/nullptr,
             /*is_auto_selected=*/false);
  } else {
    std::move(digital_credential_request_callback_)
        .Run(RequestTokenStatus::kError, absl::nullopt, "", /*error=*/nullptr,
             /*is_auto_selected=*/false);
  }
}

base::Value::Dict BuildDigitalCredentialRequest(
    blink::mojom::DigitalCredentialProviderPtr provider) {
  auto formats = Value::List();
  for (auto& format : provider->selector->format) {
    formats.Append(format);
  }

  auto params = Value::Dict();
  for (const auto& pair : provider->params) {
    params.Set(pair.first, pair.second);
  }

  auto fields = Value::List();

  if (provider->selector->doctype) {
    auto doctype = Value::Dict();
    doctype.Set("name", "doctype");
    doctype.Set("equals", provider->selector->doctype.value());
    fields.Append(std::move(doctype));
  }

  for (auto& value : provider->selector->fields) {
    auto field = Value::Dict();
    field.Set("name", value->name);
    if (value->equals) {
      field.Set("equals", value->equals.value());
    }
    fields.Append(std::move(field));
  }

  return Value::Dict().Set(
      "providers", Value::List().Append(
                       Value::Dict()
                           .Set("responseFormat", std::move(formats))
                           .Set("params", std::move(params))
                           .Set("selector", Value::Dict().Set(
                                                "fields", std::move(fields)))));
}

void FederatedAuthRequestImpl::RequestToken(
    std::vector<IdentityProviderGetParametersPtr> idp_get_params_ptrs,
    MediationRequirement requirement,
    RequestTokenCallback callback) {
  // idp_get_params_ptrs should never be empty since it is the renderer-side
  // code which populates it.
  if (idp_get_params_ptrs.empty()) {
    mojo::ReportBadMessage("idp_get_params_ptrs is empty.");
    return;
  }
  // This could only happen with a compromised renderer process. We ensure that
  // the provider list size is > 0 on the renderer side at the beginning of
  // parsing |IdentityCredentialRequestOptions|.
  for (auto& idp_get_params_ptr : idp_get_params_ptrs) {
    if (idp_get_params_ptr->providers.size() == 0) {
      mojo::ReportBadMessage("The provider list should not be empty.");
      return;
    }
  }

  if (!render_frame_host().GetPage().IsPrimary()) {
    mojo::ReportBadMessage(
        "FedCM should not be allowed in nested frame trees.");
    return;
  }
  // It should not be possible to receive multiple IDPs when the
  // `kFedCmMultipleIdentityProviders` flag is disabled. But such a message
  // could be received from a compromised renderer.
  const bool is_multi_idp_input = idp_get_params_ptrs.size() > 1u ||
                                  idp_get_params_ptrs[0]->providers.size() > 1u;
  if (is_multi_idp_input && !IsFedCmMultipleIdentityProvidersEnabled()) {
    std::move(callback).Run(RequestTokenStatus::kError, absl::nullopt, "",
                            /*error=*/nullptr,
                            /*is_auto_selected=*/false);
    return;
  }

  if (idp_get_params_ptrs[0]->providers[0]->is_holder()) {
    if (!IsWebIdentityDigitalCredentialsEnabled() ||
        IsFedCmMultipleIdentityProvidersEnabled()) {
      // TODO(https://crbug.com/1416939): Support calling the Digital
      // Credentials
      //  API with the Multi IdP API support.
      std::move(callback).Run(RequestTokenStatus::kError, absl::nullopt, "",
                              /*error=*/nullptr,
                              /*is_auto_selected=*/false);
      return;
    }

    if (digital_credential_request_callback_) {
      // Similar to the token request, only allow one in-flight wallet request.
      // TODO(https://crbug.com/1416939): Reconcile with federated identity
      // requests.
      std::move(callback).Run(RequestTokenStatus::kErrorTooManyRequests,
                              absl::nullopt, "", /*error=*/nullptr,
                              /*is_auto_selected=*/false);
      return;
    }

    digital_credential_request_callback_ = std::move(callback);
    // digital_credential_provider_ is not destroyed after a successful wallet
    // request so we need to have the nullcheck to avoid duplicated creation.
    if (!digital_credential_provider_) {
      digital_credential_provider_ = CreateDigitalCredentialProvider();
    }
    if (!digital_credential_provider_) {
      std::move(digital_credential_request_callback_)
          .Run(RequestTokenStatus::kError, absl::nullopt, "", /*error=*/nullptr,
               /*is_auto_selected=*/false);
      return;
    }

    auto digital_credential =
        std::move(idp_get_params_ptrs[0]->providers[0]->get_holder());

    auto request = BuildDigitalCredentialRequest(std::move(digital_credential));

    digital_credential_provider_->RequestDigitalCredential(
        WebContents::FromRenderFrameHost(&render_frame_host()), origin(),
        request,
        base::BindOnce(
            &FederatedAuthRequestImpl::CompleteDigitalCredentialRequest,
            weak_ptr_factory_.GetWeakPtr()));

    // TODO(https://crbug.com/1416939): rather than returning early,
    // we would ultimately like to make the wallet response reconcile with the
    // federated identities, so that they can be presented to the user in an
    // unified manner.
    return;
  }

  if (!fedcm_metrics_) {
    // Ensure the lifecycle state as GetPageUkmSourceId doesn't support the
    // prerendering page. As FederatedAithRequest runs behind the
    // BrowserInterfaceBinders, the service doesn't receive any request while
    // prerendering, and the CHECK should always meet the condition.
    CHECK(!render_frame_host().IsInLifecycleState(
        RenderFrameHost::LifecycleState::kPrerendering));

    // TODO(crbug.com/1307709): Handle FedCmMetrics for multiple IDPs.
    fedcm_metrics_ =
        CreateFedCmMetrics(idp_get_params_ptrs[0]
                               ->providers[0]
                               ->get_federated()
                               ->config->config_url,
                           render_frame_host().GetPageUkmSourceId(),
                           /*is_disabled=*/idp_get_params_ptrs.size() > 1);
  }

  if (HasPendingRequest()) {
    fedcm_metrics_->RecordRequestTokenStatus(TokenStatus::kTooManyRequests,
                                             requirement);

    AddDevToolsIssue(
        blink::mojom::FederatedAuthRequestResult::kErrorTooManyRequests);
    AddConsoleErrorMessage(
        blink::mojom::FederatedAuthRequestResult::kErrorTooManyRequests);

    std::move(callback).Run(RequestTokenStatus::kErrorTooManyRequests,
                            absl::nullopt, "", /*error=*/nullptr,
                            /*is_auto_selected=*/false);
    return;
  }

  bool intercept = false;
  bool should_complete_request_immediately = false;
  devtools_instrumentation::WillSendFedCmRequest(
      &render_frame_host(), &intercept, &should_complete_request_immediately);
  should_complete_request_immediately_ =
      (intercept && should_complete_request_immediately) ||
      api_permission_delegate_->ShouldCompleteRequestImmediately();

  mediation_requirement_ = requirement;
  auth_request_token_callback_ = std::move(callback);
  GetPageData(&render_frame_host())->SetPendingWebIdentityRequest(this);
  network_manager_ = CreateNetworkManager();
  request_dialog_controller_ = CreateDialogController();
  start_time_ = base::TimeTicks::Now();

  FederatedApiPermissionStatus permission_status = GetApiPermissionStatus();

  absl::optional<TokenStatus> error_token_status;
  FederatedAuthRequestResult request_result =
      FederatedAuthRequestResult::kError;

  switch (permission_status) {
    case FederatedApiPermissionStatus::BLOCKED_VARIATIONS:
      error_token_status = TokenStatus::kDisabledInFlags;
      break;
    case FederatedApiPermissionStatus::BLOCKED_THIRD_PARTY_COOKIES_BLOCKED:
      error_token_status = TokenStatus::kThirdPartyCookiesBlocked;
      request_result =
          FederatedAuthRequestResult::kErrorThirdPartyCookiesBlocked;
      break;
    case FederatedApiPermissionStatus::BLOCKED_SETTINGS:
      error_token_status = TokenStatus::kDisabledInSettings;
      request_result = FederatedAuthRequestResult::kErrorDisabledInSettings;
      break;
    case FederatedApiPermissionStatus::BLOCKED_EMBARGO:
      error_token_status = TokenStatus::kDisabledEmbargo;
      request_result = FederatedAuthRequestResult::kErrorDisabledInSettings;
      break;
    case FederatedApiPermissionStatus::GRANTED:
      // Intentional fall-through.
      break;
    default:
      NOTREACHED();
      break;
  }

  if (error_token_status) {
    CompleteRequestWithError(request_result, *error_token_status,
                             /*token_error=*/absl::nullopt,
                             /*should_delay_callback=*/true);
    return;
  }

  // This counter measures the number of requests made to FedCM in a document to
  // identify RPs calling FedCM in quick succession. Requests made when FedCM is
  // disabled, when there is a pending FedCM request or for the purpose of
  // wallets or multi-IDP are not counted.
  if (!IsFedCmMultipleIdentityProvidersEnabled()) {
    ++num_requests_;
  }

  std::set<GURL> unique_idps;
  for (auto& idp_get_params_ptr : idp_get_params_ptrs) {
    for (auto& idp_ptr : idp_get_params_ptr->providers) {
      // Throw an error if duplicate IDPs are specified.
      const bool is_unique_idp =
          unique_idps.insert(idp_ptr->get_federated()->config->config_url)
              .second;
      if (!is_unique_idp) {
        CompleteRequestWithError(FederatedAuthRequestResult::kError,
                                 /*token_status=*/absl::nullopt,
                                 /*token_error=*/absl::nullopt,
                                 /*should_delay_callback=*/false);
        return;
      }

      url::Origin idp_origin =
          url::Origin::Create(idp_ptr->get_federated()->config->config_url);
      if (!network::IsOriginPotentiallyTrustworthy(idp_origin)) {
        CompleteRequestWithError(FederatedAuthRequestResult::kError,
                                 TokenStatus::kIdpNotPotentiallyTrustworthy,
                                 /*token_error=*/absl::nullopt,
                                 /*should_delay_callback=*/false);
        return;
      }

      // TODO(crbug.com/1382545): Handle
      // ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp in the multi
      // IDP use case.
      bool has_failing_idp_signin_status =
          webid::ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
              render_frame_host(), idp_ptr->get_federated()->config->config_url,
              permission_delegate_);

      if (has_failing_idp_signin_status &&
          webid::GetIdpSigninStatusMode(render_frame_host(), idp_origin) ==
              FedCmIdpSigninStatusMode::ENABLED) {
        if (idp_get_params_ptr->mode == blink::mojom::RpMode::kWidget) {
          // If the user is known to be signed-out and the RP is request
          // a widget, we fail the request early before fetching anything.
          CompleteRequestWithError(
              FederatedAuthRequestResult::kErrorNotSignedInWithIdp,
              TokenStatus::kNotSignedInWithIdp,
              /*token_error=*/absl::nullopt,
              /*should_delay_callback=*/true);
          return;
        } else if (idp_get_params_ptr->mode == blink::mojom::RpMode::kButton) {
          // Only a compromised renderer can set mode = button without the
          // AuthZ flag enabled (which controls the JS WebIDL), so we crash
          // here if we ever get to this situation.
          CHECK(IsFedCmAuthzEnabled());
          if (!render_frame_host().HasTransientUserActivation()) {
            // The button flow requires user activation and a valid login_url.
            // TODO(crbug.com/1487270): use a more specific error.
            CompleteRequestWithError(FederatedAuthRequestResult::kError,
                                     TokenStatus::kUnhandledRequest,
                                     /*token_error=*/absl::nullopt,
                                     /*should_delay_callback=*/true);
            return;
          }
        }
      }
      // TODO(crbug.com/1383384): Handle auto_reauthn_ for multi IDP.
      if (ShouldFailBeforeFetchingAccounts(
              idp_ptr->get_federated()->config->config_url)) {
        CompleteRequestWithError(
            FederatedAuthRequestResult::kErrorSilentMediationFailure,
            TokenStatus::kSilentMediationFailure,
            /*token_error=*/absl::nullopt,
            /*should_delay_callback=*/false);
        return;
      }
    }
  }

  for (auto& idp_get_params_ptr : idp_get_params_ptrs) {
    for (auto& idp_ptr : idp_get_params_ptr->providers) {
      idp_order_.push_back(idp_ptr->get_federated()->config->config_url);
      blink::mojom::RpContext rp_context = idp_get_params_ptr->context;
      blink::mojom::RpMode rp_mode = idp_get_params_ptr->mode;
      const GURL& idp_config_url = idp_ptr->get_federated()->config->config_url;
      token_request_get_infos_.emplace(
          idp_config_url,
          IdentityProviderGetInfo(std::move(idp_ptr->get_federated()),
                                  rp_context, rp_mode));
    }
  }

  FetchEndpointsForIdps(std::move(unique_idps), /*for_idp_signin=*/false);
}

void FederatedAuthRequestImpl::RequestUserInfo(
    blink::mojom::IdentityProviderConfigPtr provider,
    RequestUserInfoCallback callback) {
  if (!render_frame_host().GetPage().IsPrimary()) {
    mojo::ReportBadMessage(
        "FedCM should not be allowed in nested frame trees.");
    return;
  }

  if (!fedcm_metrics_) {
    // Ensure the lifecycle state as GetPageUkmSourceId doesn't support the
    // prerendering page. As FederatedAithRequest runs behind the
    // BrowserInterfaceBinders, the service doesn't receive any request while
    // prerendering, and the CHECK should always meet the condition.
    CHECK(!render_frame_host().IsInLifecycleState(
        RenderFrameHost::LifecycleState::kPrerendering));
    fedcm_metrics_ = CreateFedCmMetrics(
        provider->config_url, render_frame_host().GetPageUkmSourceId(),
        /*is_disabled=*/false);
  }

  auto network_manager = IdpNetworkRequestManager::Create(
      static_cast<RenderFrameHostImpl*>(&render_frame_host()));
  auto user_info_request = FederatedAuthUserInfoRequest::Create(
      std::move(network_manager), permission_delegate_.get(),
      &render_frame_host(), fedcm_metrics_.get(), std::move(provider));
  FederatedAuthUserInfoRequest* user_info_request_ptr = user_info_request.get();
  user_info_requests_.insert(std::move(user_info_request));

  user_info_request_ptr->SetCallbackAndStart(
      base::BindOnce(&FederatedAuthRequestImpl::CompleteUserInfoRequest,
                     weak_ptr_factory_.GetWeakPtr(), user_info_request_ptr,
                     std::move(callback)),
      api_permission_delegate_.get());
}

void FederatedAuthRequestImpl::CancelTokenRequest() {
  if (!auth_request_token_callback_) {
    mojo::ReportBadMessage(
        "The abort controller must be used after initiating a token request.");
    return;
  }

  // Dialog will be hidden by the destructor for request_dialog_controller_,
  // triggered by CompleteRequest.

  CompleteRequestWithError(FederatedAuthRequestResult::kErrorCanceled,
                           TokenStatus::kAborted,
                           /*token_error=*/absl::nullopt,
                           /*should_delay_callback=*/false);
}

// TODO(kenrb): Depending on how this code evolves, it might make sense to
// spin session management code into its own service. The prohibition on
// making authentication requests and logout requests at the same time, while
// not problematic for any plausible use case, need not be strictly necessary
// if there is a good way to not have to resource contention between requests.
// https://crbug.com/1200581
void FederatedAuthRequestImpl::LogoutRps(
    std::vector<blink::mojom::LogoutRpsRequestPtr> logout_requests,
    LogoutRpsCallback callback) {
  if (HasPendingRequest()) {
    std::move(callback).Run(LogoutRpsStatus::kErrorTooManyRequests);
    return;
  }

  DCHECK(logout_requests_.empty());

  logout_callback_ = std::move(callback);
  GetPageData(&render_frame_host())->SetPendingWebIdentityRequest(this);

  if (logout_requests.empty()) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  if (base::ranges::any_of(logout_requests, [](auto& request) {
        return !request->url.is_valid();
      })) {
    bad_message::ReceivedBadMessage(render_frame_host().GetProcess(),
                                    bad_message::FARI_LOGOUT_BAD_ENDPOINT);
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  for (auto& request : logout_requests) {
    logout_requests_.push(std::move(request));
  }

  if (!network::IsOriginPotentiallyTrustworthy(origin())) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  network_manager_ = CreateNetworkManager();

  if (!IsFedCmIdpSignoutEnabled()) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  if (GetApiPermissionStatus() != FederatedApiPermissionStatus::GRANTED) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  // TODO(kenrb): These should be parallelized rather than being dispatched
  // serially. https://crbug.com/1200581.
  DispatchOneLogout();
}

void FederatedAuthRequestImpl::ResolveTokenRequest(
    const std::string& token,
    ResolveTokenRequestCallback callback) {
  if (!IsFedCmAuthzEnabled()) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(crbug.com/1456368): notify Android UI about token request being
  // resolved.
  if (!identity_registry_) {
    std::move(callback).Run(false);
    return;
  }

  bool accepted = identity_registry_->NotifyResolve(origin(), token);
  std::move(callback).Run(accepted);
}

void FederatedAuthRequestImpl::SetIdpSigninStatus(
    const url::Origin& idp_origin,
    blink::mojom::IdpSigninStatus status) {
  if (render_frame_host().IsNestedWithinFencedFrame()) {
    RecordSetLoginStatusIgnoredReason(
        FedCmSetLoginStatusIgnoredReason::kInFencedFrame);
    return;
  }
  // We only allow setting the IDP signin status when the subresource is loaded
  // from the same origin as the document, and the document is same-origin with
  // all ancestors. This is to protect from an RP embedding a tracker resource
  // that would set this signin status for the tracker, enabling the FedCM
  // request.
  if (!webid::IsSameOriginWithAncestors(idp_origin, &render_frame_host())) {
    RecordSetLoginStatusIgnoredReason(
        FedCmSetLoginStatusIgnoredReason::kCrossOrigin);
    return;
  }
  permission_delegate_->SetIdpSigninStatus(
      idp_origin, status == blink::mojom::IdpSigninStatus::kSignedIn);
}

void FederatedAuthRequestImpl::RegisterIdP(const GURL& idp,
                                           RegisterIdPCallback callback) {
  if (!IsFedCmIdPRegistrationEnabled()) {
    std::move(callback).Run(false);
    return;
  }
  if (!origin().IsSameOriginWith(url::Origin::Create(idp))) {
    std::move(callback).Run(false);
    return;
  }
  // TODO(crbug.com/1406698): prompt the user for permission.
  permission_delegate_->RegisterIdP(idp);
  std::move(callback).Run(true);
}

void FederatedAuthRequestImpl::UnregisterIdP(const GURL& idp,
                                             UnregisterIdPCallback callback) {
  if (!IsFedCmIdPRegistrationEnabled()) {
    std::move(callback).Run(false);
    return;
  }
  if (!origin().IsSameOriginWith(url::Origin::Create(idp))) {
    std::move(callback).Run(false);
    return;
  }
  permission_delegate_->UnregisterIdP(idp);
  std::move(callback).Run(true);
}

void FederatedAuthRequestImpl::OnIdpSigninStatusChanged(
    const url::Origin& idp_config_origin,
    bool idp_signin_status) {
  if (!idp_signin_status) {
    return;
  }

  for (const auto& [get_idp_config_url, get_info] : token_request_get_infos_) {
    if (url::Origin::Create(get_idp_config_url) == idp_config_origin) {
      permission_delegate_->RemoveIdpSigninStatusObserver(this);
      FetchEndpointsForIdps({get_idp_config_url}, /*for_idp_signin=*/true);
      break;
    }
  }
}

bool FederatedAuthRequestImpl::HasPendingRequest() const {
  bool has_pending_request =
      GetPageData(&render_frame_host())->PendingWebIdentityRequest() != nullptr;
  DCHECK(has_pending_request ||
         (!auth_request_token_callback_ && !logout_callback_));
  return has_pending_request;
}

void FederatedAuthRequestImpl::FetchEndpointsForIdps(
    const std::set<GURL>& idp_config_urls,
    bool for_idp_signin) {
  int icon_ideal_size = request_dialog_controller_->GetBrandIconIdealSize();
  int icon_minimum_size = request_dialog_controller_->GetBrandIconMinimumSize();

  {
    std::set<GURL> pending_idps = std::move(fetch_data_.pending_idps);
    pending_idps.insert(idp_config_urls.begin(), idp_config_urls.end());
    fetch_data_ = FetchData();
    fetch_data_.pending_idps = std::move(pending_idps);
    fetch_data_.for_idp_signin = for_idp_signin;
  }

  provider_fetcher_ = std::make_unique<FederatedProviderFetcher>(
      render_frame_host(), network_manager_.get());
  provider_fetcher_->Start(
      fetch_data_.pending_idps, icon_ideal_size, icon_minimum_size,
      base::BindOnce(&FederatedAuthRequestImpl::OnAllConfigAndWellKnownFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnAllConfigAndWellKnownFetched(
    std::vector<FederatedProviderFetcher::FetchResult> fetch_results) {
  provider_fetcher_.reset();

  for (const FederatedProviderFetcher::FetchResult& fetch_result :
       fetch_results) {
    const GURL& identity_provider_config_url =
        fetch_result.identity_provider_config_url;
    auto get_info_it =
        token_request_get_infos_.find(identity_provider_config_url);
    CHECK(get_info_it != token_request_get_infos_.end());

    metrics_endpoints_[identity_provider_config_url] =
        fetch_result.endpoints.metrics;

    std::unique_ptr<IdentityProviderInfo> idp_info =
        std::make_unique<IdentityProviderInfo>(
            get_info_it->second.provider, std::move(fetch_result.endpoints),
            fetch_result.metadata ? std::move(*fetch_result.metadata)
                                  : IdentityProviderMetadata(),
            get_info_it->second.rp_context, get_info_it->second.rp_mode);

    if (fetch_result.error) {
      const FederatedProviderFetcher::FetchError& fetch_error =
          *fetch_result.error;
      if (fetch_error.additional_console_error_message) {
        render_frame_host().AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kError,
            *fetch_error.additional_console_error_message);
      }
      OnFetchDataForIdpFailed(std::move(idp_info), fetch_error.result,
                              fetch_error.token_status,
                              /*should_delay_callback=*/true);
      continue;
    }

    // Make sure that we don't fetch accounts if the IDP sign-in bit is reset to
    // false during the API call. e.g. by the login/logout HEADER.
    idp_info->has_failing_idp_signin_status =
        webid::ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
            render_frame_host(), identity_provider_config_url,
            permission_delegate_);
    if (idp_info->has_failing_idp_signin_status &&
        webid::GetIdpSigninStatusMode(
            render_frame_host(),
            url::Origin::Create(identity_provider_config_url)) ==
            FedCmIdpSigninStatusMode::ENABLED) {
      // If the user is logged out and we are in a button-mode, allow the user
      // to sign-in to the IdP and return early.
      // TODO(https://crbug.com/1490611): handle the "unknown" status and button
      // flows.
      if (IsFedCmAuthzEnabled() &&
          idp_info->rp_mode == blink::mojom::RpMode::kButton &&
          idp_info->metadata.idp_login_url.is_valid()) {
        // We fail sooner before, but just to double check, we assert that
        // we are inside a user gesture here again.
        CHECK(render_frame_host().HasTransientUserActivation());
        // TODO(crbug.com/1487270): we should probably make idp_signin_url
        // optional instead of empty.
        SignInToIdP(idp_info->metadata.idp_login_url);
        // TODO(https://crbug.com/1487268): handle the button flow and the
        // Multi IdP API (what should happen if you are logged in to some
        // IdPs but not to others).
        return;
      }
      // Do not send metrics for IDP where the user is not signed-in in order
      // to prevent IDP from using the user IP to make a probabilistic model
      // of which websites a user visits.
      idp_info->endpoints.metrics = GURL();

      OnFetchDataForIdpFailed(
          std::move(idp_info),
          FederatedAuthRequestResult::kErrorNotSignedInWithIdp,
          TokenStatus::kNotSignedInWithIdp,
          /*should_delay_callback=*/true);
      continue;
    }

    GURL accounts_endpoint = idp_info->endpoints.accounts;
    std::string client_id = idp_info->provider->config->client_id;
    network_manager_->SendAccountsRequest(
        accounts_endpoint, client_id,
        base::BindOnce(&FederatedAuthRequestImpl::OnAccountsResponseReceived,
                       weak_ptr_factory_.GetWeakPtr(), std::move(idp_info)));
    fedcm_metrics_->RecordAccountsRequestSent();
  }
}

void FederatedAuthRequestImpl::CompleteRevokeRequest(
    RevokeCallback callback,
    blink::mojom::RevokeStatus status) {
  if (!revoke_request_) {
    NOTREACHED() << "The completed revocation request is nowhere to be found";
    return;
  }

  std::move(callback).Run(status);
  revoke_request_.reset();
}

void FederatedAuthRequestImpl::OnClientMetadataResponseReceived(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    const IdpNetworkRequestManager::AccountList& accounts,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::ClientMetadata client_metadata) {
  // TODO(yigu): Clean up the client metadata related errors for metrics and
  // console logs.
  OnFetchDataForIdpSucceeded(std::move(idp_info), accounts, client_metadata);
}

bool HasScope(const std::vector<std::string>& scope, std::string name) {
  auto it = std::find(std::begin(scope), std::end(scope), name);
  if (it == std::end(scope)) {
    return false;
  }
  return true;
}

// static
bool FederatedAuthRequestImpl::ShouldMediateAuthz(
    const std::vector<std::string>& scope) {
  if (!IsFedCmAuthzEnabled()) {
    return true;
  }

  if (scope.size() == 0) {
    // If "scope" is not passed, defaults the parameter to
    // ["sub", "name", "email" and "picture"].
    return true;
  }

  if (scope.size() == 2) {
    return HasScope(scope, "profile") && HasScope(scope, "email");
  }

  return false;
}

void FederatedAuthRequestImpl::OnFetchDataForIdpSucceeded(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    const IdpNetworkRequestManager::AccountList& accounts,
    const IdpNetworkRequestManager::ClientMetadata& client_metadata) {
  fetch_data_.did_succeed_for_at_least_one_idp = true;

  const GURL& idp_config_url = idp_info->provider->config->config_url;

  bool request_permission = ShouldMediateAuthz(idp_info->provider->scope);

  const std::string idp_for_display =
      webid::FormatUrlWithDomain(idp_config_url, /*for_display=*/true);
  idp_info->data = IdentityProviderData(
      idp_for_display, accounts, idp_info->metadata,
      ClientMetadata{client_metadata.terms_of_service_url,
                     client_metadata.privacy_policy_url},
      idp_info->rp_context, /* request_permission */ request_permission);
  idp_infos_[idp_config_url] = std::move(idp_info);

  fetch_data_.pending_idps.erase(idp_config_url);
  MaybeShowAccountsDialog();
}

void FederatedAuthRequestImpl::OnFetchDataForIdpFailed(
    const std::unique_ptr<IdentityProviderInfo> idp_info,
    blink::mojom::FederatedAuthRequestResult result,
    absl::optional<TokenStatus> token_status,
    bool should_delay_callback) {
  const GURL& idp_config_url = idp_info->provider->config->config_url;
  fetch_data_.pending_idps.erase(idp_config_url);

  if (fetch_data_.pending_idps.empty() &&
      !fetch_data_.did_succeed_for_at_least_one_idp) {
    CompleteRequestWithError(result, token_status,
                             /*token_error=*/absl::nullopt,
                             should_delay_callback);
    return;
  }

  AddDevToolsIssue(result);
  AddConsoleErrorMessage(result);

  if (IsFedCmMetricsEndpointEnabled()) {
    SendFailedTokenRequestMetrics(idp_info->endpoints.metrics, result);
  }

  metrics_endpoints_.erase(idp_config_url);

  idp_infos_.erase(idp_config_url);
  // Do not use `idp_config_url` after this line because the reference is no
  // longer valid.

  MaybeShowAccountsDialog();
}

void FederatedAuthRequestImpl::MaybeShowAccountsDialog() {
  if (!fetch_data_.pending_idps.empty()) {
    return;
  }

  // The accounts fetch could be delayed for legitimate reasons. A user may be
  // able to disable FedCM API (e.g. via settings or dismissing another FedCM UI
  // on the same RP origin) before the browser receives the accounts response.
  // We should exit early without showing any UI.
  if (GetApiPermissionStatus() != FederatedApiPermissionStatus::GRANTED) {
    CompleteRequestWithError(
        FederatedAuthRequestResult::kErrorDisabledInSettings,
        TokenStatus::kDisabledInSettings,
        /*token_error=*/absl::nullopt,
        /*should_delay_callback=*/true);
    return;
  }

  // The RenderFrameHost may be alive but not visible in the following
  // situations:
  // Situation #1: User switched tabs
  // Situation #2: User navigated the page. The RenderFrameHost is still
  //   alive thanks to the BFCache.
  //
  // - If this fetch is as a result of an IdP sign-in status change, the FedCM
  // dialog is either visible or temporarily hidden. Update the contents of
  // the dialog.
  // - If the FedCM dialog has not already been shown, do not show the dialog
  // if the RenderFrameHost is hidden because the user does not seem interested
  // in the contents of the current page.
  if (!fetch_data_.for_idp_signin) {
    bool is_visible = IsFrameVisible(render_frame_host().GetMainFrame());
    fedcm_metrics_->RecordWebContentsVisibilityUponReadyToShowDialog(
        is_visible);

    if (!is_visible) {
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorRpPageNotVisible,
          TokenStatus::kRpPageNotVisible,
          /*token_error=*/absl::nullopt,
          /*should_delay_callback=*/true);
      return;
    }

    show_accounts_dialog_time_ = base::TimeTicks::Now();
    fedcm_metrics_->RecordShowAccountsDialogTime(show_accounts_dialog_time_ -
                                                 start_time_);
  }

  fetch_data_ = FetchData();

  DCHECK(idp_data_for_display_.empty());

  for (const auto& idp : idp_order_) {
    auto idp_info_it = idp_infos_.find(idp);
    if (idp_info_it != idp_infos_.end() && idp_info_it->second->data) {
      idp_data_for_display_.push_back(*idp_info_it->second->data);
    }
  }

  // RenderFrameHost should be in the primary page (ex not in the BFCache).
  DCHECK(render_frame_host().GetPage().IsPrimary());

  // TODO(crbug.com/1383384): Handle auto_reauthn_ for multi IDP.
  bool auto_reauthn_enabled =
      mediation_requirement_ != MediationRequirement::kRequired;

  dialog_type_ = auto_reauthn_enabled ? kAutoReauth : kSelectAccount;
  bool is_auto_reauthn_setting_enabled = false;
  bool is_auto_reauthn_embargoed = false;
  absl::optional<base::TimeDelta> time_from_embargo;
  bool requires_user_mediation = false;
  const IdentityProviderData* auto_reauthn_idp = nullptr;
  const IdentityRequestAccount* auto_reauthn_account = nullptr;
  bool has_single_returning_account = false;
  if (auto_reauthn_enabled) {
    is_auto_reauthn_setting_enabled =
        auto_reauthn_permission_delegate_->IsAutoReauthnSettingEnabled();
    is_auto_reauthn_embargoed =
        auto_reauthn_permission_delegate_->IsAutoReauthnEmbargoed(
            GetEmbeddingOrigin());
    if (is_auto_reauthn_embargoed) {
      time_from_embargo =
          base::Time::Now() -
          auto_reauthn_permission_delegate_->GetAutoReauthnEmbargoStartTime(
              GetEmbeddingOrigin());

      // See `kFederatedIdentityAutoReauthnEmbargoDuration`.
      render_frame_host().AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kInfo,
          "Auto re-authn was previously triggered less than 10 minutes ago. "
          "Only one auto re-authn request can be made every 10 minutes.");
    }
    requires_user_mediation = RequiresUserMediation();
    // Auto signs in returning users if they have a single returning account and
    // are signing in.
    has_single_returning_account =
        GetSingleReturningAccount(&auto_reauthn_idp, &auto_reauthn_account);
    if (dialog_type_ == kAutoReauth &&
        (requires_user_mediation || !is_auto_reauthn_setting_enabled ||
         is_auto_reauthn_embargoed || !has_single_returning_account)) {
      dialog_type_ = kSelectAccount;
    }
    if (!has_single_returning_account &&
        mediation_requirement_ == MediationRequirement::kSilent) {
      fedcm_metrics_->RecordAutoReauthnMetrics(
          has_single_returning_account, auto_reauthn_account,
          dialog_type_ == kAutoReauth, !is_auto_reauthn_setting_enabled,
          is_auto_reauthn_embargoed, time_from_embargo,
          requires_user_mediation);

      // By this moment we know that the user has granted permission in the past
      // for the RP/IdP. Because otherwise we have returned already in
      // `ShouldFailBeforeFetchingAccounts`. It means that we can do the
      // following without privacy cost:
      // 1. Reject the promise immediately without delay
      // 2. Not to show any UI to respect `mediation: silent`
      // TODO(crbug.com/1441436): validate the statement above with stakeholders
      render_frame_host().AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "Silent mediation failed reason: the user has used FedCM with "
          "multiple accounts on "
          "this site.");
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorSilentMediationFailure,
          TokenStatus::kSilentMediationFailure,
          /*token_error=*/absl::nullopt,
          /*should_delay_callback=*/false);
      return;
    }

    if (dialog_type_ == kAutoReauth) {
      IdentityRequestAccount account{*auto_reauthn_account};
      IdentityProviderData idp{*auto_reauthn_idp};
      idp.accounts = {account};
      idp_data_for_display_ = {idp};
    }
  }

  // TODO(crbug.com/1408520): opt-out affordance is not included in the origin
  // trial. Should revisit based on the OT feedback.
  bool show_auto_reauthn_checkbox = false;

  bool intercept = false;
  // In tests (content_shell or when --use-fake-ui-for-fedcm is used), the
  // dialog controller will immediately select an account. But if browser
  // automation is enabled, we don't want that to happen because automation
  // should be able to choose which account to select or to cancel.
  // So we use this call to see whether interception is enabled.
  // It is not needed in regular Chrome even when automation is used because
  // there, the dialog will wait for user input anyway.
  devtools_instrumentation::WillShowFedCmDialog(&render_frame_host(),
                                                &intercept);
  // Since we don't reuse the controller for each request, and intercept
  // defaults to false, we only need to call this if intercept is true.
  if (intercept) {
    request_dialog_controller_->SetIsInterceptionEnabled(intercept);
  }

  absl::optional<std::string> iframe_for_display = GetIframeOriginForDisplay(
      GetEmbeddingOrigin(), origin(),
      base::BindOnce(&FederatedAuthRequestImpl::CompleteRequestWithError,
                     weak_ptr_factory_.GetWeakPtr()));

  // TODO(crbug.com/1382863): Handle UI where some IDPs are successful and some
  // IDPs are failing in the multi IDP case.
  request_dialog_controller_->ShowAccountsDialog(
      GetTopFrameOriginForDisplay(GetEmbeddingOrigin()), iframe_for_display,
      idp_data_for_display_,
      (dialog_type_ == kAutoReauth) ? SignInMode::kAuto : SignInMode::kExplicit,
      show_auto_reauthn_checkbox,
      base::BindOnce(&FederatedAuthRequestImpl::OnAccountSelected,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FederatedAuthRequestImpl::OnDialogDismissed,
                     weak_ptr_factory_.GetWeakPtr()));
  devtools_instrumentation::OnFedCmDialogShown(&render_frame_host());

  if (dialog_type_ != kAutoReauth) {
    // We omit recording the accounts dialog shown metric for auto re-authn
    // because the metric is used to detect IDPs flashing UI. Auto re-authn
    // verifying UI cannot be flashed since it is destroyed automatically after
    // 3 seconds and cannot be destroyed earlier for a11y reasons.
    accounts_dialog_shown_time_ = base::TimeTicks::Now();
  }

  // Note that accounts dialog shown after mismatch dialog is also recorded.
  // Although not useful for catching malicious IDPs, it should only be a very
  // small percentage of the samples recorded.
  fedcm_metrics_->RecordAccountsDialogShown();

  if (auto_reauthn_enabled) {
    fedcm_metrics_->RecordAutoReauthnMetrics(
        has_single_returning_account, auto_reauthn_account,
        dialog_type_ == kAutoReauth, !is_auto_reauthn_setting_enabled,
        is_auto_reauthn_embargoed, time_from_embargo, requires_user_mediation);
  }
}

void FederatedAuthRequestImpl::HandleAccountsFetchFailure(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    absl::optional<bool> old_idp_signin_status,
    blink::mojom::FederatedAuthRequestResult result,
    absl::optional<TokenStatus> token_status) {
  url::Origin idp_origin =
      url::Origin::Create(idp_info->provider->config->config_url);
  FedCmIdpSigninStatusMode signin_status_mode =
      webid::GetIdpSigninStatusMode(render_frame_host(), idp_origin);
  if (signin_status_mode == FedCmIdpSigninStatusMode::DISABLED) {
    OnFetchDataForIdpFailed(std::move(idp_info), result, token_status,
                            /*should_delay_callback=*/true);
    return;
  }

  if (!old_idp_signin_status.has_value() ||
      signin_status_mode == FedCmIdpSigninStatusMode::METRICS_ONLY) {
    OnFetchDataForIdpFailed(std::move(idp_info), result, token_status,
                            /*should_delay_callback=*/true);
    return;
  }

  if (!IsFrameVisible(render_frame_host().GetMainFrame())) {
    CompleteRequestWithError(FederatedAuthRequestResult::kErrorRpPageNotVisible,
                             TokenStatus::kRpPageNotVisible,
                             /*token_error=*/absl::nullopt,
                             /*should_delay_callback=*/true);
    return;
  }

  // By this moment we know that the user has granted permission in the past
  // for the RP/IdP. Because otherwise we have returned already in
  // `ShouldFailBeforeFetchingAccounts`. It means that we can do the
  // following without privacy cost:
  // 1. Reject the promise immediately without delay
  // 2. Not to show any UI to respect `mediation: silent`
  // TODO(crbug.com/1441436): validate the statement above with stakeholders
  if (mediation_requirement_ == MediationRequirement::kSilent) {
    CompleteRequestWithError(
        FederatedAuthRequestResult::kErrorSilentMediationFailure,
        TokenStatus::kSilentMediationFailure,
        /*token_error=*/absl::nullopt,
        /*should_delay_callback=*/false);
    return;
  }

  // TODO(crbug.com/1357790): we should figure out how to handle multiple IDP
  // w.r.t. showing a static failure UI. e.g. one IDP is always successful and
  // one always returns 404.

  // RenderFrameHost should be in the primary page (ex not in the BFCache).
  DCHECK(render_frame_host().GetPage().IsPrimary());
  // TODO(crbug.com/1382495): Handle failure UI in the multi IDP case.

  fetch_data_ = FetchData();

  absl::optional<std::string> iframe_for_display = GetIframeOriginForDisplay(
      GetEmbeddingOrigin(), origin(),
      base::BindOnce(&FederatedAuthRequestImpl::CompleteRequestWithError,
                     weak_ptr_factory_.GetWeakPtr()));

  // If IdP login status mismatch dialog is already visible, calling
  // ShowFailureDialog() a 2nd time should notify the user that login
  // failed.
  dialog_type_ = kConfirmIdpLogin;
  login_url_ = idp_info->metadata.idp_login_url;
  request_dialog_controller_->ShowFailureDialog(
      GetTopFrameOriginForDisplay(GetEmbeddingOrigin()), iframe_for_display,
      FormatOriginForDisplay(idp_origin), idp_info->rp_context,
      idp_info->metadata,
      base::BindOnce(&FederatedAuthRequestImpl::OnDismissFailureDialog,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FederatedAuthRequestImpl::SignInToIdP,
                     weak_ptr_factory_.GetWeakPtr(), login_url_));
  fedcm_metrics_->RecordMismatchDialogShown();
  mismatch_dialog_shown_time_ = base::TimeTicks::Now();
  devtools_instrumentation::OnFedCmDialogShown(&render_frame_host());
}

void FederatedAuthRequestImpl::CloseModalDialogView() {
#if BUILDFLAG(IS_ANDROID)
  // On android, invoke NotifyClose on the modal dialog, as the UI code needs to
  // then notify the opener.
  NotifyClose();
#else
  // On desktop, invoke NotifyClose on the opener.
  if (identity_registry_) {
    identity_registry_->NotifyClose(origin());
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void FederatedAuthRequestImpl::OnAccountsResponseReceived(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::AccountList accounts) {
  GURL idp_config_url = idp_info->provider->config->config_url;
  const absl::optional<bool> old_idp_signin_status =
      permission_delegate_->GetIdpSigninStatus(
          url::Origin::Create(idp_config_url));
  webid::UpdateIdpSigninStatusForAccountsEndpointResponse(
      render_frame_host(), idp_config_url, status,
      idp_info->has_failing_idp_signin_status, permission_delegate_,
      fedcm_metrics_.get());

  constexpr char kAccountsUrl[] = "accounts endpoint";
  switch (status.parse_status) {
    case IdpNetworkRequestManager::ParseStatus::kHttpNotFoundError: {
      MaybeAddResponseCodeToConsole(kAccountsUrl, status.response_code);
      HandleAccountsFetchFailure(
          std::move(idp_info), old_idp_signin_status,
          FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound,
          TokenStatus::kAccountsHttpNotFound);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kNoResponseError: {
      MaybeAddResponseCodeToConsole(kAccountsUrl, status.response_code);
      HandleAccountsFetchFailure(
          std::move(idp_info), old_idp_signin_status,
          FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse,
          TokenStatus::kAccountsNoResponse);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kInvalidResponseError: {
      MaybeAddResponseCodeToConsole(kAccountsUrl, status.response_code);
      HandleAccountsFetchFailure(
          std::move(idp_info), old_idp_signin_status,
          FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse,
          TokenStatus::kAccountsInvalidResponse);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kEmptyListError: {
      MaybeAddResponseCodeToConsole(kAccountsUrl, status.response_code);
      HandleAccountsFetchFailure(
          std::move(idp_info), old_idp_signin_status,
          FederatedAuthRequestResult::kErrorFetchingAccountsListEmpty,
          TokenStatus::kAccountsListEmpty);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kInvalidContentTypeError: {
      MaybeAddResponseCodeToConsole(kAccountsUrl, status.response_code);
      HandleAccountsFetchFailure(
          std::move(idp_info), old_idp_signin_status,
          FederatedAuthRequestResult::kErrorFetchingAccountsInvalidContentType,
          TokenStatus::kAccountsInvalidContentType);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kSuccess: {
      FilterAccountsWithLoginHint(idp_info->provider->login_hint, accounts);
      if (IsFedCmDomainHintEnabled()) {
        FilterAccountsWithDomainHint(idp_info->provider->domain_hint, accounts);
      }
      if (accounts.empty()) {
        render_frame_host().AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kError,
            "Accounts were received, but none matched the loginHint.");
        // If there are no accounts after filtering based on the login hint,
        // treat this exactly the same as if we had received an empty accounts
        // list, i.e. IdpNetworkRequestManager::ParseStatus::kEmptyListError.
        HandleAccountsFetchFailure(
            std::move(idp_info), old_idp_signin_status,
            FederatedAuthRequestResult::kErrorFetchingAccountsListEmpty,
            TokenStatus::kAccountsListEmpty);
        return;
      }
      ComputeLoginStateAndReorderAccounts(
          url::Origin::Create(idp_info->provider->config->config_url),
          accounts);

      bool need_client_metadata = false;

      if (ShouldMediateAuthz(idp_info->provider->scope)) {
        for (const IdentityRequestAccount& account : accounts) {
          // ComputeLoginStateAndReorderAccounts() should have populated
          // IdentityRequestAccount::login_state.
          DCHECK(account.login_state);
          if (*account.login_state == LoginState::kSignUp) {
            need_client_metadata = true;
            break;
          }
        }
      }

      if (need_client_metadata &&
          webid::IsEndpointSameOrigin(idp_info->provider->config->config_url,
                                      idp_info->endpoints.client_metadata)) {
        // Copy OnClientMetadataResponseReceived() parameters because `idp_info`
        // is moved.
        GURL client_metadata_endpoint = idp_info->endpoints.client_metadata;
        std::string client_id = idp_info->provider->config->client_id;
        network_manager_->FetchClientMetadata(
            client_metadata_endpoint, client_id,
            base::BindOnce(
                &FederatedAuthRequestImpl::OnClientMetadataResponseReceived,
                weak_ptr_factory_.GetWeakPtr(), std::move(idp_info),
                std::move(accounts)));
      } else {
        OnFetchDataForIdpSucceeded(std::move(idp_info), accounts,
                                   IdpNetworkRequestManager::ClientMetadata());
      }
    }
  }
}

void FederatedAuthRequestImpl::ComputeLoginStateAndReorderAccounts(
    const url::Origin& idp_origin,
    IdpNetworkRequestManager::AccountList& accounts) {
  // Populate the accounts login state.
  for (auto& account : accounts) {
    // Record when IDP and browser have different user sign-in states.
    bool idp_claimed_sign_in = account.login_state == LoginState::kSignIn;
    bool browser_observed_sign_in = permission_delegate_->HasSharingPermission(
        origin(), GetEmbeddingOrigin(), idp_origin, account.id);

    if (idp_claimed_sign_in == browser_observed_sign_in) {
      fedcm_metrics_->RecordSignInStateMatchStatus(
          SignInStateMatchStatus::kMatch);
    } else if (idp_claimed_sign_in) {
      fedcm_metrics_->RecordSignInStateMatchStatus(
          SignInStateMatchStatus::kIdpClaimedSignIn);
    } else {
      fedcm_metrics_->RecordSignInStateMatchStatus(
          SignInStateMatchStatus::kBrowserObservedSignIn);
    }

    // We set the login state based on the IDP response if it sends
    // back an approved_clients list. If it does not, we need to set
    // it here based on browser state.
    if (account.login_state) {
      continue;
    }
    LoginState login_state = LoginState::kSignUp;
    // Consider this a sign-in if we have seen a successful sign-up for
    // this account before.
    if (browser_observed_sign_in) {
      login_state = LoginState::kSignIn;
    }
    account.login_state = login_state;
  }

  // Now that the login states have been computed, order accounts so that the
  // returning accounts go first and the other accounts go afterwards. Since the
  // number of accounts is likely very small, sorting by login_state should be
  // fast.
  std::sort(accounts.begin(), accounts.end(), [](const auto& a, const auto& b) {
    return a.login_state < b.login_state;
  });
}

void FederatedAuthRequestImpl::OnAccountSelected(const GURL& idp_config_url,
                                                 const std::string& account_id,
                                                 bool is_sign_in) {
  DCHECK(!account_id.empty());
  const IdentityProviderInfo& idp_info = *idp_infos_[idp_config_url];

  // Check if the user has disabled the FedCM API after the FedCM UI is
  // displayed. This ensures that requests are not wrongfully sent to IDPs when
  // settings are changed while an existing FedCM UI is displayed. Ideally, we
  // should enforce this check before all requests but users typically won't
  // have time to disable the FedCM API in other types of requests.
  if (GetApiPermissionStatus() != FederatedApiPermissionStatus::GRANTED) {
    CompleteRequestWithError(
        FederatedAuthRequestResult::kErrorDisabledInSettings,
        TokenStatus::kDisabledInSettings,
        /*token_error=*/absl::nullopt,
        /*should_delay_callback=*/true);
    return;
  }

  if (dialog_type_ == kAutoReauth) {
    // Embargo auto re-authn to mitigate a deadloop where an auto
    // re-authenticated user gets auto re-authenticated again soon after logging
    // out of the active session.
    auto_reauthn_permission_delegate_->RecordEmbargoForAutoReauthn(
        GetEmbeddingOrigin());
  } else {
    // Once a user has explicitly selected an account, there is no need to block
    // auto re-authn with embargo.
    auto_reauthn_permission_delegate_->RemoveEmbargoForAutoReauthn(
        GetEmbeddingOrigin());
  }

  fedcm_metrics_->RecordIsSignInUser(is_sign_in);

  api_permission_delegate_->RemoveEmbargoAndResetCounts(GetEmbeddingOrigin());

  account_id_ = account_id;
  select_account_time_ = base::TimeTicks::Now();
  fedcm_metrics_->RecordContinueOnDialogTime(select_account_time_ -
                                             show_accounts_dialog_time_);

  network_manager_->SendTokenRequest(
      idp_info.endpoints.token, account_id_,
      ComputeUrlEncodedTokenPostData(
          idp_info.provider->config->client_id, idp_info.provider->nonce,
          account_id, is_sign_in, dialog_type_ == kAutoReauth,
          idp_info.provider->scope, idp_info.provider->responseType,
          idp_info.provider->params),
      base::BindOnce(&FederatedAuthRequestImpl::OnTokenResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     idp_info.provider->Clone()),
      base::BindOnce(&FederatedAuthRequestImpl::OnContinueOnResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     idp_info.provider->Clone()),
      base::BindOnce(&FederatedAuthRequestImpl::RecordErrorMetrics,
                     weak_ptr_factory_.GetWeakPtr(),
                     idp_info.provider->Clone()));
}

void FederatedAuthRequestImpl::OnDismissFailureDialog(
    IdentityRequestDialogController::DismissReason dismiss_reason) {
  // Clicking the close button and swiping away the account chooser are more
  // intentional than other ways of dismissing the account chooser such as
  // the virtual keyboard showing on Android. Dismissal through closing the
  // pop-up window is not embargoed since the user has taken some action to
  // continue to open the pop-up window.
  bool should_embargo = false;
  switch (dismiss_reason) {
    case IdentityRequestDialogController::DismissReason::kCloseButton:
    case IdentityRequestDialogController::DismissReason::kSwipe:
      should_embargo = true;
      break;
    default:
      break;
  }

  fedcm_metrics_->RecordCancelReason(dismiss_reason);

  if (should_embargo) {
    api_permission_delegate_->RecordDismissAndEmbargo(GetEmbeddingOrigin());
  }

  CompleteRequestWithError(should_embargo
                               ? FederatedAuthRequestResult::kShouldEmbargo
                               : FederatedAuthRequestResult::kError,
                           should_embargo ? TokenStatus::kShouldEmbargo
                                          : TokenStatus::kNotSignedInWithIdp,
                           /*token_error=*/absl::nullopt,
                           /*should_delay_callback=*/false);
}

void FederatedAuthRequestImpl::OnDismissErrorDialog(
    const GURL& idp_config_url,
    IdpNetworkRequestManager::FetchStatus status,
    absl::optional<TokenError> token_error,
    IdentityRequestDialogController::DismissReason dismiss_reason) {
  bool has_url = token_error && !token_error->url.is_empty();
  ErrorDialogResult result;
  switch (dismiss_reason) {
    case IdentityRequestDialogController::DismissReason::kCloseButton:
      result = has_url ? ErrorDialogResult::kCloseWithMoreDetails
                       : ErrorDialogResult::kCloseWithoutMoreDetails;
      break;
    case IdentityRequestDialogController::DismissReason::kSwipe:
      result = has_url ? ErrorDialogResult::kSwipeWithMoreDetails
                       : ErrorDialogResult::kSwipeWithoutMoreDetails;
      break;
    case IdentityRequestDialogController::DismissReason::kGotItButton:
      result = has_url ? ErrorDialogResult::kGotItWithMoreDetails
                       : ErrorDialogResult::kGotItWithoutMoreDetails;
      break;
    case IdentityRequestDialogController::DismissReason::kMoreDetailsButton:
      result = ErrorDialogResult::kMoreDetails;
      break;
    default:
      result = has_url ? ErrorDialogResult::kOtherWithMoreDetails
                       : ErrorDialogResult::kOtherWithoutMoreDetails;
      break;
  }
  fedcm_metrics_->RecordErrorDialogResult(result);

  CompleteTokenRequest(idp_config_url, status, /*token=*/absl::nullopt,
                       token_error, /*should_delay_callback=*/false);
}

void FederatedAuthRequestImpl::OnDialogDismissed(
    IdentityRequestDialogController::DismissReason dismiss_reason) {
  // Clicking the close button and swiping away the account chooser are more
  // intentional than other ways of dismissing the account chooser such as
  // the virtual keyboard showing on Android.
  bool should_embargo = false;
  switch (dismiss_reason) {
    case IdentityRequestDialogController::DismissReason::kCloseButton:
    case IdentityRequestDialogController::DismissReason::kSwipe:
      should_embargo = true;
      break;
    default:
      break;
  }

  if (should_embargo) {
    base::TimeTicks dismiss_dialog_time = base::TimeTicks::Now();
    fedcm_metrics_->RecordCancelOnDialogTime(dismiss_dialog_time -
                                             show_accounts_dialog_time_);
  }
  fedcm_metrics_->RecordCancelReason(dismiss_reason);

  if (should_embargo) {
    api_permission_delegate_->RecordDismissAndEmbargo(GetEmbeddingOrigin());
  }

  // Reject the promise immediately if the UI is dismissed without selecting
  // an account. Meanwhile, we fuzz the rejection time for other failures to
  // make it indistinguishable.
  CompleteRequestWithError(should_embargo
                               ? FederatedAuthRequestResult::kShouldEmbargo
                               : FederatedAuthRequestResult::kError,
                           should_embargo ? TokenStatus::kShouldEmbargo
                                          : TokenStatus::kNotSelectAccount,
                           /*token_error=*/absl::nullopt,
                           /*should_delay_callback=*/false);
}

void FederatedAuthRequestImpl::ShowModalDialog(const GURL& url) {
  // Reset dialog type since we are not showing a fedcm dialog while the
  // popup window is open.
  // TODO(cbiesinger): Should this return a special dialog type?
  dialog_type_ = kNone;

  WebContents* web_contents = request_dialog_controller_->ShowModalDialog(
      url, base::BindOnce(&FederatedAuthRequestImpl::OnDialogDismissed,
                          weak_ptr_factory_.GetWeakPtr()));
  // This may be null on Android, as the method cannot return the WebContents of
  // the CCT that will be created.
  if (web_contents) {
    IdentityRegistry::CreateForWebContents(
        web_contents, weak_ptr_factory_.GetWeakPtr(), url::Origin::Create(url));
  }

  // Samples are at most 10 minutes. This metric is used to determine a
  // reasonable minimum duration for the mismatch dialog to be shown to prevent
  // abuse through flashing UI. When users trigger the IDP sign-in flow, the
  // mismatch dialog is hidden so we record this metric upon user triggering the
  // flow.
  if (mismatch_dialog_shown_time_.has_value()) {
    fedcm_metrics_->RecordMismatchDialogShownDuration(
        base::TimeTicks::Now() - mismatch_dialog_shown_time_.value());
    mismatch_dialog_shown_time_ = absl::nullopt;
  }
}

void FederatedAuthRequestImpl::OnContinueOnResponseReceived(
    IdentityProviderRequestOptionsPtr idp,
    IdpNetworkRequestManager::FetchStatus status,
    const GURL& continue_on) {
  // We only allow loading continue_on urls that are same-origin
  // with the IdP.
  // This isn't necessarily final, but seemed like a safer
  // and sufficient default for now.
  // This behavior may change in https://crbug.com/1429083
  bool is_same_origin =
      url::Origin::Create(continue_on)
          .IsSameOriginWith(url::Origin::Create(idp->config->config_url));

  if (!IsFedCmAuthzEnabled() || !is_same_origin) {
    CompleteRequestWithError(
        FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse,
        TokenStatus::kIdTokenInvalidResponse,
        /*token_error=*/absl::nullopt,
        /*should_delay_callback=*/true);
    return;
  }

  // TODO(crbug.com/1429083): record the appropriate metrics.
  ShowModalDialog(continue_on);
}

void FederatedAuthRequestImpl::ShowErrorDialog(
    const GURL& idp_config_url,
    IdpNetworkRequestManager::FetchStatus status,
    absl::optional<TokenError> token_error) {
  CHECK(idp_infos_.find(idp_config_url) != idp_infos_.end());

  absl::optional<std::string> iframe_for_display = GetIframeOriginForDisplay(
      GetEmbeddingOrigin(), origin(),
      base::BindOnce(&FederatedAuthRequestImpl::CompleteRequestWithError,
                     weak_ptr_factory_.GetWeakPtr()));

  // TODO(crbug.com/1485710): Refactor IdentityCredentialTokenError
  request_dialog_controller_->ShowErrorDialog(
      GetTopFrameOriginForDisplay(GetEmbeddingOrigin()), iframe_for_display,
      FormatOriginForDisplay(url::Origin::Create(idp_config_url)),
      idp_infos_[idp_config_url]->rp_context,
      idp_infos_[idp_config_url]->metadata, token_error,
      base::BindOnce(&FederatedAuthRequestImpl::OnDismissErrorDialog,
                     weak_ptr_factory_.GetWeakPtr(), idp_config_url, status,
                     token_error),
      token_error && !token_error->url.is_empty()
          ? base::BindOnce(&FederatedAuthRequestImpl::ShowModalDialog,
                           weak_ptr_factory_.GetWeakPtr(), token_error->url)
          : base::NullCallback());
}

void FederatedAuthRequestImpl::OnTokenResponseReceived(
    IdentityProviderRequestOptionsPtr idp,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::TokenResult result) {
  CHECK(result.token.empty() || !result.error);

  bool should_show_error_ui =
      IsFedCmErrorEnabled() &&
      (result.error ||
       status.parse_status != IdpNetworkRequestManager::ParseStatus::kSuccess);
  auto complete_request_callback =
      should_show_error_ui
          ? base::BindOnce(&FederatedAuthRequestImpl::ShowErrorDialog,
                           weak_ptr_factory_.GetWeakPtr(),
                           idp->config->config_url, status, result.error)
          : base::BindOnce(&FederatedAuthRequestImpl::CompleteTokenRequest,
                           weak_ptr_factory_.GetWeakPtr(),
                           idp->config->config_url, status, result.token,
                           /*token_error=*/absl::nullopt,
                           /*should_delay_callback=*/true);

  // When fetching id tokens we show a "Verify" sheet to users in case fetching
  // takes a long time due to latency etc. In case that the fetching process is
  // fast, we still want to show the "Verify" sheet for at least
  // |token_request_delay_| seconds for better UX.
  token_response_time_ = base::TimeTicks::Now();
  base::TimeDelta fetch_time = token_response_time_ - select_account_time_;
  if (should_complete_request_immediately_ ||
      fetch_time >= token_request_delay_) {
    std::move(complete_request_callback).Run();
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(complete_request_callback),
      token_request_delay_ - fetch_time);
}

void FederatedAuthRequestImpl::CompleteTokenRequest(
    const GURL& idp_config_url,
    IdpNetworkRequestManager::FetchStatus status,
    absl::optional<std::string> token,
    absl::optional<TokenError> token_error,
    bool should_delay_callback) {
  DCHECK(!start_time_.is_null());
  constexpr char kIdAssertionUrl[] = "id assertion endpoint";
  switch (status.parse_status) {
    case IdpNetworkRequestManager::ParseStatus::kHttpNotFoundError: {
      MaybeAddResponseCodeToConsole(kIdAssertionUrl, status.response_code);
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound,
          TokenStatus::kIdTokenHttpNotFound, token_error,
          should_delay_callback);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kNoResponseError: {
      MaybeAddResponseCodeToConsole(kIdAssertionUrl, status.response_code);
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse,
          TokenStatus::kIdTokenNoResponse, token_error, should_delay_callback);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kInvalidResponseError: {
      MaybeAddResponseCodeToConsole(kIdAssertionUrl, status.response_code);
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse,
          TokenStatus::kIdTokenInvalidResponse, token_error,
          should_delay_callback);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kInvalidContentTypeError: {
      MaybeAddResponseCodeToConsole(kIdAssertionUrl, status.response_code);
      CompleteRequestWithError(
          FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidContentType,
          TokenStatus::kIdTokenInvalidContentType, token_error,
          should_delay_callback);
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kEmptyListError: {
      NOTREACHED() << "kEmptyListError is undefined for CompleteTokenRequest";
      return;
    }
    case IdpNetworkRequestManager::ParseStatus::kSuccess: {
      if (token_error) {
        MaybeAddResponseCodeToConsole(kIdAssertionUrl, status.response_code);
        if (error_url_type_ && *error_url_type_ == ErrorUrlType::kCrossSite) {
          CompleteRequestWithError(
              FederatedAuthRequestResult::
                  kErrorFetchingIdTokenCrossSiteIdpErrorResponse,
              TokenStatus::kIdTokenCrossSiteIdpErrorResponse, token_error,
              should_delay_callback);
          return;
        }
        CompleteRequestWithError(
            FederatedAuthRequestResult::kErrorFetchingIdTokenIdpErrorResponse,
            TokenStatus::kIdTokenIdpErrorResponse, token_error,
            should_delay_callback);
        return;
      }
      // Grant sharing permission specific to *this account*.
      //
      // TODO(majidvp): But wait which account?
      //   1) The account that user selected in our UI (i.e., account_id_) or
      //   2) The one for which the IDP generated a token.
      //
      // Ideally these are one and the same but currently there is no
      // enforcement for that equality so they could be different. In the
      // future we may want to enforce that the token account (aka subject)
      // matches the user selected account. But for now these questions are
      // moot since we don't actually inspect the returned idtoken.
      // https://crbug.com/1199088
      CHECK(!account_id_.empty());
      permission_delegate_->GrantSharingPermission(
          origin(), GetEmbeddingOrigin(), url::Origin::Create(idp_config_url),
          account_id_);

      permission_delegate_->GrantActiveSession(
          origin(), url::Origin::Create(idp_config_url), account_id_);

      SetRequiresUserMediation(false);

      fedcm_metrics_->RecordTokenResponseAndTurnaroundTime(
          token_response_time_ - select_account_time_,
          token_response_time_ - start_time_);

      if (IsFedCmMetricsEndpointEnabled()) {
        for (const auto& metrics_endpoint_kv : metrics_endpoints_) {
          const GURL& metrics_endpoint = metrics_endpoint_kv.second;
          if (!metrics_endpoint.is_valid()) {
            continue;
          }

          if (metrics_endpoint_kv.first == idp_config_url) {
            network_manager_->SendSuccessfulTokenRequestMetrics(
                metrics_endpoint, show_accounts_dialog_time_ - start_time_,
                select_account_time_ - show_accounts_dialog_time_,
                token_response_time_ - select_account_time_,
                token_response_time_ - start_time_);
          } else {
            // Send kUserFailure so that IDP cannot tell difference between user
            // selecting a different IDP and user dismissing dialog without
            // selecting any IDP.
            network_manager_->SendFailedTokenRequestMetrics(
                metrics_endpoint, IdpNetworkRequestManager::
                                      MetricsEndpointErrorCode::kUserFailure);
          }
        }
      }

      CompleteRequest(FederatedAuthRequestResult::kSuccess,
                      TokenStatus::kSuccess, /*token_error=*/absl::nullopt,
                      idp_config_url, token.value(),
                      /*should_delay_callback=*/false);
      return;
    }
  }
}

void FederatedAuthRequestImpl::DispatchOneLogout() {
  auto logout_request = std::move(logout_requests_.front());
  DCHECK(logout_request->url.is_valid());
  std::string account_id = logout_request->account_id;
  auto logout_origin = url::Origin::Create(logout_request->url);
  logout_requests_.pop();

  if (permission_delegate_->HasActiveSession(logout_origin, origin(),
                                             account_id)) {
    network_manager_->SendLogout(
        logout_request->url,
        base::BindOnce(&FederatedAuthRequestImpl::OnLogoutCompleted,
                       weak_ptr_factory_.GetWeakPtr()));
    permission_delegate_->RevokeActiveSession(logout_origin, origin(),
                                              account_id);
  } else {
    if (logout_requests_.empty()) {
      CompleteLogoutRequest(LogoutRpsStatus::kSuccess);
      return;
    }

    DispatchOneLogout();
  }
}

void FederatedAuthRequestImpl::OnLogoutCompleted() {
  if (logout_requests_.empty()) {
    CompleteLogoutRequest(LogoutRpsStatus::kSuccess);
    return;
  }

  DispatchOneLogout();
}

void FederatedAuthRequestImpl::CompleteRequestWithError(
    blink::mojom::FederatedAuthRequestResult result,
    absl::optional<TokenStatus> token_status,
    absl::optional<TokenError> token_error,
    bool should_delay_callback) {
  CompleteRequest(result, token_status, token_error,
                  /*selected_idp_config_url=*/absl::nullopt,
                  /*token=*/"", should_delay_callback);
}

void FederatedAuthRequestImpl::CompleteRequest(
    blink::mojom::FederatedAuthRequestResult result,
    absl::optional<TokenStatus> token_status,
    absl::optional<TokenError> token_error,
    const absl::optional<GURL>& selected_idp_config_url,
    const std::string& id_token,
    bool should_delay_callback) {
  DCHECK(result == FederatedAuthRequestResult::kSuccess || id_token.empty());

  if (accounts_dialog_shown_time_.has_value()) {
    fedcm_metrics_->RecordAccountsDialogShownDuration(
        base::TimeTicks::Now() - accounts_dialog_shown_time_.value());
    accounts_dialog_shown_time_ = absl::nullopt;
  }

  if (mismatch_dialog_shown_time_.has_value()) {
    fedcm_metrics_->RecordMismatchDialogShownDuration(
        base::TimeTicks::Now() - mismatch_dialog_shown_time_.value());
    mismatch_dialog_shown_time_ = absl::nullopt;
  }

  if (!auth_request_token_callback_) {
    return;
  }

  if (result != FederatedAuthRequestResult::kSuccess &&
      fetch_data_.for_idp_signin &&
      !ShouldSuppressIdpSigninFailureDialog(token_status)) {
    fetch_data_ = FetchData();

    request_dialog_controller_->ShowIdpSigninFailureDialog(base::BindOnce(
        &FederatedAuthRequestImpl::CompleteRequest,
        weak_ptr_factory_.GetWeakPtr(), result, std::move(token_status),
        std::move(token_error), selected_idp_config_url, id_token,
        should_delay_callback));
    return;
  }

  if (token_status) {
    fedcm_metrics_->RecordRequestTokenStatus(*token_status,
                                             mediation_requirement_);
  }

  if (!errors_logged_to_console_ &&
      result != FederatedAuthRequestResult::kSuccess) {
    errors_logged_to_console_ = true;

    AddDevToolsIssue(result);
    AddConsoleErrorMessage(result);

    if (IsFedCmMetricsEndpointEnabled()) {
      for (const auto& metrics_endpoint_kv : metrics_endpoints_) {
        SendFailedTokenRequestMetrics(metrics_endpoint_kv.second, result);
      }
    }
  }

  bool is_auto_selected =
      IsFedCmAutoSelectedFlagEnabled() && dialog_type_ == kAutoReauth;

  CleanUp();

  if (!should_delay_callback || should_complete_request_immediately_) {
    GetPageData(&render_frame_host())->SetPendingWebIdentityRequest(nullptr);
    errors_logged_to_console_ = false;

    blink::mojom::TokenErrorPtr error;
    if (token_error) {
      error = blink::mojom::TokenError::New();
      error->code = token_error->code;
      error->url = token_error->url.spec();
    }
    RequestTokenStatus status =
        FederatedAuthRequestResultToRequestTokenStatus(result);
    std::move(auth_request_token_callback_)
        .Run(status, selected_idp_config_url, id_token, std::move(error),
             is_auto_selected);
    auth_request_token_callback_.Reset();
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FederatedAuthRequestImpl::OnRejectRequest,
                       weak_ptr_factory_.GetWeakPtr()),
        GetRandomRejectionTime());
  }
}

void FederatedAuthRequestImpl::SendFailedTokenRequestMetrics(
    const GURL& metrics_endpoint,
    blink::mojom::FederatedAuthRequestResult result) {
  DCHECK(IsFedCmMetricsEndpointEnabled());
  if (!metrics_endpoint.is_valid()) {
    return;
  }

  network_manager_->SendFailedTokenRequestMetrics(
      metrics_endpoint,
      FederatedAuthRequestResultToMetricsEndpointErrorCode(result));
}

void FederatedAuthRequestImpl::CleanUp() {
  weak_ptr_factory_.InvalidateWeakPtrs();

  permission_delegate_->RemoveIdpSigninStatusObserver(this);

  request_dialog_controller_.reset();
  network_manager_.reset();
  // Given that |request_dialog_controller_| has reference to this web content
  // instance we destroy that first.
  provider_fetcher_.reset();
  account_id_ = std::string();
  start_time_ = base::TimeTicks();
  show_accounts_dialog_time_ = base::TimeTicks();
  select_account_time_ = base::TimeTicks();
  token_response_time_ = base::TimeTicks();
  accounts_dialog_shown_time_ = absl::nullopt;
  mismatch_dialog_shown_time_ = absl::nullopt;
  idp_infos_.clear();
  idp_data_for_display_.clear();
  fetch_data_ = FetchData();
  idp_order_.clear();
  metrics_endpoints_.clear();
  token_request_get_infos_.clear();
  login_url_ = GURL();
  dialog_type_ = kNone;
}

void FederatedAuthRequestImpl::AddDevToolsIssue(
    FederatedAuthRequestResult result) {
  DCHECK_NE(result, FederatedAuthRequestResult::kSuccess);

  // It would be possible to add this inspector issue on the renderer, which
  // will receive the callback. However, it is preferable to do so on the
  // browser because this is closer to the source, which means adding
  // additional metadata is easier. In addition, in the future we may only
  // need to pass a small amount of information to the renderer in the case of
  // an error, so it would be cleaner to do this by reporting the inspector
  // issue from the browser.
  auto details = blink::mojom::InspectorIssueDetails::New();
  auto federated_auth_request_details =
      blink::mojom::FederatedAuthRequestIssueDetails::New(result);
  details->federated_auth_request_details =
      std::move(federated_auth_request_details);
  render_frame_host().ReportInspectorIssue(
      blink::mojom::InspectorIssueInfo::New(
          blink::mojom::InspectorIssueCode::kFederatedAuthRequestIssue,
          std::move(details)));
}

void FederatedAuthRequestImpl::AddConsoleErrorMessage(
    FederatedAuthRequestResult result) {
  render_frame_host().AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError,
      webid::GetConsoleErrorMessageFromResult(result));
}

void FederatedAuthRequestImpl::MaybeAddResponseCodeToConsole(
    const char* fetch_description,
    int response_code) {
  absl::optional<std::string> console_message =
      webid::ComputeConsoleMessageForHttpResponseCode(fetch_description,
                                                      response_code);
  if (console_message) {
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError, *console_message);
  }
}

url::Origin FederatedAuthRequestImpl::GetEmbeddingOrigin() const {
  return render_frame_host().GetMainFrame()->GetLastCommittedOrigin();
}

void FederatedAuthRequestImpl::CompleteLogoutRequest(
    blink::mojom::LogoutRpsStatus status) {
  network_manager_.reset();
  base::queue<blink::mojom::LogoutRpsRequestPtr>().swap(logout_requests_);
  if (logout_callback_) {
    std::move(logout_callback_).Run(status);
    logout_callback_.Reset();
    GetPageData(&render_frame_host())->SetPendingWebIdentityRequest(nullptr);
  }
}

void FederatedAuthRequestImpl::CompleteUserInfoRequest(
    FederatedAuthUserInfoRequest* request,
    RequestUserInfoCallback callback,
    blink::mojom::RequestUserInfoStatus status,
    absl::optional<std::vector<blink::mojom::IdentityUserInfoPtr>> user_info) {
  auto it = std::find_if(
      user_info_requests_.begin(), user_info_requests_.end(),
      [request](const std::unique_ptr<FederatedAuthUserInfoRequest>& ptr) {
        return ptr.get() == request;
      });
  if (it == user_info_requests_.end()) {
    NOTREACHED() << "The completed user info request is nowhere to be found";
    return;
  }

  std::move(callback).Run(status, std::move(user_info));
  user_info_requests_.erase(it);
}

std::unique_ptr<IdpNetworkRequestManager>
FederatedAuthRequestImpl::CreateNetworkManager() {
  if (mock_network_manager_) {
    return std::move(mock_network_manager_);
  }

  return IdpNetworkRequestManager::Create(
      static_cast<RenderFrameHostImpl*>(&render_frame_host()));
}

std::unique_ptr<IdentityRequestDialogController>
FederatedAuthRequestImpl::CreateDialogController() {
  if (mock_dialog_controller_) {
    return std::move(mock_dialog_controller_);
  }

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForFedCM)) {
    std::string selected_account =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kUseFakeUIForFedCM);
    return std::make_unique<FakeIdentityRequestDialogController>(
        selected_account.empty()
            ? absl::nullopt
            : absl::optional<std::string>(selected_account),
        web_contents);
  }

  return GetContentClient()->browser()->CreateIdentityRequestDialogController(
      web_contents);
}

std::unique_ptr<DigitalCredentialProvider>
FederatedAuthRequestImpl::CreateDigitalCredentialProvider() {
  // A provider may only be created in browser tests by this moment.
  std::unique_ptr<DigitalCredentialProvider> provider =
      GetContentClient()->browser()->CreateDigitalCredentialProvider();

  if (!provider) {
    return DigitalCredentialProvider::Create();
  }
  return provider;
}

void FederatedAuthRequestImpl::SetTokenRequestDelayForTests(
    base::TimeDelta delay) {
  token_request_delay_ = delay;
}

void FederatedAuthRequestImpl::SetNetworkManagerForTests(
    std::unique_ptr<IdpNetworkRequestManager> manager) {
  mock_network_manager_ = std::move(manager);
}

void FederatedAuthRequestImpl::SetDialogControllerForTests(
    std::unique_ptr<IdentityRequestDialogController> controller) {
  mock_dialog_controller_ = std::move(controller);
}

void FederatedAuthRequestImpl::NotifyClose() {
#if BUILDFLAG(IS_ANDROID)
  // We invoke this method on the modal dialog on Android, so we may need to
  // create the controller at this point.
  if (!request_dialog_controller_) {
    request_dialog_controller_ = CreateDialogController();
  }
#endif  // BUILDFLAG(IS_ANDROID)
  CHECK(request_dialog_controller_);
  request_dialog_controller_->CloseModalDialog();
}

bool FederatedAuthRequestImpl::NotifyResolve(const std::string& token) {
  // Close the pop-up window post user permission.
  NotifyClose();

  // TODO(crbug.com/1429083): handle the multi-idp case when there are
  // more than one config_urls hanging.
  CompleteRequest(FederatedAuthRequestResult::kSuccess, TokenStatus::kSuccess,
                  /*token_error=*/absl::nullopt,
                  /*selected_idp_config_url=*/absl::nullopt, token,
                  /*should_delay_callback=*/false);
  // TODO(crbug.com/1429083): handle the corner cases where CompleteRequest
  // can't actually fulfill the request.
  return true;
}

void FederatedAuthRequestImpl::OnRejectRequest() {
  if (!auth_request_token_callback_) {
    return;
  }
  DCHECK(!logout_callback_);
  DCHECK(errors_logged_to_console_);
  CompleteRequestWithError(FederatedAuthRequestResult::kError,
                           /*token_status=*/absl::nullopt,
                           /*token_error=*/absl::nullopt,
                           /*should_delay_callback=*/false);
}

FederatedApiPermissionStatus
FederatedAuthRequestImpl::GetApiPermissionStatus() {
  DCHECK(api_permission_delegate_);
  FederatedApiPermissionStatus status =
      api_permission_delegate_->GetApiPermissionStatus(GetEmbeddingOrigin());
  // We allow FedCM without third-party cookies when the IDP sign-in
  // status API is enabled, in general or through OT.
  if (status ==
          FederatedApiPermissionStatus::BLOCKED_THIRD_PARTY_COOKIES_BLOCKED &&
      webid::GetIdpSigninStatusMode(render_frame_host(), url::Origin()) ==
          FedCmIdpSigninStatusMode::ENABLED) {
    status = FederatedApiPermissionStatus::GRANTED;
  }
  return status;
}

void FederatedAuthRequestImpl::AcceptAccountsDialogForDevtools(
    const GURL& config_url,
    const IdentityRequestAccount& account) {
  bool is_sign_in =
      account.login_state == IdentityRequestAccount::LoginState::kSignIn;
  OnAccountSelected(config_url, account.id, is_sign_in);
}

void FederatedAuthRequestImpl::DismissAccountsDialogForDevtools(
    bool should_embargo) {
  // We somewhat arbitrarily pick a reason that does/does not trigger
  // cooldown.
  IdentityRequestDialogController::DismissReason reason =
      should_embargo
          ? IdentityRequestDialogController::DismissReason::kCloseButton
          : IdentityRequestDialogController::DismissReason::kOther;
  OnDialogDismissed(reason);
}

void FederatedAuthRequestImpl::AcceptConfirmIdpLoginDialogForDevtools() {
  DCHECK(login_url_.is_valid());
  SignInToIdP(login_url_);
}

void FederatedAuthRequestImpl::DismissConfirmIdpLoginDialogForDevtools() {
  // These values match what HandleAccountsFetchFailure passes.
  OnDismissFailureDialog(
      IdentityRequestDialogController::DismissReason::kOther);
}

bool FederatedAuthRequestImpl::GetSingleReturningAccount(
    const IdentityProviderData** out_idp_data,
    const IdentityRequestAccount** out_account) {
  for (const auto& idp_info : idp_infos_) {
    for (const auto& account : idp_info.second->data->accounts) {
      if (account.login_state == LoginState::kSignUp) {
        continue;
      }
      // account.login_state could be set to kSignIn if the client is on the
      // `approved_clients` list provided by IDP. However, in this case we have
      // to trust the browser observed sign-in.
      if (!permission_delegate_->HasSharingPermission(
              origin(), GetEmbeddingOrigin(),
              url::Origin::Create(idp_info.first), account.id)) {
        continue;
      }

      if (*out_account) {
        return false;
      }
      *out_idp_data = &(*idp_info.second->data);
      *out_account = &account;
    }
  }

  if (*out_account) {
    return true;
  }

  return false;
}

bool FederatedAuthRequestImpl::ShouldFailBeforeFetchingAccounts(
    const GURL& config_url) {
  if (mediation_requirement_ != MediationRequirement::kSilent) {
    return false;
  }

  bool is_auto_reauthn_setting_enabled =
      auto_reauthn_permission_delegate_->IsAutoReauthnSettingEnabled();
  if (!is_auto_reauthn_setting_enabled) {
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Silent mediation failed reason: the user has disabled auto re-authn.");
  }

  bool is_auto_reauthn_embargoed =
      auto_reauthn_permission_delegate_->IsAutoReauthnEmbargoed(
          GetEmbeddingOrigin());
  absl::optional<base::TimeDelta> time_from_embargo;
  if (is_auto_reauthn_embargoed) {
    time_from_embargo =
        base::Time::Now() -
        auto_reauthn_permission_delegate_->GetAutoReauthnEmbargoStartTime(
            GetEmbeddingOrigin());
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Silent mediation failed reason: auto re-authn is in quiet period "
        "because "
        "it was recently used on this site.");
  }

  bool has_sharing_permission_for_any_account =
      permission_delegate_->HasSharingPermission(
          origin(), GetEmbeddingOrigin(), url::Origin::Create(config_url),
          absl::nullopt);

  if (!has_sharing_permission_for_any_account) {
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Silent mediation failed reason: the user has not used FedCM on this "
        "site with this identity provider.");
  }

  bool requires_user_mediation = RequiresUserMediation();
  if (requires_user_mediation) {
    render_frame_host().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Silent mediation failed reason: preventSilentAccess() has been "
        "invoked on the site.");
  }

  if (requires_user_mediation || !is_auto_reauthn_setting_enabled ||
      is_auto_reauthn_embargoed || !has_sharing_permission_for_any_account) {
    // Record the relevant auto reauthn metrics before aborting the FedCM flow.
    fedcm_metrics_->RecordAutoReauthnMetrics(
        /*has_single_returning_account=*/absl::nullopt,
        /*auto_signin_account=*/nullptr,
        /*auto_reauthn_success=*/false, !is_auto_reauthn_setting_enabled,
        is_auto_reauthn_embargoed, time_from_embargo, requires_user_mediation);
    return true;
  }
  return false;
}

bool FederatedAuthRequestImpl::RequiresUserMediation() {
  std::string site = webid::FormatUrlWithDomain(origin().GetURL(),
                                                /*for_display=*/false);
  return auto_reauthn_permission_delegate_->RequiresUserMediation(GURL(site));
}

void FederatedAuthRequestImpl::SetRequiresUserMediation(
    bool requires_user_mediation) {
  // Get the domain and set RequiresUserMediation to the given value on it.
  // Include the scheme to make sure that for example that http://domain and
  // https://domain are not treated as the same.
  std::string site = webid::FormatUrlWithDomain(origin().GetURL(),
                                                /*for_display=*/false);
  auto_reauthn_permission_delegate_->SetRequiresUserMediation(
      GURL(site), requires_user_mediation);
}

void FederatedAuthRequestImpl::SignInToIdP(GURL signin_url) {
  permission_delegate_->AddIdpSigninStatusObserver(this);
  ShowModalDialog(signin_url);
}

void FederatedAuthRequestImpl::PreventSilentAccess(
    PreventSilentAccessCallback callback) {
  SetRequiresUserMediation(true);
  if (permission_delegate_->HasSharingPermission(GetEmbeddingOrigin())) {
    PreventSilentAccessFrameType frame_type =
        PreventSilentAccessFrameType::kMainFrame;
    RenderFrameHost* main_rfh = render_frame_host().GetMainFrame();
    if (main_rfh != &render_frame_host()) {
      std::string site =
          webid::FormatUrlWithDomain(origin().GetURL(), /*for_display=*/false);
      std::string embedder =
          webid::FormatUrlWithDomain(GetEmbeddingOrigin().GetURL(),
                                     /*for_display=*/false);
      if (site == embedder) {
        frame_type = PreventSilentAccessFrameType::kSameSiteIframe;
      } else {
        frame_type = PreventSilentAccessFrameType::kCrossSiteIframe;
      }
    }
    RecordPreventSilentAccess(render_frame_host(), frame_type);
  }

  // Send acknowledge response back.
  std::move(callback).Run();
}

void FederatedAuthRequestImpl::Revoke(
    blink::mojom::IdentityCredentialRevokeOptionsPtr options,
    RevokeCallback callback) {
  if (!IsFedCmRevokeEnabled()) {
    // This should only happen when the request comes from a compromised
    // renderer.
    std::move(callback).Run(RevokeStatus::kError);
    return;
  }
  if (!fedcm_metrics_) {
    // Ensure the lifecycle state as GetPageUkmSourceId doesn't support the
    // prerendering page. As FederatedAuthRequest runs behind the
    // BrowserInterfaceBinders, the service doesn't receive any request while
    // prerendering, and the CHECK should always meet the condition.
    CHECK(!render_frame_host().IsInLifecycleState(
        RenderFrameHost::LifecycleState::kPrerendering));
    fedcm_metrics_ = CreateFedCmMetrics(
        options->config->config_url, render_frame_host().GetPageUkmSourceId(),
        /*is_disabled=*/false);
  }
  if (revoke_request_) {
    fedcm_metrics_->RecordRevokeStatus(FedCmRevokeStatus::kTooManyRequests);
    std::move(callback).Run(RevokeStatus::kErrorTooManyRequests);
    return;
  }

  bool intercept = false;
  bool should_complete_request_immediately = false;
  devtools_instrumentation::WillSendFedCmRequest(
      &render_frame_host(), &intercept, &should_complete_request_immediately);
  should_complete_request_immediately_ =
      (intercept && should_complete_request_immediately) ||
      api_permission_delegate_->ShouldCompleteRequestImmediately();

  auto network_manager = CreateNetworkManager();

  revoke_request_ = FederatedAuthRevokeRequest::Create(
      std::move(network_manager), permission_delegate_.get(),
      &render_frame_host(), fedcm_metrics_.get(), std::move(options),
      should_complete_request_immediately_);
  FederatedAuthRevokeRequest* revoke_request_ptr = revoke_request_.get();

  revoke_request_ptr->SetCallbackAndStart(
      base::BindOnce(&FederatedAuthRequestImpl::CompleteRevokeRequest,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      api_permission_delegate_.get());
}

void FederatedAuthRequestImpl::RecordErrorMetrics(
    IdentityProviderRequestOptionsPtr idp,
    TokenResponseType token_response_type,
    absl::optional<ErrorDialogType> error_dialog_type,
    absl::optional<ErrorUrlType> error_url_type) {
  if (!IsFedCmErrorEnabled()) {
    return;
  }

  if (!fedcm_metrics_) {
    // Ensure the lifecycle state as GetPageUkmSourceId doesn't support the
    // prerendering page. As FederatedAuthRequest runs behind the
    // BrowserInterfaceBinders, the service doesn't receive any request while
    // prerendering, and the CHECK should always meet the condition.
    CHECK(!render_frame_host().IsInLifecycleState(
        RenderFrameHost::LifecycleState::kPrerendering));
    fedcm_metrics_ = CreateFedCmMetrics(
        idp->config->config_url, render_frame_host().GetPageUkmSourceId(),
        /*is_disabled=*/false);
  }

  fedcm_metrics_->RecordTokenResponseTypeMetrics(token_response_type);

  if (error_dialog_type) {
    fedcm_metrics_->RecordErrorDialogType(*error_dialog_type);
  }

  if (error_url_type) {
    // This is used to determine if we need to use the cross-site specific
    // devtools issue when failing the request.
    error_url_type_ = error_url_type;
    fedcm_metrics_->RecordErrorUrlTypeMetrics(*error_url_type);
  }
}

}  // namespace content
