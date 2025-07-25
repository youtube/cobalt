// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"

#include <algorithm>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device_authenticator.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/authenticator.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "device/fido/win/webauthn_api.h"
#include "third_party/microsoft_webauthn/webauthn.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/authenticator_config.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

static constexpr char kRelyingPartyID[] = "example.com";

class Observer : public testing::NiceMock<
                     ChromeAuthenticatorRequestDelegate::TestObserver> {
 public:
  MOCK_METHOD(void,
              Created,
              (ChromeAuthenticatorRequestDelegate * delegate),
              (override));
  MOCK_METHOD(std::vector<std::unique_ptr<device::cablev2::Pairing>>,
              GetCablePairingsFromSyncedDevices,
              (),
              (override));
  MOCK_METHOD(void,
              OnTransportAvailabilityEnumerated,
              (ChromeAuthenticatorRequestDelegate * delegate,
               device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai),
              (override));
  MOCK_METHOD(void,
              UIShown,
              (ChromeAuthenticatorRequestDelegate * delegate),
              (override));
  MOCK_METHOD(void,
              CableV2ExtensionSeen,
              (base::span<const uint8_t> server_link_data),
              (override));
};

class MockCableDiscoveryFactory : public device::FidoDiscoveryFactory {
 public:
  void set_cable_data(
      device::FidoRequestType request_type,
      std::vector<device::CableDiscoveryData> data,
      const absl::optional<std::array<uint8_t, device::cablev2::kQRKeySize>>&
          qr_generator_key) override {
    cable_data = std::move(data);
    qr_key = qr_generator_key;
  }

  void set_android_accessory_params(
      mojo::Remote<device::mojom::UsbDeviceManager>,
      std::string aoa_request_description) override {
    this->aoa_configured = true;
  }

  std::vector<device::CableDiscoveryData> cable_data;
  absl::optional<std::array<uint8_t, device::cablev2::kQRKeySize>> qr_key;
  bool aoa_configured = false;
};

class ChromeAuthenticatorRequestDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeAuthenticatorRequestDelegateTest() {
    scoped_feature_list_.InitWithFeatures(
        {syncer::kSyncWebauthnCredentials, syncer::kSyncWebauthnCredentials,
         device::kWebAuthnNewPasskeyUI},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PasskeyModelFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<webauthn::TestPasskeyModel>();
            }));
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(&observer_);
  }

  void TearDown() override {
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  Observer observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class TestAuthenticatorModelObserver final
    : public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit TestAuthenticatorModelObserver(
      AuthenticatorRequestDialogModel* model)
      : model_(model) {
    last_step_ = model_->current_step();
  }
  ~TestAuthenticatorModelObserver() override {
    if (model_) {
      model_->RemoveObserver(this);
    }
  }

  AuthenticatorRequestDialogModel::Step last_step() { return last_step_; }

  // AuthenticatorRequestDialogModel::Observer:
  void OnStepTransition() override { last_step_ = model_->current_step(); }

  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override {
    model_ = nullptr;
  }

 private:
  raw_ptr<AuthenticatorRequestDialogModel> model_;
  AuthenticatorRequestDialogModel::Step last_step_;
};

TEST_F(ChromeAuthenticatorRequestDelegateTest, IndividualAttestation) {
  static const struct TestCase {
    std::string name;
    std::string origin;
    std::string rp_id;
    std::string enterprise_attestation_switch_value;
    std::vector<std::string> permit_attestation_policy_values;
    bool expected;
  } kTestCases[] = {
      {"Basic", "https://login.example.com", "example.com", "", {}, false},
      {"Policy permits RP ID",
       "https://login.example.com",
       "example.com",
       "",
       {"example.com", "other.com"},
       true},
      {"Policy doesn't permit RP ID",
       "https://login.example.com",
       "example.com",
       "",
       {"other.com", "login.example.com", "https://example.com",
        "http://example.com", "https://login.example.com", "com", "*"},
       false},
      {"Policy doesn't care about the origin",
       "https://login.example.com",
       "example.com",
       "",
       {"https://login.example.com", "https://example.com"},
       false},
      {"Switch permits origin",
       "https://login.example.com",
       "example.com",
       "https://login.example.com,https://other.com,xyz:/invalidorigin",
       {},
       true},
      {"Switch doesn't permit origin",
       "https://login.example.com",
       "example.com",
       "example.com,login.example.com,http://login.example.com,https://"
       "example.com,https://a.login.example.com,https://*.example.com",
       {},
       false},
  };
  for (const auto& test : kTestCases) {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        webauthn::switches::kPermitEnterpriseAttestationOriginList,
        test.enterprise_attestation_switch_value);
    PrefService* prefs =
        Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
    if (!test.permit_attestation_policy_values.empty()) {
      base::Value::List policy_values;
      for (const std::string& v : test.permit_attestation_policy_values) {
        policy_values.Append(v);
      }
      prefs->SetList(prefs::kSecurityKeyPermitAttestation,
                     std::move(policy_values));
    } else {
      prefs->ClearPref(prefs::kSecurityKeyPermitAttestation);
    }
    ChromeWebAuthenticationDelegate delegate;
    EXPECT_EQ(delegate.ShouldPermitIndividualAttestation(
                  GetBrowserContext(), url::Origin::Create(GURL(test.origin)),
                  test.rp_id),
              test.expected)
        << test.name;
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, CableConfiguration) {
  const std::array<uint8_t, 16> eid = {1, 2, 3, 4};
  const std::array<uint8_t, 32> prekey = {5, 6, 7, 8};
  const device::CableDiscoveryData v1_extension(
      device::CableDiscoveryData::Version::V1, eid, eid, prekey);

  device::CableDiscoveryData v2_extension;
  v2_extension.version = device::CableDiscoveryData::Version::V2;
  v2_extension.v2.emplace(std::vector<uint8_t>(prekey.begin(), prekey.end()),
                          std::vector<uint8_t>());

  enum class Result {
    kNone,
    kV1,
    kServerLink,
    k3rdParty,
  };

#if BUILDFLAG(IS_LINUX)
  // On Linux, some configurations aren't supported because of bluez
  // limitations. This macro maps the expected result in that case.
#define NONE_ON_LINUX(r) (Result::kNone)
#else
#define NONE_ON_LINUX(r) (r)
#endif

  const struct {
    const char* origin;
    std::vector<device::CableDiscoveryData> extensions;
    device::FidoRequestType request_type;
    absl::optional<device::ResidentKeyRequirement> resident_key_requirement;
    Result expected_result;
    // expected_result_with_system_hybrid is the behaviour that should occur
    // when the operating system supports hybrid itself. (I.e. recent versions
    // of Windows.)
    Result expected_result_with_system_hybrid;
  } kTests[] = {
      {
          "https://example.com",
          {},
          device::FidoRequestType::kGetAssertion,
          absl::nullopt,
          Result::k3rdParty,
          Result::kNone,
      },
      {
          // Extensions should be ignored on a 3rd-party site.
          "https://example.com",
          {v1_extension},
          device::FidoRequestType::kGetAssertion,
          absl::nullopt,
          Result::k3rdParty,
          Result::kNone,
      },
      {
          // Extensions should be ignored on a 3rd-party site.
          "https://example.com",
          {v2_extension},
          device::FidoRequestType::kGetAssertion,
          absl::nullopt,
          Result::k3rdParty,
          Result::kNone,
      },
      {
          // a.g.c should still be able to get 3rd-party caBLE
          // if it doesn't send an extension in an assertion request.
          "https://accounts.google.com",
          {},
          device::FidoRequestType::kGetAssertion,
          absl::nullopt,
          Result::k3rdParty,
          Result::kNone,
      },
      {
          // ... but not for non-discoverable registration.
          "https://accounts.google.com",
          {},
          device::FidoRequestType::kMakeCredential,
          device::ResidentKeyRequirement::kDiscouraged,
          Result::kNone,
          Result::kNone,
      },
      {
          // ... but yes for rk=preferred
          "https://accounts.google.com",
          {},
          device::FidoRequestType::kMakeCredential,
          device::ResidentKeyRequirement::kPreferred,
          Result::k3rdParty,
          Result::kNone,
      },
      {
          // ... or rk=required.
          "https://accounts.google.com",
          {},
          device::FidoRequestType::kMakeCredential,
          device::ResidentKeyRequirement::kRequired,
          Result::k3rdParty,
          Result::kNone,
      },
      {
          "https://accounts.google.com",
          {v1_extension},
          device::FidoRequestType::kGetAssertion,
          absl::nullopt,
          NONE_ON_LINUX(Result::kV1),
          Result::kV1,
      },
      {
          "https://accounts.google.com",
          {v2_extension},
          device::FidoRequestType::kGetAssertion,
          absl::nullopt,
          Result::kServerLink,
          Result::kServerLink,
      },
  };

  // On Windows, all the tests are run twice. Once to check that, when Windows
  // has hybrid support, it's not also configured in Chrome, and again to test
  // the prior behaviour.

#if BUILDFLAG(IS_WIN)
  device::FakeWinWebAuthnApi fake_win_webauthn_api;
  device::WinWebAuthnApi::ScopedOverride win_webauthn_api_override(
      &fake_win_webauthn_api);
#endif

  enum WinHybridExpectation {
    kNoWinHybrid,
    kWinHybridPasskeySyncing,
    kWinHybridNoPasskeySyncing,
  };

  for (const WinHybridExpectation windows_has_hybrid : {
         kNoWinHybrid,
#if BUILDFLAG(IS_WIN)
             kWinHybridPasskeySyncing, kWinHybridNoPasskeySyncing,
#endif
       }) {
    unsigned test_case = 0;
    for (const auto& test : kTests) {
      SCOPED_TRACE(test_case);
      test_case++;

#if BUILDFLAG(IS_WIN)
      fake_win_webauthn_api.set_version(windows_has_hybrid == kNoWinHybrid ? 4
                                                                           : 7);
      base::test::ScopedFeatureList scoped_feature_list;
      if (windows_has_hybrid == kWinHybridNoPasskeySyncing) {
        scoped_feature_list.InitWithFeatures(
            {}, {syncer::kSyncWebauthnCredentials});
      } else if (windows_has_hybrid == kWinHybridPasskeySyncing) {
        scoped_feature_list.InitWithFeatures({syncer::kSyncWebauthnCredentials},
                                             {});
      }
      SCOPED_TRACE(windows_has_hybrid);
#endif

      MockCableDiscoveryFactory discovery_factory;
      ChromeAuthenticatorRequestDelegate delegate(main_rfh());
      delegate.SetRelyingPartyId(/*rp_id=*/"example.com");
      delegate.SetPassEmptyUsbDeviceManagerForTesting(true);
      delegate.ConfigureDiscoveries(
          url::Origin::Create(GURL(test.origin)), test.origin,
          content::AuthenticatorRequestClientDelegate::RequestSource::
              kWebAuthentication,
          test.request_type, test.resident_key_requirement, test.extensions,
          &discovery_factory);

      switch (windows_has_hybrid == kWinHybridNoPasskeySyncing
                  ? test.expected_result_with_system_hybrid
                  : test.expected_result) {
        case Result::kNone:
          EXPECT_FALSE(discovery_factory.qr_key.has_value());
          EXPECT_TRUE(discovery_factory.cable_data.empty());
          EXPECT_TRUE(discovery_factory.aoa_configured);
          break;

        case Result::kV1:
          EXPECT_FALSE(discovery_factory.qr_key.has_value());
          EXPECT_FALSE(discovery_factory.cable_data.empty());
          EXPECT_TRUE(discovery_factory.aoa_configured);
          EXPECT_EQ(delegate.dialog_model()->cable_ui_type(),
                    AuthenticatorRequestDialogModel::CableUIType::CABLE_V1);
          break;

        case Result::kServerLink:
          EXPECT_TRUE(discovery_factory.qr_key.has_value());
          EXPECT_FALSE(discovery_factory.cable_data.empty());
          EXPECT_TRUE(discovery_factory.aoa_configured);
          EXPECT_EQ(delegate.dialog_model()->cable_ui_type(),
                    AuthenticatorRequestDialogModel::CableUIType::
                        CABLE_V2_SERVER_LINK);
          break;

        case Result::k3rdParty:
          EXPECT_TRUE(discovery_factory.qr_key.has_value());
          EXPECT_TRUE(discovery_factory.cable_data.empty());
          EXPECT_TRUE(discovery_factory.aoa_configured);
          EXPECT_EQ(delegate.dialog_model()->cable_ui_type(),
                    AuthenticatorRequestDialogModel::CableUIType::
                        CABLE_V2_2ND_FACTOR);
          break;
      }
    }
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, NoExtraDiscoveriesWithoutUI) {
  const std::string rp_id = "https://example.com";
  const std::string origin = "https://" + rp_id;

  for (const bool disable_ui : {false, true}) {
    SCOPED_TRACE(disable_ui);

    ChromeAuthenticatorRequestDelegate delegate(main_rfh());
    delegate.SetRelyingPartyId(rp_id);
    delegate.SetPassEmptyUsbDeviceManagerForTesting(true);
    if (disable_ui) {
      delegate.DisableUI();
    }
    MockCableDiscoveryFactory discovery_factory;
    delegate.ConfigureDiscoveries(url::Origin::Create(GURL(origin)), origin,
                                  content::AuthenticatorRequestClientDelegate::
                                      RequestSource::kWebAuthentication,
                                  device::FidoRequestType::kMakeCredential,
                                  device::ResidentKeyRequirement::kPreferred,
                                  {}, &discovery_factory);

    EXPECT_EQ(discovery_factory.qr_key.has_value(), !disable_ui);
    EXPECT_EQ(discovery_factory.aoa_configured, !disable_ui);

    // `discovery_factory.nswindow_` won't be set in any case because it depends
    // on the `RenderFrameHost` having a `BrowserWindow`, which it doesn't in
    // this context.
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, ConditionalUI) {
  // The RenderFrame has to be live for the ChromeWebAuthnCredentialsDelegate to
  // be created.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://example.com"));
  ChromeWebAuthnCredentialsDelegateFactory::CreateForWebContents(
      web_contents());

  // Enabling conditional mode should cause the modal dialog to stay hidden at
  // the beginning of a request. An omnibar icon might be shown instead.
  for (bool conditional_ui : {true, false}) {
    ChromeAuthenticatorRequestDelegate delegate(main_rfh());
    delegate.SetConditionalRequest(conditional_ui);
    delegate.SetRelyingPartyId(/*rp_id=*/"example.com");
    AuthenticatorRequestDialogModel* model = delegate.dialog_model();
    TestAuthenticatorModelObserver observer(model);
    model->AddObserver(&observer);
    EXPECT_EQ(observer.last_step(),
              AuthenticatorRequestDialogModel::Step::kNotStarted);
    delegate.OnTransportAvailabilityEnumerated(
        AuthenticatorRequestDialogModel::TransportAvailabilityInfo());
    EXPECT_EQ(observer.last_step() ==
                  AuthenticatorRequestDialogModel::Step::kConditionalMediation,
              conditional_ui);
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       OverrideValidateDomainAndRelyingPartyIDTest) {
  constexpr char kTestExtensionOrigin[] = "chrome-extension://abcdef";
  static const struct {
    std::string rp_id;
    std::string origin;
    bool expected;
  } kTests[] = {
      {"example.com", "https://example.com", false},
      {"foo.com", "https://example.com", false},
      {"abcdef", kTestExtensionOrigin, true},
      {"abcdefg", kTestExtensionOrigin, false},
      {"example.com", kTestExtensionOrigin, false},
  };

  ChromeWebAuthenticationDelegate delegate;
  for (const auto& test : kTests) {
    EXPECT_EQ(delegate.OverrideCallerOriginAndRelyingPartyIdValidation(
                  GetBrowserContext(), url::Origin::Create(GURL(test.origin)),
                  test.rp_id),
              test.expected);
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, MaybeGetRelyingPartyIdOverride) {
  constexpr char kTestExtensionOrigin[] = "chrome-extension://abcdef";
  ChromeWebAuthenticationDelegate delegate;
  static const struct {
    std::string rp_id;
    std::string origin;
    absl::optional<std::string> expected;
  } kTests[] = {
      {"example.com", "https://example.com", absl::nullopt},
      {"foo.com", "https://example.com", absl::nullopt},
      {"abcdef", kTestExtensionOrigin, kTestExtensionOrigin},
      {"example.com", kTestExtensionOrigin, kTestExtensionOrigin},
  };
  for (const auto& test : kTests) {
    EXPECT_EQ(delegate.MaybeGetRelyingPartyIdOverride(
                  test.rp_id, url::Origin::Create(GURL(test.origin))),
              test.expected);
  }
}

// Tests that attestation is returned if the virtual environment is enabled and
// the UI is disabled.
// Regression test for crbug.com/1342458
TEST_F(ChromeAuthenticatorRequestDelegateTest, VirtualEnvironmentAttestation) {
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  delegate.DisableUI();
  delegate.SetVirtualEnvironment(true);
  device::VirtualFidoDeviceAuthenticator authenticator(
      std::make_unique<device::VirtualCtap2Device>());
  device::test::ValueCallbackReceiver<bool> cb;
  delegate.ShouldReturnAttestation(kRelyingPartyID, &authenticator,
                                   /*is_enterprise_attestation=*/false,
                                   cb.callback());
  EXPECT_TRUE(cb.value());
}

// Tests that synced GPM passkeys are injected in the transport availability
// info.
TEST_F(ChromeAuthenticatorRequestDelegateTest, GpmPasskeys) {
  std::string relying_party = "example.com";
  GURL url("https://example.com");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
  ChromeWebAuthnCredentialsDelegateFactory::CreateForWebContents(
      web_contents());
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  delegate.SetPassEmptyUsbDeviceManagerForTesting(true);
  delegate.SetRelyingPartyId(relying_party);

  // Set up a paired phone from sync.
  auto phone = std::make_unique<device::cablev2::Pairing>();
  phone->name = "Miku's Pixel 7 XL";
  phone->contact_id = {1, 2, 3, 4};
  phone->id = {5, 6, 7, 8};
  phone->from_sync_deviceinfo = true;
  std::vector<std::unique_ptr<device::cablev2::Pairing>> phones;
  phones.emplace_back(std::move(phone));
  EXPECT_CALL(observer_, GetCablePairingsFromSyncedDevices)
      .WillOnce(testing::Return(testing::ByMove(std::move(phones))));
  MockCableDiscoveryFactory discovery_factory;
  delegate.ConfigureDiscoveries(
      url::Origin::Create(url), relying_party,
      content::AuthenticatorRequestClientDelegate::RequestSource::
          kWebAuthentication,
      device::FidoRequestType::kGetAssertion,
      /*resident_key_requirement=*/absl::nullopt,
      /*pairings_from_extension=*/std::vector<device::CableDiscoveryData>(),
      &discovery_factory);

  // Add a synced passkey for example.com and another for othersite.com.
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(profile());
  ASSERT_TRUE(passkey_model);
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(std::string(16, 'a'));
  passkey.set_credential_id(std::string(16, 'b'));
  passkey.set_rp_id("example.com");
  passkey.set_user_id(std::string({5, 6, 7, 8}));
  passkey.set_user_name("hmiku");
  passkey.set_user_display_name("Hatsune Miku");

  sync_pb::WebauthnCredentialSpecifics passkey_other_rp_id = passkey;
  passkey_other_rp_id.set_rp_id("othersite.com");

  passkey_model->AddNewPasskeyForTesting(std::move(passkey));
  passkey_model->AddNewPasskeyForTesting(std::move(passkey_other_rp_id));

  AuthenticatorRequestDialogModel::TransportAvailabilityInfo tai;
  EXPECT_CALL(observer_, OnTransportAvailabilityEnumerated)
      .WillOnce([&tai](const auto* _, const auto* new_tai) {
        tai = std::move(*new_tai);
      });
  delegate.OnTransportAvailabilityEnumerated(tai);

  // The GPM passkey for example.com should have been added to the recognized
  // credentials list.
  ASSERT_EQ(tai.recognized_credentials.size(), 1u);
  const device::DiscoverableCredentialMetadata credential =
      tai.recognized_credentials.at(0);
  EXPECT_EQ(credential.cred_id, std::vector<uint8_t>(16, 'b'));
  EXPECT_EQ(credential.rp_id, "example.com");
  EXPECT_EQ(credential.source, device::AuthenticatorType::kPhone);
  EXPECT_EQ(credential.user.display_name, "Hatsune Miku");
  EXPECT_EQ(credential.user.name, "hmiku");
  EXPECT_EQ(credential.user.id, std::vector<uint8_t>({5, 6, 7, 8}));
}

// Tests that synced GPM passkeys are not discovered if there are no sync paired
// phones.
TEST_F(ChromeAuthenticatorRequestDelegateTest, GpmPasskeys_NoSyncPairedPhones) {
  std::string relying_party = "example.com";
  GURL url("https://example.com");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
  ChromeWebAuthnCredentialsDelegateFactory::CreateForWebContents(
      web_contents());
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  delegate.SetPassEmptyUsbDeviceManagerForTesting(true);
  delegate.SetRelyingPartyId(relying_party);

  // Return an empty list of synced devices.
  EXPECT_CALL(observer_, GetCablePairingsFromSyncedDevices);
  MockCableDiscoveryFactory discovery_factory;
  delegate.ConfigureDiscoveries(
      url::Origin::Create(url), relying_party,
      content::AuthenticatorRequestClientDelegate::RequestSource::
          kWebAuthentication,
      device::FidoRequestType::kGetAssertion,
      /*resident_key_requirement=*/absl::nullopt,
      /*pairings_from_extension=*/std::vector<device::CableDiscoveryData>(),
      &discovery_factory);

  // Add a synced passkey for example.com.
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(profile());
  ASSERT_TRUE(passkey_model);
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(std::string(16, 'a'));
  passkey.set_credential_id(std::string(16, 'b'));
  passkey.set_rp_id(relying_party);
  passkey.set_user_id(std::string({5, 6, 7, 8}));
  passkey_model->AddNewPasskeyForTesting(std::move(passkey));

  AuthenticatorRequestDialogModel::TransportAvailabilityInfo tai;
  EXPECT_CALL(observer_, OnTransportAvailabilityEnumerated)
      .WillOnce([&tai](const auto* _, const auto* new_tai) {
        tai = std::move(*new_tai);
      });
  delegate.OnTransportAvailabilityEnumerated(tai);

  // The GPM passkey should not be present in the recognized credentials list.
  EXPECT_TRUE(tai.recognized_credentials.empty());
}

// Tests that shadowed GPM passkeys are not discovered.
TEST_F(ChromeAuthenticatorRequestDelegateTest, GpmPasskeys_ShadowedPasskeys) {
  std::string relying_party = "example.com";
  GURL url("https://example.com");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
  ChromeWebAuthnCredentialsDelegateFactory::CreateForWebContents(
      web_contents());
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  delegate.SetPassEmptyUsbDeviceManagerForTesting(true);
  delegate.SetRelyingPartyId(relying_party);

  // Set up a paired phone from sync.
  auto phone = std::make_unique<device::cablev2::Pairing>();
  phone->name = "Miku's Pixel 7 XL";
  phone->contact_id = {1, 2, 3, 4};
  phone->id = {5, 6, 7, 8};
  phone->from_sync_deviceinfo = true;
  std::vector<std::unique_ptr<device::cablev2::Pairing>> phones;
  phones.emplace_back(std::move(phone));
  EXPECT_CALL(observer_, GetCablePairingsFromSyncedDevices)
      .WillOnce(testing::Return(testing::ByMove(std::move(phones))));
  MockCableDiscoveryFactory discovery_factory;
  delegate.ConfigureDiscoveries(
      url::Origin::Create(url), relying_party,
      content::AuthenticatorRequestClientDelegate::RequestSource::
          kWebAuthentication,
      device::FidoRequestType::kGetAssertion,
      /*resident_key_requirement=*/absl::nullopt,
      /*pairings_from_extension=*/std::vector<device::CableDiscoveryData>(),
      &discovery_factory);

  // Add a synced passkey for example.com and another that shadows it.
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(profile());
  ASSERT_TRUE(passkey_model);
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(std::string(16, 'a'));
  passkey.set_credential_id(std::string(16, 'b'));
  passkey.set_rp_id(relying_party);
  passkey.set_user_id(std::string({5, 6, 7, 8}));
  passkey.set_user_name("hmiku");
  passkey.set_user_display_name("Hatsune Miku");

  sync_pb::WebauthnCredentialSpecifics shadowed_passkey = passkey;
  shadowed_passkey.set_credential_id(std::string(16, 'c'));
  passkey.add_newly_shadowed_credential_ids(shadowed_passkey.credential_id());

  passkey_model->AddNewPasskeyForTesting(std::move(passkey));
  passkey_model->AddNewPasskeyForTesting(std::move(shadowed_passkey));

  AuthenticatorRequestDialogModel::TransportAvailabilityInfo tai;
  EXPECT_CALL(observer_, OnTransportAvailabilityEnumerated)
      .WillOnce([&tai](const auto* _, const auto* new_tai) {
        tai = std::move(*new_tai);
      });
  delegate.OnTransportAvailabilityEnumerated(tai);

  // The GPM passkey that is not shadowed should have been added to the
  // recognized credentials list.
  ASSERT_EQ(tai.recognized_credentials.size(), 1u);
  const device::DiscoverableCredentialMetadata credential =
      tai.recognized_credentials.at(0);
  EXPECT_EQ(credential.cred_id, std::vector<uint8_t>(16, 'b'));
  EXPECT_EQ(credential.rp_id, relying_party);
  EXPECT_EQ(credential.source, device::AuthenticatorType::kPhone);
  EXPECT_EQ(credential.user.display_name, "Hatsune Miku");
  EXPECT_EQ(credential.user.name, "hmiku");
  EXPECT_EQ(credential.user.id, std::vector<uint8_t>({5, 6, 7, 8}));
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, FilterGoogleComPasskeys) {
  base::test::ScopedFeatureList scoped_feature_list{
      device::kWebAuthnFilterGooglePasskeys};
  auto HasCreds = device::FidoRequestHandlerBase::RecognizedCredential::
      kHasRecognizedCredential;
  auto NoCreds = device::FidoRequestHandlerBase::RecognizedCredential::
      kNoRecognizedCredential;
  auto UnknownCreds =
      device::FidoRequestHandlerBase::RecognizedCredential::kUnknown;
  constexpr char kGoogle[] = "google.com";
  constexpr char kOtherRpId[] = "example.com";
  struct {
    std::string rp_id;
    device::FidoRequestHandlerBase::RecognizedCredential recognized_credential;
    std::vector<std::string> user_ids;

    device::FidoRequestHandlerBase::RecognizedCredential
        expected_recognized_credential;
    std::vector<std::string> expected_user_ids;
  } kTests[] = {
      {kOtherRpId,
       HasCreds,
       {"GOOGLE_ACCOUNT:c1", "c2"},
       HasCreds,
       {"GOOGLE_ACCOUNT:c1", "c2"}},
      {kGoogle,
       HasCreds,
       {"GOOGLE_ACCOUNT:c1", "c2", "AUTOFILL_AUTH:c3"},
       HasCreds,
       {"GOOGLE_ACCOUNT:c1"}},
      {kGoogle, HasCreds, {"c2", "AUTOFILL_AUTH:c3"}, NoCreds, {}},
      {kGoogle, UnknownCreds, {}, UnknownCreds, {}},
      {kGoogle, HasCreds, {}, HasCreds, {}},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(::testing::Message() << "rp_id=" << test.rp_id);
    SCOPED_TRACE(::testing::Message()
                 << "creds=" << base::JoinString(test.user_ids, ","));
    device::FidoRequestHandlerBase::TransportAvailabilityInfo data;
    device::FidoRequestHandlerBase::TransportAvailabilityInfo result;
    EXPECT_CALL(observer_, OnTransportAvailabilityEnumerated)
        .WillOnce([&result](const auto* _, const auto* new_tai) {
          result = std::move(*new_tai);
        });

    for (const std::string& user_id : test.user_ids) {
      data.recognized_credentials.emplace_back(
          device::AuthenticatorType::kOther, test.rp_id,
          std::vector<uint8_t>{0},
          device::PublicKeyCredentialUserEntity(
              std::vector<uint8_t>(user_id.begin(), user_id.end())));
    }
    data.has_platform_authenticator_credential = test.recognized_credential;

    // Mix in an icloud keychain credential. These should not be filtered or
    // affect setting the recognized credentials flag.
    data.recognized_credentials.emplace_back(
        device::AuthenticatorType::kICloudKeychain, test.rp_id,
        std::vector<uint8_t>{0}, device::PublicKeyCredentialUserEntity({1}));
    data.has_icloud_keychain_credential = device::FidoRequestHandlerBase::
        RecognizedCredential::kHasRecognizedCredential;

    ChromeAuthenticatorRequestDelegate delegate(main_rfh());
    delegate.SetRelyingPartyId(test.rp_id);
    delegate.OnTransportAvailabilityEnumerated(std::move(data));

    EXPECT_EQ(result.has_platform_authenticator_credential,
              test.expected_recognized_credential);
    EXPECT_EQ(result.has_icloud_keychain_credential,
              device::FidoRequestHandlerBase::RecognizedCredential::
                  kHasRecognizedCredential);
    ASSERT_EQ(result.recognized_credentials.size(),
              test.expected_user_ids.size() + 1);
    for (size_t i = 0; i < test.expected_user_ids.size(); ++i) {
      std::string expected_id = test.expected_user_ids[i];
      EXPECT_EQ(result.recognized_credentials[i].user.id,
                std::vector<uint8_t>(expected_id.begin(), expected_id.end()));
    }
    EXPECT_EQ(result.recognized_credentials.back().source,
              device::AuthenticatorType::kICloudKeychain);
    testing::Mock::VerifyAndClearExpectations(&observer_);
  }
}

#if BUILDFLAG(IS_MAC)
std::string TouchIdMetadataSecret(ChromeWebAuthenticationDelegate& delegate,
                                  content::BrowserContext* browser_context) {
  return delegate.GetTouchIdAuthenticatorConfig(browser_context)
      ->metadata_secret;
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, TouchIdMetadataSecret) {
  ChromeWebAuthenticationDelegate delegate;
  std::string secret = TouchIdMetadataSecret(delegate, GetBrowserContext());
  EXPECT_EQ(secret.size(), 32u);
  // The secret should be stable.
  EXPECT_EQ(secret, TouchIdMetadataSecret(delegate, GetBrowserContext()));
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TouchIdMetadataSecret_EqualForSameProfile) {
  // Different delegates on the same BrowserContext (Profile) should return
  // the same secret.
  ChromeWebAuthenticationDelegate delegate1;
  ChromeWebAuthenticationDelegate delegate2;
  EXPECT_EQ(TouchIdMetadataSecret(delegate1, GetBrowserContext()),
            TouchIdMetadataSecret(delegate2, GetBrowserContext()));
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TouchIdMetadataSecret_NotEqualForDifferentProfiles) {
  // Different profiles have different secrets.
  auto other_browser_context = CreateBrowserContext();
  ChromeWebAuthenticationDelegate delegate;
  EXPECT_NE(TouchIdMetadataSecret(delegate, GetBrowserContext()),
            TouchIdMetadataSecret(delegate, other_browser_context.get()));
  // Ensure this second secret is actually valid.
  EXPECT_EQ(
      32u, TouchIdMetadataSecret(delegate, other_browser_context.get()).size());
}

#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)

// Tests that ShouldReturnAttestation() returns with true if |authenticator|
// is the Windows native WebAuthn API with WEBAUTHN_API_VERSION_2 or higher,
// where Windows prompts for attestation in its own native UI.
//
// Ideally, this would also test the inverse case, i.e. that with
// WEBAUTHN_API_VERSION_1 Chrome's own attestation prompt is shown. However,
// there seems to be no good way to test AuthenticatorRequestDialogModel UI.
TEST_F(ChromeAuthenticatorRequestDelegateTest, ShouldPromptForAttestationWin) {
  ::device::FakeWinWebAuthnApi win_webauthn_api;
  win_webauthn_api.set_version(WEBAUTHN_API_VERSION_2);
  ::device::WinWebAuthnApiAuthenticator authenticator(
      /*current_window=*/nullptr, &win_webauthn_api);

  ::device::test::ValueCallbackReceiver<bool> cb;
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  delegate.ShouldReturnAttestation(kRelyingPartyID, &authenticator,
                                   /*is_enterprise_attestation=*/false,
                                   cb.callback());
  cb.WaitForCallback();
  EXPECT_EQ(cb.value(), true);
}

#endif  // BUILDFLAG(IS_WIN)

class OriginMayUseRemoteDesktopClientOverrideTest
    : public ChromeAuthenticatorRequestDelegateTest {
 protected:
  static constexpr char kCorpCrdOrigin[] =
      "https://remotedesktop.corp.google.com";
  static constexpr char kCorpCrdAutopushOrigin[] =
      "https://remotedesktop-autopush.corp.google.com/";
  static constexpr char kCorpCrdDailyOrigin[] =
      "https://remotedesktop-daily-6.corp.google.com/";
  static constexpr char kExampleOrigin[] = "https://example.com";

  base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthnGoogleCorpRemoteDesktopClientPrivilege};
};

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       RemoteProxiedRequestsAllowedPolicy) {
  // The "webauthn.remote_proxied_requests_allowed" policy pref should enable
  // Google's internal CRD origin to use the RemoteDesktopClientOverride
  // extension.
  enum class Policy {
    kUnset,
    kDisabled,
    kEnabled,
  };
  ChromeWebAuthenticationDelegate delegate;
  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
  for (auto* origin : {kCorpCrdOrigin, kCorpCrdAutopushOrigin,
                       kCorpCrdDailyOrigin, kExampleOrigin}) {
    for (const auto policy :
         {Policy::kUnset, Policy::kDisabled, Policy::kEnabled}) {
      switch (policy) {
        case Policy::kUnset:
          prefs->ClearPref(webauthn::pref_names::kRemoteProxiedRequestsAllowed);
          break;
        case Policy::kDisabled:
          prefs->SetBoolean(webauthn::pref_names::kRemoteProxiedRequestsAllowed,
                            false);
          break;
        case Policy::kEnabled:
          prefs->SetBoolean(webauthn::pref_names::kRemoteProxiedRequestsAllowed,
                            true);
          break;
      }

      constexpr const char* const crd_origins[] = {
          kCorpCrdOrigin,
          kCorpCrdAutopushOrigin,
          kCorpCrdDailyOrigin,
      };
      EXPECT_EQ(
          delegate.OriginMayUseRemoteDesktopClientOverride(
              browser_context(), url::Origin::Create(GURL(origin))),
          base::Contains(crd_origins, origin) && policy == Policy::kEnabled);
    }
  }
}

TEST_F(OriginMayUseRemoteDesktopClientOverrideTest,
       OriginMayUseRemoteDesktopClientOverrideAdditionalOriginSwitch) {
  // The --webauthn-remote-proxied-requests-allowed-additional-origin switch
  // allows passing an additional origin for testing.
  ChromeWebAuthenticationDelegate delegate;
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      webauthn::switches::kRemoteProxiedRequestsAllowedAdditionalOrigin,
      kExampleOrigin);

  // The flag shouldn't have an effect without the policy enabled.
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kCorpCrdOrigin))));

  // With the policy enabled, both the hard-coded and flag origin should be
  // allowed.
  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
  prefs->SetBoolean(webauthn::pref_names::kRemoteProxiedRequestsAllowed, true);
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kExampleOrigin))));
  EXPECT_TRUE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(), url::Origin::Create(GURL(kCorpCrdOrigin))));

  // Other origins still shouldn't be permitted.
  EXPECT_FALSE(delegate.OriginMayUseRemoteDesktopClientOverride(
      browser_context(),
      url::Origin::Create(GURL("https://other.example.com"))));
}

class DisableWebAuthnWithBrokenCertsTest
    : public ChromeAuthenticatorRequestDelegateTest {};

TEST_F(DisableWebAuthnWithBrokenCertsTest, SecurityLevelNotAcceptable) {
  GURL url("https://doofenshmirtz.evil");
  ChromeWebAuthenticationDelegate delegate;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  net::SSLInfo ssl_info;
  ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  simulator->SetSSLInfo(std::move(ssl_info));
  simulator->Commit();
  EXPECT_FALSE(delegate.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}

TEST_F(DisableWebAuthnWithBrokenCertsTest, ExtensionSupported) {
  GURL url("chrome-extension://extensionid");
  ChromeWebAuthenticationDelegate delegate;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  net::SSLInfo ssl_info;
  ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  simulator->SetSSLInfo(std::move(ssl_info));
  simulator->Commit();
  EXPECT_TRUE(delegate.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}

TEST_F(DisableWebAuthnWithBrokenCertsTest, EnterpriseOverride) {
  PrefService* prefs =
      Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
  prefs->SetBoolean(webauthn::pref_names::kAllowWithBrokenCerts, true);
  GURL url("https://doofenshmirtz.evil");
  ChromeWebAuthenticationDelegate delegate;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  net::SSLInfo ssl_info;
  ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  simulator->SetSSLInfo(std::move(ssl_info));
  simulator->Commit();
  EXPECT_TRUE(delegate.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}

TEST_F(DisableWebAuthnWithBrokenCertsTest, Localhost) {
  GURL url("http://localhost");
  ChromeWebAuthenticationDelegate delegate;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  EXPECT_TRUE(delegate.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}

TEST_F(DisableWebAuthnWithBrokenCertsTest, SecurityLevelAcceptable) {
  GURL url("https://owca.org");
  ChromeWebAuthenticationDelegate delegate;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  net::SSLInfo ssl_info;
  ssl_info.cert_status = 0;  // ok.
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  simulator->SetSSLInfo(std::move(ssl_info));
  simulator->Commit();
  EXPECT_TRUE(delegate.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}

// Regression test for crbug.com/1421174.
TEST_F(DisableWebAuthnWithBrokenCertsTest, IgnoreCertificateErrorsFlag) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kIgnoreCertificateErrors);
  GURL url("https://doofenshmirtz.evil");
  ChromeWebAuthenticationDelegate delegate;
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  net::SSLInfo ssl_info;
  ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  simulator->SetSSLInfo(std::move(ssl_info));
  simulator->Commit();
  EXPECT_TRUE(delegate.IsSecurityLevelAcceptableForWebAuthn(
      main_rfh(), url::Origin::Create(url)));
}

}  // namespace

#if BUILDFLAG(IS_MAC)

// These test functions are outside of the anonymous namespace so that
// `FRIEND_TEST_ALL_PREFIXES` works to let them test private functions.

class ChromeAuthenticatorRequestDelegatePrivateTest : public testing::Test {
  // A `BrowserTaskEnvironment` needs to be in-scope in order to create a
  // `TestingProfile`.
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthnICloudKeychain};
};

TEST_F(ChromeAuthenticatorRequestDelegatePrivateTest, DaysSinceDate) {
  const base::Time now = base::Time::FromTimeT(1691188997);  // 2023-08-04
  const struct {
    char input[16];
    absl::optional<int> expected_result;
  } kTestCases[] = {
      {"", absl::nullopt},          //
      {"2023-08-", absl::nullopt},  //
      {"2023-08-04", 0},            //
      {"2023-08-03", 1},            //
      {"2023-8-3", 1},              //
      {"2023-07-04", 31},           //
      {"2001-11-23", 7924},         //
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.input);
    const absl::optional<int> result =
        ChromeAuthenticatorRequestDelegate::DaysSinceDate(test.input, now);
    EXPECT_EQ(result, test.expected_result);
  }
}

TEST_F(ChromeAuthenticatorRequestDelegatePrivateTest, GetICloudKeychainPref) {
  TestingProfile profile;

  // We use a boolean preference as a tristate, so it's worth checking that
  // an unset preference is recognised as such.
  EXPECT_FALSE(ChromeAuthenticatorRequestDelegate::GetICloudKeychainPref(
                   profile.GetPrefs())
                   .has_value());
  profile.GetPrefs()->SetBoolean(prefs::kCreatePasskeysInICloudKeychain, true);
  EXPECT_EQ(*ChromeAuthenticatorRequestDelegate::GetICloudKeychainPref(
                profile.GetPrefs()),
            true);
}

TEST_F(ChromeAuthenticatorRequestDelegatePrivateTest,
       ShouldCreateInICloudKeychain) {
  // Safety check that SPC requests never default to iCloud Keychain.
  EXPECT_FALSE(ChromeAuthenticatorRequestDelegate::ShouldCreateInICloudKeychain(
      ChromeAuthenticatorRequestDelegate::RequestSource::
          kSecurePaymentConfirmation,
      /*is_active_profile_authenticator_user=*/false,
      /*has_icloud_drive_enabled=*/true, /*request_is_for_google_com=*/true,
      /*preference=*/true));

  // For the valid request type, the preference should be controlling if set.
  for (const bool preference : {false, true}) {
    EXPECT_EQ(preference,
              ChromeAuthenticatorRequestDelegate::ShouldCreateInICloudKeychain(
                  ChromeAuthenticatorRequestDelegate::RequestSource::
                      kWebAuthentication,
                  /*is_active_profile_authenticator_user=*/false,
                  /*has_icloud_drive_enabled=*/true,
                  /*request_is_for_google_com=*/true,
                  /*preference=*/preference));

    // Otherwise the default is controlled by several feature flags. Testing
    // them would just be a change detector.
  }
}

#endif
