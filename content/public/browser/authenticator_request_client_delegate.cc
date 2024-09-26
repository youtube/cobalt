// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/authenticator_request_client_delegate.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/webauth/authenticator_environment.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/webauthn_api.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

WebAuthenticationDelegate::WebAuthenticationDelegate() = default;

WebAuthenticationDelegate::~WebAuthenticationDelegate() = default;

bool WebAuthenticationDelegate::OverrideCallerOriginAndRelyingPartyIdValidation(
    BrowserContext* browser_context,
    const url::Origin& caller_origin,
    const std::string& relying_party_id) {
  // Perform regular security checks for all origins and RP IDs.
  return false;
}

bool WebAuthenticationDelegate::OriginMayUseRemoteDesktopClientOverride(
    BrowserContext* browser_context,
    const url::Origin& caller_origin) {
  // No origin is permitted to claim RP IDs on behalf of another origin.
  return false;
}

bool WebAuthenticationDelegate::IsSecurityLevelAcceptableForWebAuthn(
    content::RenderFrameHost* rfh,
    const url::Origin& caller_origin) {
  return true;
}

#if !BUILDFLAG(IS_ANDROID)
absl::optional<std::string>
WebAuthenticationDelegate::MaybeGetRelyingPartyIdOverride(
    const std::string& claimed_relying_party_id,
    const url::Origin& caller_origin) {
  return absl::nullopt;
}

bool WebAuthenticationDelegate::ShouldPermitIndividualAttestation(
    BrowserContext* browser_context,
    const url::Origin& caller_origin,
    const std::string& relying_party_id) {
  return false;
}

bool WebAuthenticationDelegate::SupportsResidentKeys(
    RenderFrameHost* render_frame_host) {
  // The testing API supports resident keys, but for real requests //content
  // doesn't by default.
  FrameTreeNode* frame_tree_node =
      static_cast<RenderFrameHostImpl*>(render_frame_host)->frame_tree_node();
  if (AuthenticatorEnvironment::GetInstance()->IsVirtualAuthenticatorEnabledFor(
          frame_tree_node)) {
    return true;
  }
  return false;
}

bool WebAuthenticationDelegate::IsFocused(WebContents* web_contents) {
  return true;
}

absl::optional<bool> WebAuthenticationDelegate::
    IsUserVerifyingPlatformAuthenticatorAvailableOverride(
        RenderFrameHost* render_frame_host) {
  FrameTreeNode* frame_tree_node =
      static_cast<RenderFrameHostImpl*>(render_frame_host)->frame_tree_node();
  if (AuthenticatorEnvironment::GetInstance()->IsVirtualAuthenticatorEnabledFor(
          frame_tree_node)) {
    return AuthenticatorEnvironment::GetInstance()
        ->HasVirtualUserVerifyingPlatformAuthenticator(frame_tree_node);
  }
  return absl::nullopt;
}

WebAuthenticationRequestProxy* WebAuthenticationDelegate::MaybeGetRequestProxy(
    BrowserContext* browser_context,
    const url::Origin& caller_origin) {
  return nullptr;
}
#endif  // !IS_ANDROID

#if BUILDFLAG(IS_MAC)
absl::optional<WebAuthenticationDelegate::TouchIdAuthenticatorConfig>
WebAuthenticationDelegate::GetTouchIdAuthenticatorConfig(
    BrowserContext* browser_context) {
  return absl::nullopt;
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
WebAuthenticationDelegate::ChromeOSGenerateRequestIdCallback
WebAuthenticationDelegate::GetGenerateRequestIdCallback(
    RenderFrameHost* render_frame_host) {
  return base::NullCallback();
}
#endif

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
WebAuthenticationDelegate::GetIntentSender(WebContents* web_contents) {
  return nullptr;
}

int WebAuthenticationDelegate::GetSupportLevel(WebContents* web_contents) {
  return 2 /* browser-like support */;
}
#endif

#if !BUILDFLAG(IS_ANDROID)

AuthenticatorRequestClientDelegate::AuthenticatorRequestClientDelegate() =
    default;

AuthenticatorRequestClientDelegate::~AuthenticatorRequestClientDelegate() =
    default;

void AuthenticatorRequestClientDelegate::SetRelyingPartyId(const std::string&) {
}

bool AuthenticatorRequestClientDelegate::DoesBlockRequestOnFailure(
    InterestingFailureReason reason) {
  return false;
}

void AuthenticatorRequestClientDelegate::RegisterActionCallbacks(
    base::OnceClosure cancel_callback,
    base::RepeatingClosure start_over_callback,
    AccountPreselectedCallback account_preselected_callback,
    device::FidoRequestHandlerBase::RequestCallback request_callback,
    base::RepeatingClosure bluetooth_adapter_power_on_callback) {}

void AuthenticatorRequestClientDelegate::ShouldReturnAttestation(
    const std::string& relying_party_id,
    const device::FidoAuthenticator* authenticator,
    bool is_enterprise_attestation,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(!is_enterprise_attestation);
}

void AuthenticatorRequestClientDelegate::ConfigureCable(
    const url::Origin& origin,
    device::FidoRequestType request_type,
    absl::optional<device::ResidentKeyRequirement> resident_key_requirement,
    base::span<const device::CableDiscoveryData> pairings_from_extension,
    device::FidoDiscoveryFactory* fido_discovery_factory) {}

void AuthenticatorRequestClientDelegate::SelectAccount(
    std::vector<device::AuthenticatorGetAssertionResponse> responses,
    base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
        callback) {
  // Automatically choose the first account to allow resident keys for virtual
  // authenticators without a browser implementation, e.g. on content shell.
  // TODO(crbug.com/991666): Provide a way to determine which account gets
  // picked.
  DCHECK(virtual_environment_);
  std::move(callback).Run(std::move(responses.at(0)));
}

void AuthenticatorRequestClientDelegate::DisableUI() {}

bool AuthenticatorRequestClientDelegate::IsWebAuthnUIEnabled() {
  return false;
}

void AuthenticatorRequestClientDelegate::SetVirtualEnvironment(
    bool virtual_environment) {
  virtual_environment_ = virtual_environment;
}

bool AuthenticatorRequestClientDelegate::IsVirtualEnvironmentEnabled() {
  return virtual_environment_;
}

void AuthenticatorRequestClientDelegate::SetConditionalRequest(
    bool is_conditional) {}

void AuthenticatorRequestClientDelegate::SetCredentialIdFilter(
    std::vector<device::PublicKeyCredentialDescriptor>) {}

void AuthenticatorRequestClientDelegate::SetUserEntityForMakeCredentialRequest(
    const device::PublicKeyCredentialUserEntity&) {}

void AuthenticatorRequestClientDelegate::OnTransportAvailabilityEnumerated(
    device::FidoRequestHandlerBase::TransportAvailabilityInfo data) {}

bool AuthenticatorRequestClientDelegate::EmbedderControlsAuthenticatorDispatch(
    const device::FidoAuthenticator& authenticator) {
  return false;
}

void AuthenticatorRequestClientDelegate::BluetoothAdapterPowerChanged(
    bool is_powered_on) {}

void AuthenticatorRequestClientDelegate::FidoAuthenticatorAdded(
    const device::FidoAuthenticator& authenticator) {}

void AuthenticatorRequestClientDelegate::FidoAuthenticatorRemoved(
    base::StringPiece device_id) {}

bool AuthenticatorRequestClientDelegate::SupportsPIN() const {
  return false;
}

void AuthenticatorRequestClientDelegate::CollectPIN(
    CollectPINOptions options,
    base::OnceCallback<void(std::u16string)> provide_pin_cb) {
  NOTREACHED();
}

void AuthenticatorRequestClientDelegate::StartBioEnrollment(
    base::OnceClosure next_callback) {}

void AuthenticatorRequestClientDelegate::OnSampleCollected(
    int bio_samples_remaining) {}

void AuthenticatorRequestClientDelegate::FinishCollectToken() {}

void AuthenticatorRequestClientDelegate::OnRetryUserVerification(int attempts) {
}

#endif  // !IS_ANDROID

}  // namespace content
