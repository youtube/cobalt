// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/common/buildflags.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "net/base/features.h"
#include "net/cert/internal/trust_store_chrome.h"
#include "net/cert/internal/trust_store_features.h"
#include "net/cert/x509_util.h"
#include "net/net_buildflags.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
class CertVerifierServiceChromeRootStoreFeaturePolicyTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<
          std::tuple<bool, absl::optional<bool>>> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    // This test puts a test cert in the Chrome Root Store, which will fail in
    // builds where Certificate Transparency is required, so disable CT
    // during this test.
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        false);
    scoped_feature_list_.InitWithFeatureState(
        net::features::kChromeRootStoreUsed, feature_use_chrome_root_store());

    policy::PolicyTest::SetUpInProcessBrowserTestFixture();

#if BUILDFLAG(CHROME_ROOT_STORE_POLICY_SUPPORTED)
    auto policy_val = policy_use_chrome_root_store();
    if (policy_val.has_value()) {
      SetPolicyValue(*policy_val);
    }
#endif
  }

  void SetPolicyValue(bool value) {
    policy::PolicyMap policies;
#if BUILDFLAG(CHROME_ROOT_STORE_POLICY_SUPPORTED)
    SetPolicy(&policies, policy::key::kChromeRootStoreEnabled,
              base::Value(value));
#endif
    UpdateProviderPolicy(policies);
  }

  void TearDownInProcessBrowserTestFixture() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        absl::nullopt);
  }

  bool feature_use_chrome_root_store() const { return std::get<0>(GetParam()); }

  absl::optional<bool> policy_use_chrome_root_store() const {
    return std::get<1>(GetParam());
  }

  bool expected_use_chrome_root_store() const {
    auto policy_val = policy_use_chrome_root_store();
    if (policy_val.has_value())
      return *policy_val;
    return feature_use_chrome_root_store();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(CertVerifierServiceChromeRootStoreFeaturePolicyTest,
                       Test) {
  // IsUsingChromeRootStore return value should match the expected state.
  EXPECT_EQ(expected_use_chrome_root_store(),
            SystemNetworkContextManager::IsUsingChromeRootStore());

  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  // Use a runtime generated cert, as the pre-generated ok_cert has too long of
  // a validity period to be accepted by a publicly trusted root.
  https_test_server.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_AUTO);
  ASSERT_TRUE(https_test_server.Start());

  // Clear test roots so that cert validation only happens with
  // what's in the relevant root store.
  net::TestRootCerts::GetInstance()->Clear();

  {
    // Create updated Chrome Root Store with just the test server root cert.
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(net::CompiledChromeRootStoreVersion() +
                                       1);

    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    scoped_refptr<net::X509Certificate> root_cert =
        net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
    ASSERT_TRUE(root_cert);
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));

    std::string proto_serialized;
    root_store_proto.SerializeToString(&proto_serialized);
    cert_verifier::mojom::ChromeRootStorePtr root_store_ptr =
        cert_verifier::mojom::ChromeRootStore::New(
            base::as_bytes(base::make_span(proto_serialized)));

    base::RunLoop update_run_loop;
    content::GetCertVerifierServiceFactory()->UpdateChromeRootStore(
        std::move(root_store_ptr), update_run_loop.QuitClosure());
    update_run_loop.Run();
  }

  ASSERT_TRUE(NavigateToUrl(https_test_server.GetURL("/simple.html"), this));

  // The navigation should show an interstitial if CRS was not in use, since
  // the root was only trusted in the test CRS update and won't be trusted by
  // the platform roots that are used when CRS is not used.
  EXPECT_NE(expected_use_chrome_root_store(),
            chrome_browser_interstitials::IsShowingInterstitial(
                chrome_test_utils::GetActiveWebContents(this)));

#if BUILDFLAG(CHROME_ROOT_STORE_POLICY_SUPPORTED)
  // Set the policy to the opposite.
  bool new_expected_use_chrome_root_store = !expected_use_chrome_root_store();
  SetPolicyValue(new_expected_use_chrome_root_store);

  // IsUsingChromeRootStore return value should match the new policy setting.
  EXPECT_EQ(new_expected_use_chrome_root_store,
            SystemNetworkContextManager::IsUsingChromeRootStore());

  // The SetUseChromeRootStore message should have been dispatched to the
  // CertVerifierServiceFactory but may not have actually been processed yet.
  // Doing a round-trip to the CertVerifierServiceFactory and back should
  // ensure that the previous message in the queue has already been processed.
  content::GetCertVerifierServiceFactoryRemoteForTesting().FlushForTesting();

  ASSERT_TRUE(NavigateToUrl(https_test_server.GetURL("/title2.html"), this));

  // Navigating to the test server again should respect the new policy setting.
  EXPECT_NE(new_expected_use_chrome_root_store,
            chrome_browser_interstitials::IsShowingInterstitial(
                chrome_test_utils::GetActiveWebContents(this)));
#endif
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CertVerifierServiceChromeRootStoreFeaturePolicyTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(absl::nullopt
#if BUILDFLAG(CHROME_ROOT_STORE_POLICY_SUPPORTED)
                                         ,
                                         false,
                                         true
#endif
                                         )),
    [](const testing::TestParamInfo<
        CertVerifierServiceChromeRootStoreFeaturePolicyTest::ParamType>& info) {
      return base::StrCat(
          {std::get<0>(info.param) ? "FeatureTrue" : "FeatureFalse",
           std::get<1>(info.param).has_value()
               ? (*std::get<1>(info.param) ? "PolicyTrue" : "PolicyFalse")
               : "PolicyNotSet"});
    });
#endif  // BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)

class CertVerifierServiceEnforceLocalAnchorConstraintsFeaturePolicyTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<
          std::tuple<bool, absl::optional<bool>>> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeatureState(
        net::features::kEnforceLocalAnchorConstraints,
        feature_enforce_local_anchor_constraints());

    policy::PolicyTest::SetUpInProcessBrowserTestFixture();

    auto policy_val = policy_enforce_local_anchor_constraints();
    if (policy_val.has_value()) {
      SetPolicyValue(*policy_val);
    }
  }

  void SetPolicyValue(absl::optional<bool> value) {
    policy::PolicyMap policies;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
    SetPolicy(&policies, policy::key::kEnforceLocalAnchorConstraintsEnabled,
              absl::optional<base::Value>(value));
#endif
    UpdateProviderPolicy(policies);
  }

  void ExpectEnforceLocalAnchorConstraintsCorrect(
      bool enforce_local_anchor_constraints) {
    EXPECT_EQ(enforce_local_anchor_constraints,
              net::IsLocalAnchorConstraintsEnforcementEnabled());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
    // Set policy to the opposite value, and then test the value returned by
    // IsLocalAnchorConstraintsEnforcementEnabled has changed.
    SetPolicyValue(!enforce_local_anchor_constraints);
    EXPECT_EQ(!enforce_local_anchor_constraints,
              net::IsLocalAnchorConstraintsEnforcementEnabled());

    // Unset the policy, the value used should go back to the one set by the
    // feature flag.
    SetPolicyValue(absl::nullopt);
    EXPECT_EQ(feature_enforce_local_anchor_constraints(),
              net::IsLocalAnchorConstraintsEnforcementEnabled());
#endif
  }

  bool feature_enforce_local_anchor_constraints() const {
    return std::get<0>(GetParam());
  }

  absl::optional<bool> policy_enforce_local_anchor_constraints() const {
    return std::get<1>(GetParam());
  }

  bool expected_enforce_local_anchor_constraints() const {
    auto policy_val = policy_enforce_local_anchor_constraints();
    if (policy_val.has_value()) {
      return *policy_val;
    }
    return feature_enforce_local_anchor_constraints();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    CertVerifierServiceEnforceLocalAnchorConstraintsFeaturePolicyTest,
    Test) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(https://crbug.com/1410924): Avoid flake on android browser tests by
  // requiring the test to always take at least 1 second to finish. Remove this
  // delay once issue 1410924 is resolved.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(1));
#endif
  ExpectEnforceLocalAnchorConstraintsCorrect(
      expected_enforce_local_anchor_constraints());
#if BUILDFLAG(IS_ANDROID)
  run_loop.Run();
#endif
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CertVerifierServiceEnforceLocalAnchorConstraintsFeaturePolicyTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(absl::nullopt
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
                                         ,
                                         false,
                                         true
#endif
                                         )),
    [](const testing::TestParamInfo<
        CertVerifierServiceEnforceLocalAnchorConstraintsFeaturePolicyTest::
            ParamType>& info) {
      return base::StrCat(
          {std::get<0>(info.param) ? "FeatureTrue" : "FeatureFalse",
           std::get<1>(info.param).has_value()
               ? (*std::get<1>(info.param) ? "PolicyTrue" : "PolicyFalse")
               : "PolicyNotSet"});
    });
