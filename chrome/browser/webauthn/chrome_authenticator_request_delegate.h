// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_
#define CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/global_routing_id.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace device {
class PublicKeyCredentialDescriptor;
class PublicKeyCredentialUserEntity;
}  // namespace device

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace device {
class FidoAuthenticator;
class FidoDiscoveryFactory;
}  // namespace device

// ChromeWebAuthenticationDelegate is the //chrome layer implementation of
// content::WebAuthenticationDelegate.
class ChromeWebAuthenticationDelegate
    : public content::WebAuthenticationDelegate {
 public:
#if BUILDFLAG(IS_MAC)
  // Returns a configuration struct for instantiating the macOS WebAuthn
  // platform authenticator for the given Profile.
  static TouchIdAuthenticatorConfig TouchIdAuthenticatorConfigForProfile(
      Profile* profile);
#endif  // BUILDFLAG(IS_MAC)

  ~ChromeWebAuthenticationDelegate() override;

#if !BUILDFLAG(IS_ANDROID)
  // content::WebAuthenticationDelegate:
  bool OverrideCallerOriginAndRelyingPartyIdValidation(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin,
      const std::string& relying_party_id) override;
  bool OriginMayUseRemoteDesktopClientOverride(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin) override;
  bool IsSecurityLevelAcceptableForWebAuthn(
      content::RenderFrameHost* rfh,
      const url::Origin& caller_origin) override;
  absl::optional<std::string> MaybeGetRelyingPartyIdOverride(
      const std::string& claimed_relying_party_id,
      const url::Origin& caller_origin) override;
  bool ShouldPermitIndividualAttestation(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin,
      const std::string& relying_party_id) override;
  bool SupportsResidentKeys(
      content::RenderFrameHost* render_frame_host) override;
  bool IsFocused(content::WebContents* web_contents) override;
  absl::optional<bool> IsUserVerifyingPlatformAuthenticatorAvailableOverride(
      content::RenderFrameHost* render_frame_host) override;
  content::WebAuthenticationRequestProxy* MaybeGetRequestProxy(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin) override;
#endif
#if BUILDFLAG(IS_MAC)
  absl::optional<TouchIdAuthenticatorConfig> GetTouchIdAuthenticatorConfig(
      content::BrowserContext* browser_context) override;
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_CHROMEOS)
  ChromeOSGenerateRequestIdCallback GetGenerateRequestIdCallback(
      content::RenderFrameHost* render_frame_host) override;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

class ChromeAuthenticatorRequestDelegate
    : public content::AuthenticatorRequestClientDelegate,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  // TestObserver is an interface that observes certain events related to this
  // class for testing purposes. Only a single instance of this interface can
  // be installed at a given time.
  class TestObserver {
   public:
    virtual void Created(ChromeAuthenticatorRequestDelegate* delegate) = 0;

    virtual std::vector<std::unique_ptr<device::cablev2::Pairing>>
    GetCablePairingsFromSyncedDevices() = 0;

    virtual void OnTransportAvailabilityEnumerated(
        ChromeAuthenticatorRequestDelegate* delegate,
        device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai) = 0;

    virtual void UIShown(ChromeAuthenticatorRequestDelegate* delegate) = 0;

    virtual void CableV2ExtensionSeen(
        base::span<const uint8_t> server_link_data) = 0;

    virtual void ConfiguringCable(device::FidoRequestType request_type) {}

    virtual void AccountSelectorShown(
        const std::vector<device::AuthenticatorGetAssertionResponse>&
            responses) {}
  };

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  // The |render_frame_host| must outlive this instance.
  explicit ChromeAuthenticatorRequestDelegate(
      content::RenderFrameHost* render_frame_host);

  ChromeAuthenticatorRequestDelegate(
      const ChromeAuthenticatorRequestDelegate&) = delete;
  ChromeAuthenticatorRequestDelegate& operator=(
      const ChromeAuthenticatorRequestDelegate&) = delete;

  ~ChromeAuthenticatorRequestDelegate() override;

  // SetGlobalObserverForTesting sets the single |TestObserver| that is active
  // at a given time. Call be called with |nullptr| to unregister a
  // |TestObserver|. It is a fatal error to try and register a |TestObserver|
  // while one is still installed.
  static void SetGlobalObserverForTesting(TestObserver*);

  base::WeakPtr<ChromeAuthenticatorRequestDelegate> AsWeakPtr();

  AuthenticatorRequestDialogModel* dialog_model() const {
    return dialog_model_.get();
  }

  // content::AuthenticatorRequestClientDelegate:
  void SetRelyingPartyId(const std::string& rp_id) override;
  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override;
  void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::RepeatingClosure start_over_callback,
      AccountPreselectedCallback account_preselected_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback) override;
  void ShouldReturnAttestation(
      const std::string& relying_party_id,
      const device::FidoAuthenticator* authenticator,
      bool is_enterprise_attestation,
      base::OnceCallback<void(bool)> callback) override;
  void ConfigureCable(
      const url::Origin& origin,
      device::FidoRequestType request_type,
      absl::optional<device::ResidentKeyRequirement> resident_key_requirement,
      base::span<const device::CableDiscoveryData> pairings_from_extension,
      device::FidoDiscoveryFactory* discovery_factory) override;
  void SelectAccount(
      std::vector<device::AuthenticatorGetAssertionResponse> responses,
      base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
          callback) override;
  void DisableUI() override;
  bool IsWebAuthnUIEnabled() override;
  void SetConditionalRequest(bool is_conditional) override;
  void SetCredentialIdFilter(std::vector<device::PublicKeyCredentialDescriptor>
                                 credential_list) override;
  void SetUserEntityForMakeCredentialRequest(
      const device::PublicKeyCredentialUserEntity& user_entity) override;

  // device::FidoRequestHandlerBase::Observer:
  void OnTransportAvailabilityEnumerated(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo data) override;
  bool EmbedderControlsAuthenticatorDispatch(
      const device::FidoAuthenticator& authenticator) override;
  void FidoAuthenticatorAdded(
      const device::FidoAuthenticator& authenticator) override;
  void FidoAuthenticatorRemoved(base::StringPiece authenticator_id) override;
  void BluetoothAdapterPowerChanged(bool is_powered_on) override;
  bool SupportsPIN() const override;
  void CollectPIN(
      CollectPINOptions options,
      base::OnceCallback<void(std::u16string)> provide_pin_cb) override;
  void StartBioEnrollment(base::OnceClosure next_callback) override;
  void OnSampleCollected(int bio_samples_remaining) override;
  void FinishCollectToken() override;
  void OnRetryUserVerification(int attempts) override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnStartOver() override;
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override;
  void OnCancelRequest() override;
  void OnManageDevicesClicked() override;

  // A non-const version of dialog_model().
  AuthenticatorRequestDialogModel* GetDialogModelForTesting();

  // SetPassEmptyUsbDeviceManagerForTesting controls whether the
  // `DiscoveryFactory` will be given an empty USB device manager. This is
  // needed in tests because creating a real `device::mojom::UsbDeviceManager`
  // can create objects on thread-pool threads. Those objects aren't scheduled
  // for deletion until after the thread-pool is shutdown when testing, causing
  // "leaks" to be reported.
  void SetPassEmptyUsbDeviceManagerForTesting(bool value);

  FRIEND_TEST_ALL_PREFIXES(ChromeAuthenticatorRequestDelegateTest,
                           TestTransportPrefType);
  FRIEND_TEST_ALL_PREFIXES(ChromeAuthenticatorRequestDelegateTest,
                           TestPairedDeviceAddressPreference);

  // GetRenderFrameHost returns a pointer to the RenderFrameHost that was given
  // to the constructor.
  content::RenderFrameHost* GetRenderFrameHost() const;

  content::BrowserContext* GetBrowserContext() const;

  absl::optional<device::FidoTransportProtocol> GetLastTransportUsed() const;

  // ShouldPermitCableExtension returns true if the given |origin| may set a
  // caBLE extension. This extension contains website-chosen BLE pairing
  // information that will be broadcast by the device.
  bool ShouldPermitCableExtension(const url::Origin& origin);

  void OnInvalidatedCablePairing(size_t failed_contact_index);

  const content::GlobalRenderFrameHostId render_frame_host_id_;
  const std::unique_ptr<AuthenticatorRequestDialogModel> dialog_model_;
  base::OnceClosure cancel_callback_;
  base::RepeatingClosure start_over_callback_;
  AccountPreselectedCallback account_preselected_callback_;
  device::FidoRequestHandlerBase::RequestCallback request_callback_;

  // The next two fields are the same length and contain the names and public
  // keys of paired phones.
  std::vector<std::string> phone_names_;
  std::vector<std::array<uint8_t, device::kP256X962Length>> phone_public_keys_;

  // If in the TransportAvailabilityInfo reported by the request handler,
  // disable_embedder_ui is set, this will be set to true. No UI must be
  // rendered and all request handler callbacks will be ignored.
  bool disable_ui_ = false;

  // If true, show a more subtle UI unless the user has platform discoverable
  // credentials on the device.
  bool is_conditional_ = false;

  // A list of credentials used to filter passkeys by ID. When non-empty,
  // non-matching passkeys will not be displayed during conditional mediation
  // requests. When empty, no filter is applied and all passkeys are displayed.
  std::vector<device::PublicKeyCredentialDescriptor> credential_filter_;

  // See `SetPassEmptyUsbDeviceManagerForTesting`.
  bool pass_empty_usb_device_manager_ = false;

 private:
  base::WeakPtrFactory<ChromeAuthenticatorRequestDelegate> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_
