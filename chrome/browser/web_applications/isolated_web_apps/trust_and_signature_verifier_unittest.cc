// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/trust_and_signature_verifier.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/identity/iwa_identity_validator.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {
namespace {

using ::base::test::ErrorIs;
using ::base::test::HasValue;
using ::testing::HasSubstr;

IsolatedWebAppUrlInfo CreateRandomIsolatedWebAppUrlInfo() {
  web_package::SignedWebBundleId signed_web_bundle_id =
      web_package::SignedWebBundleId::CreateRandomForProxyMode();
  return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      signed_web_bundle_id);
}

IwaSourceWithMode CreateDevProxySource(
    std::string_view dev_mode_proxy_url = "http://default-proxy-url.org/") {
  return IwaSourceProxy{url::Origin::Create(GURL(dev_mode_proxy_url))};
}

web_package::SignedWebBundleId CreateValidNonProxyId() {
  std::array<uint8_t, 32> public_key_bytes = {0};
  auto public_key = web_package::Ed25519PublicKey::Create(
      base::span<const uint8_t, 32>(public_key_bytes));
  return web_package::SignedWebBundleId::CreateForPublicKey(public_key);
}

class TrustAndSignatureVerifierTest : public ::testing::Test {
 public:
  void SetUp() override {
    IwaIdentityValidator::CreateSingleton();
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
  }

  TestingProfile* profile() const { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestingProfile> profile_ = []() {
    TestingProfile::Builder builder;
    return builder.Build();
  }();
};

TEST_F(TrustAndSignatureVerifierTest, DevProxySucceeds) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();

  base::test::TestFuture<base::expected<void, std::string>> future;
  CheckTrustAndSignatures(
      url_info.web_bundle_id(), CreateDevProxySource(),
      IwaInstallOperation{.source = webapps::WebappInstallSource::IWA_DEV_UI},
      &*profile(), future.GetCallback());
  EXPECT_THAT(future.Get(), HasValue());
}

TEST_F(TrustAndSignatureVerifierTest, DevProxyFailsWhenDevModeIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatedWebAppDevMode);

  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();

  base::test::TestFuture<base::expected<void, std::string>> future;
  CheckTrustAndSignatures(
      url_info.web_bundle_id(), CreateDevProxySource(),
      IwaInstallOperation{.source = webapps::WebappInstallSource::IWA_DEV_UI},
      &*profile(), future.GetCallback());
  EXPECT_THAT(
      future.Take(),
      ErrorIs(HasSubstr("Isolated Web App Developer Mode is not enabled")));
}

TEST_F(TrustAndSignatureVerifierTest, NonDevBundleFailsWhenNotTrusted) {
  web_package::SignedWebBundleId web_bundle_id = CreateValidNonProxyId();

  base::FilePath path(FILE_PATH_LITERAL("/dummy/path/to/bundle.swbn"));
  IwaSourceWithMode source = IwaSourceBundleWithMode(path, /*dev_mode=*/false);

  base::test::TestFuture<base::expected<void, std::string>> future;
  CheckTrustAndSignatures(
      web_bundle_id, source,
      IwaInstallOperation{
          .source = webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER},
      &*profile(), future.GetCallback());
  EXPECT_THAT(future.Take(),
              ErrorIs(HasSubstr("must be on the user install allowlist")));
}

TEST_F(TrustAndSignatureVerifierTest,
       NonDevBundleMetadataReadingProceedsToSignatureCheck) {
  web_package::SignedWebBundleId web_bundle_id = CreateValidNonProxyId();

  base::FilePath path(FILE_PATH_LITERAL("/dummy/path/to/bundle.swbn"));
  IwaSourceWithMode source = IwaSourceBundleWithMode(path, /*dev_mode=*/false);

  base::test::TestFuture<base::expected<void, std::string>> future;
  CheckTrustAndSignatures(web_bundle_id, source, IwaMetadataReadingOperation{},
                          &*profile(), future.GetCallback());
  // It should NOT fail with trust error, but proceed to signature check and
  // fail there because the file is missing.
  EXPECT_THAT(future.Take(),
              ErrorIs(Not(HasSubstr("must be on the user install allowlist"))));
}

using TrustAndSignatureVerifierDeathTest = TrustAndSignatureVerifierTest;

TEST_F(TrustAndSignatureVerifierDeathTest,
       MismatchedProxyIdAndBundleSourceCrashes) {
  web_package::SignedWebBundleId web_bundle_id =
      web_package::SignedWebBundleId::CreateRandomForProxyMode();

  base::FilePath path(FILE_PATH_LITERAL("/dummy/path/to/bundle.swbn"));
  IwaSourceWithMode source = IwaSourceBundleWithMode(path, /*dev_mode=*/true);

  EXPECT_DEATH(CheckTrustAndSignatures(
                   web_bundle_id, source,
                   IwaInstallOperation{
                       .source = webapps::WebappInstallSource::IWA_DEV_UI},
                   &*profile(),
                   base::BindOnce([](base::expected<void, std::string>) {})),
               "");
}

TEST_F(TrustAndSignatureVerifierDeathTest,
       MismatchedNonProxyIdAndProxySourceCrashes) {
  web_package::SignedWebBundleId web_bundle_id = CreateValidNonProxyId();

  IwaSourceWithMode source = CreateDevProxySource();

  EXPECT_DEATH(CheckTrustAndSignatures(
                   web_bundle_id, source,
                   IwaInstallOperation{
                       .source = webapps::WebappInstallSource::IWA_DEV_UI},
                   &*profile(),
                   base::BindOnce([](base::expected<void, std::string>) {})),
               "");
}

}  // namespace
}  // namespace web_app
