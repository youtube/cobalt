// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cryptohi.h>
#include <pk11pub.h>

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/extension_key_permissions_service.h"
#include "chrome/browser/chromeos/platform_keys/extension_key_permissions_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/extensions/api/platform_keys/platform_keys_api.h"
#include "chrome/browser/extensions/api/platform_keys/platform_keys_test_base.h"
#include "chrome/browser/extensions/api/platform_keys/verify_trust_api_base.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {

constexpr char kExtensionId[] = "knldjmfmopnpolahpmmgbagdohdnhkik";

using ContextType = extensions::browser_test_util::ContextType;

using ::testing::Combine;
using ::testing::ConvertGenerator;
using ::testing::Values;
using ::testing::WithParamInterface;

class PlatformKeysTest : public PlatformKeysTestBase {
 public:
  enum class UserClientCertSlot { kPrivateSlot, kPublicSlot };

  PlatformKeysTest(EnrollmentStatus enrollment_status,
                   UserStatus user_status,
                   bool key_permission_policy,
                   UserClientCertSlot user_client_cert_slot,
                   ContextType context_type)
      : PlatformKeysTestBase(SystemTokenStatus::EXISTS,
                             enrollment_status,
                             user_status),
        key_permission_policy_(key_permission_policy),
        user_client_cert_slot_(user_client_cert_slot),
        context_type_(context_type) {
    // Most tests require this to be true. Those that don't can reset
    // it to false if necessary. This is always reset in the destructor.
    extensions::PlatformKeysInternalSelectClientCertificatesFunction::
        SetSkipInteractiveCheckForTest(true);
  }

  ~PlatformKeysTest() override {
    extensions::PlatformKeysInternalSelectClientCertificatesFunction::
        SetSkipInteractiveCheckForTest(false);
  }

  PlatformKeysTest(const PlatformKeysTest&) = delete;
  PlatformKeysTest& operator=(const PlatformKeysTest&) = delete;

  void SetUpOnMainThread() override {
    if (ash::features::IsCopyClientKeysCertsToChapsEnabled() &&
        (user_client_cert_slot_ == UserClientCertSlot::kPublicSlot)) {
      // There's an active effort to deprecate the public slot. Some components
      // (e.g. Kcer) don't take it into account, which breaks tests, but they
      // also don't have to consider it because with the
      // CopyClientKeysCertsToChaps feature enabled all the necessary data is
      // automatically copied from the public slot into the private slot.
      GTEST_SKIP();
    }

    base::AddTagToTestResult("feature_id",
                             "screenplay-63f95a00-bff8-4d81-9cf9-ccf5fdacbef0");
    if (!IsPreTest()) {
      // Set up the private slot before
      // |PlatformKeysTestBase::SetUpOnMainThread| triggers the user sign-in.
      ASSERT_TRUE(user_private_slot_db_.is_open());
      base::RunLoop loop;
      content::GetIOThreadTaskRunner({})->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&PlatformKeysTest::SetPrivateSoftwareSlotOnIO,
                         base::Unretained(this),
                         crypto::ScopedPK11Slot(
                             PK11_ReferenceSlot(user_private_slot_db_.slot()))),
          loop.QuitClosure());
      loop.Run();
    }

    PlatformKeysTestBase::SetUpOnMainThread();

    if (IsPreTest()) {
      return;
    }

    {
      base::RunLoop loop;
      NssServiceFactory::GetForContext(profile())
          ->UnsafelyGetNSSCertDatabaseForTesting(
              base::BindOnce(&PlatformKeysTest::SetupTestCerts,
                             base::Unretained(this), loop.QuitClosure()));
      loop.Run();
    }

    if (user_status() != UserStatus::UNMANAGED && key_permission_policy_) {
      SetupKeyPermissionUserPolicy();
    }
  }

  void SetupKeyPermissionUserPolicy() {
    policy::PolicyMap policy;

    // Set up the test policy that gives |extension_| the permission to access
    // corporate keys.
    base::Value::Dict key_permissions_policy;
    {
      base::Value::Dict cert1_key_permission;
      cert1_key_permission.Set("allowCorporateKeyUsage", true);
      key_permissions_policy.Set(kExtensionId, std::move(cert1_key_permission));
    }

    policy.Set(policy::key::kKeyPermissions, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(key_permissions_policy)), nullptr);

    mock_policy_provider()->UpdateChromePolicy(policy);
  }

  chromeos::ExtensionPlatformKeysService* GetExtensionPlatformKeysService() {
    return chromeos::ExtensionPlatformKeysServiceFactory::GetForBrowserContext(
        profile());
  }

  bool RunPlatformKeysTest(const char* test_suite_name) {
    // By default, the system token is not available.
    const char* system_token_availability = "false";

    // Only if the current user is of the same domain as the device is enrolled
    // to, the system token is available to the extension.
    if (system_token_status() == SystemTokenStatus::EXISTS &&
        enrollment_status() == EnrollmentStatus::ENROLLED &&
        user_status() == UserStatus::MANAGED_AFFILIATED_DOMAIN) {
      system_token_availability = "true";
    }

    // The test gets configuration values from the custom arg.
    const std::string custom_arg = base::StringPrintf(
        R"({ "testSuiteName": "%s", "systemTokenEnabled": %s })",
        test_suite_name, system_token_availability);
    return RunExtensionTest("platform_keys", {.custom_arg = custom_arg.c_str()},
                            {.context_type = context_type_});
  }

  void RegisterClient1AsCorporateKey() {
    const extensions::Extension* const fake_gen_extension =
        LoadExtension(test_data_dir_.AppendASCII("platform_keys_genkey"));

    base::RunLoop run_loop;
    chromeos::platform_keys::ExtensionKeyPermissionsServiceFactory::
        GetForBrowserContextAndExtension(
            base::BindOnce(&PlatformKeysTest::GotPermissionsForExtension,
                           base::Unretained(this), run_loop.QuitClosure()),
            profile(), fake_gen_extension->id());
    run_loop.Run();
  }

 protected:
  // Imported into user's private or public slot, depending on the value of
  // |user_client_cert_slot_|.
  scoped_refptr<net::X509Certificate> client_cert1_;
  // Imported into system slot.
  scoped_refptr<net::X509Certificate> client_cert2_;
  // Signed using an elliptic curve (ECDSA) algorithm.
  // Imported in the same slot as |client_cert1_|.
  scoped_refptr<net::X509Certificate> client_cert3_;

 private:
  base::FilePath extension_path() const {
    return test_data_dir_.AppendASCII("platform_keys");
  }

  void SetPrivateSoftwareSlotOnIO(crypto::ScopedPK11Slot slot) {
    crypto::SetPrivateSoftwareSlotForChromeOSUserForTesting(std::move(slot));
  }

  void OnKeyRegisteredForCorporateUsage(
      std::unique_ptr<chromeos::platform_keys::ExtensionKeyPermissionsService>
          extension_key_permissions_service,
      base::OnceClosure done_callback,
      bool is_error,
      crosapi::mojom::KeystoreError error) {
    ASSERT_FALSE(is_error) << static_cast<int>(error);
    std::move(done_callback).Run();
  }

  void GotPermissionsForExtension(
      base::OnceClosure done_callback,
      std::unique_ptr<chromeos::platform_keys::ExtensionKeyPermissionsService>
          extension_key_permissions_service) {
    auto* extension_key_permissions_service_unowned =
        extension_key_permissions_service.get();
    std::vector<uint8_t> subject_public_key_info =
        chromeos::platform_keys::GetSubjectPublicKeyInfoBlob(client_cert1_);

    // Mimics the behaviour of the ExtensionPlatformKeysService, which sets the
    // one-time signing permission when the key is registered for corporate
    // usage.
    extension_key_permissions_service_unowned
        ->RegisterOneTimeSigningPermissionForKey(subject_public_key_info);
    extension_key_permissions_service_unowned->RegisterKeyForCorporateUsage(
        subject_public_key_info,
        base::BindOnce(&PlatformKeysTest::OnKeyRegisteredForCorporateUsage,
                       base::Unretained(this),
                       std::move(extension_key_permissions_service),
                       std::move(done_callback)));
  }

  void SetupTestCerts(base::OnceClosure done_callback,
                      net::NSSCertDatabase* cert_db) {
    SetupTestClientCerts(cert_db);
    std::move(done_callback).Run();
  }

  void SetupTestClientCerts(net::NSSCertDatabase* cert_db) {
    // Sanity check to ensure that
    // SetPrivateSoftwareSlotForChromeOSUserForTesting took effect.
    EXPECT_EQ(user_private_slot_db_.slot(), cert_db->GetPrivateSlot().get());
    EXPECT_NE(cert_db->GetPrivateSlot().get(), cert_db->GetPublicSlot().get());

    crypto::ScopedPK11Slot slot =
        user_client_cert_slot_ == UserClientCertSlot::kPrivateSlot
            ? cert_db->GetPrivateSlot()
            : cert_db->GetPublicSlot();
    client_cert1_ = net::ImportClientCertAndKeyFromFile(
        extension_path(), "client_1.pem", "client_1.pk8", slot.get());
    ASSERT_TRUE(client_cert1_.get());

    // Import a second client cert signed by another CA than client_1 into the
    // system wide key slot.
    client_cert2_ = net::ImportClientCertAndKeyFromFile(
        extension_path(), "client_2.pem", "client_2.pk8",
        test_system_slot()->slot());
    ASSERT_TRUE(client_cert2_.get());

    client_cert3_ = net::ImportClientCertAndKeyFromFile(
        extension_path(), "client_3.pem", "client_3.pk8", slot.get());
    ASSERT_TRUE(client_cert3_.get());

    // The main important observer for these tests is Kcer.
    net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
  }

  const bool key_permission_policy_;
  const UserClientCertSlot user_client_cert_slot_;
  crypto::ScopedTestNSSDB user_private_slot_db_;
  const ContextType context_type_;
};

class TestSelectDelegate
    : public chromeos::ExtensionPlatformKeysService::SelectDelegate {
 public:
  // On each Select call, selects the next entry in |certs_to_select| from back
  // to front. Once the first entry is reached, that one will be selected
  // repeatedly.
  // Entries of |certs_to_select| can be null in which case no certificate will
  // be selected.
  // If |certs_to_select| is empty, any invocation of |Select| will fail.
  explicit TestSelectDelegate(net::CertificateList certs_to_select)
      : certs_to_select_(certs_to_select) {}

  ~TestSelectDelegate() override = default;

  void Select(const std::string& extension_id,
              const net::CertificateList& certs,
              CertificateSelectedCallback callback,
              content::WebContents* web_contents,
              content::BrowserContext* context) override {
    ASSERT_TRUE(context);
    ASSERT_FALSE(certs_to_select_.empty());
    scoped_refptr<net::X509Certificate> selection;
    if (certs_to_select_.back()) {
      for (scoped_refptr<net::X509Certificate> cert : certs) {
        if (cert->EqualsExcludingChain(certs_to_select_.back().get())) {
          selection = cert;
          break;
        }
      }
    }
    if (certs_to_select_.size() > 1) {
      certs_to_select_.pop_back();
    }
    std::move(callback).Run(selection);
  }

 private:
  net::CertificateList certs_to_select_;
};

struct UnmanagedPlatformKeysTestParams {
  UnmanagedPlatformKeysTestParams(
      PlatformKeysTestBase::EnrollmentStatus enrollment_status,
      PlatformKeysTest::UserClientCertSlot user_client_cert_slot,
      ContextType context_type)
      : enrollment_status(enrollment_status),
        user_client_cert_slot(user_client_cert_slot),
        context_type(context_type) {}

  PlatformKeysTestBase::EnrollmentStatus enrollment_status;
  PlatformKeysTest::UserClientCertSlot user_client_cert_slot;
  ContextType context_type;
};

class UnmanagedPlatformKeysTest
    : public PlatformKeysTest,
      public WithParamInterface<UnmanagedPlatformKeysTestParams> {
 public:
  UnmanagedPlatformKeysTest()
      : PlatformKeysTest(GetParam().enrollment_status,
                         UserStatus::UNMANAGED,
                         false /* unused */,
                         GetParam().user_client_cert_slot,
                         GetParam().context_type) {}
};

struct ManagedPlatformKeysTestParams {
  ManagedPlatformKeysTestParams(
      PlatformKeysTestBase::EnrollmentStatus enrollment_status,
      PlatformKeysTestBase::UserStatus user_status,
      ContextType context_type)
      : enrollment_status_(enrollment_status),
        user_status_(user_status),
        context_type_(context_type) {}

  PlatformKeysTestBase::EnrollmentStatus enrollment_status_;
  PlatformKeysTestBase::UserStatus user_status_;
  ContextType context_type_;
};

class ManagedWithPermissionPlatformKeysTest
    : public PlatformKeysTest,
      public WithParamInterface<ManagedPlatformKeysTestParams> {
 public:
  ManagedWithPermissionPlatformKeysTest()
      : PlatformKeysTest(GetParam().enrollment_status_,
                         GetParam().user_status_,
                         true /* grant the extension key permission */,
                         UserClientCertSlot::kPrivateSlot,
                         GetParam().context_type_) {}
};

class ManagedWithoutPermissionPlatformKeysTest
    : public PlatformKeysTest,
      public WithParamInterface<ManagedPlatformKeysTestParams> {
 public:
  ManagedWithoutPermissionPlatformKeysTest()
      : PlatformKeysTest(GetParam().enrollment_status_,
                         GetParam().user_status_,
                         false /* do not grant key permission */,
                         UserClientCertSlot::kPrivateSlot,
                         GetParam().context_type_) {}
};

std::unique_ptr<net::test_server::HttpResponse> HandleFileRequest(
    const base::FilePath& server_root,
    const net::test_server::HttpRequest& request) {
  if (request.method != net::test_server::METHOD_GET) {
    return nullptr;
  }
  const std::string& relative_path = request.GetURL().path();
  if (!relative_path.starts_with("/")) {
    return nullptr;
  }
  std::string request_path = relative_path.substr(1);
  base::FilePath file_path(server_root.AppendASCII(request_path));
  std::string file_contents;
  if (!base::ReadFileToString(file_path, &file_contents)) {
    return nullptr;
  }

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  http_response->set_content(file_contents);
  return http_response;
}

enum class CertNetFetchStatus { kEnabled, kDisabled };

struct UnmanagedVerifyServerCertPlatformKeysTestParams {
  using TupleT = std::tuple<PlatformKeysTestBase::EnrollmentStatus,
                            PlatformKeysTest::UserClientCertSlot,
                            ContextType,
                            CertNetFetchStatus>;

  explicit UnmanagedVerifyServerCertPlatformKeysTestParams(const TupleT& t)
      : enrollment_status(std::get<0>(t)),
        user_client_cert_slot(std::get<1>(t)),
        context_type(std::get<2>(t)),
        cert_net_fetch_status(std::get<3>(t)) {}

  PlatformKeysTestBase::EnrollmentStatus enrollment_status;
  PlatformKeysTest::UserClientCertSlot user_client_cert_slot;
  ContextType context_type;
  CertNetFetchStatus cert_net_fetch_status;
};

class UnmanagedVerifyServerCertPlatformKeysTest
    : public PlatformKeysTest,
      public WithParamInterface<
          UnmanagedVerifyServerCertPlatformKeysTestParams> {
 public:
  UnmanagedVerifyServerCertPlatformKeysTest()
      : PlatformKeysTest(GetParam().enrollment_status,
                         UserStatus::UNMANAGED,
                         false /* unused */,
                         GetParam().user_client_cert_slot,
                         GetParam().context_type) {
    cert_net_fetch_status_ = GetParam().cert_net_fetch_status;

    switch (GetCertNetFetchStatus()) {
      case CertNetFetchStatus::kEnabled:
        scoped_feature_list_.InitAndEnableFeature(
            extensions::kVerifyTLSServerCertificateUseNetFetcher);
        break;
      case CertNetFetchStatus::kDisabled:
        scoped_feature_list_.InitAndDisableFeature(
            extensions::kVerifyTLSServerCertificateUseNetFetcher);
        break;
    }
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&HandleFileRequest, temp_dir_.GetPath()));

    PlatformKeysTest::SetUpOnMainThread();
    GenerateServerCertificates();
  }

  CertNetFetchStatus GetCertNetFetchStatus() { return cert_net_fetch_status_; }

 private:
  void GenerateServerCertificates() {
    const base::FilePath& output_dir = temp_dir_.GetPath();
    GURL base_url = embedded_test_server()->base_url();

    base::Time not_before = base::Time::Now() - base::Days(100);
    base::Time not_after = base::Time::Now() + base::Days(1000);

    // Self-signed root certificate.
    auto root = std::make_unique<net::CertBuilder>(nullptr, nullptr);
    root->SetValidity(not_before, not_after);

    root->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/-1);
    root->SetKeyUsages(
        {bssl::KEY_USAGE_BIT_KEY_CERT_SIGN, bssl::KEY_USAGE_BIT_CRL_SIGN});

    ASSERT_TRUE(
        base::WriteFile(output_dir.AppendASCII("root.pem"), root->GetPEM()));

    scoped_test_root_.Reset({root->GetX509Certificate()});

    // Intermediate certificates with correct caIssuers.
    auto l1_interm =
        std::make_unique<net::CertBuilder>(/*orig_cert=*/nullptr, root.get());
    l1_interm->SetValidity(not_before, not_after);
    l1_interm->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/-1);
    l1_interm->SetKeyUsages(
        {bssl::KEY_USAGE_BIT_KEY_CERT_SIGN, bssl::KEY_USAGE_BIT_CRL_SIGN});
    ASSERT_TRUE(base::WriteFile(output_dir.AppendASCII("l1_interm.der"),
                                l1_interm->GetDER()));

    auto l2_interm =
        std::make_unique<net::CertBuilder>(/*orig_cert=*/nullptr, root.get());
    l2_interm->SetValidity(not_before, not_after);
    l2_interm->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/-1);
    l2_interm->SetKeyUsages(
        {bssl::KEY_USAGE_BIT_KEY_CERT_SIGN, bssl::KEY_USAGE_BIT_CRL_SIGN});
    ASSERT_TRUE(base::WriteFile(output_dir.AppendASCII("l2_interm.der"),
                                l2_interm->GetDER()));

    // Intermediate certificates with incorrect caIssuers.
    auto l3_interm =
        std::make_unique<net::CertBuilder>(/*orig_cert=*/nullptr, root.get());
    l3_interm->SetValidity(not_before, not_after);
    l3_interm->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/-1);
    l3_interm->SetKeyUsages(
        {bssl::KEY_USAGE_BIT_KEY_CERT_SIGN, bssl::KEY_USAGE_BIT_CRL_SIGN});
    ASSERT_TRUE(base::WriteFile(output_dir.AppendASCII("l3_interm.der"),
                                l3_interm->GetDER()));

    // Target certs.
    auto l1_leaf =
        std::make_unique<net::CertBuilder>(/*orig_cert=*/nullptr, root.get());
    l1_leaf->SetValidity(not_before, not_after);
    l1_leaf->SetBasicConstraints(/*is_ca=*/false, /*path_len=*/-1);
    l1_leaf->SetKeyUsages({bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE});
    l1_leaf->SetSubjectAltName("l1_leaf");
    ASSERT_TRUE(base::WriteFile(output_dir.AppendASCII("l1_leaf.der"),
                                l1_leaf->GetDER()));

    auto l2_leaf = std::make_unique<net::CertBuilder>(/*orig_cert=*/nullptr,
                                                      l1_interm.get());
    l2_leaf->SetValidity(not_before, not_after);
    l2_leaf->SetBasicConstraints(/*is_ca=*/false, /*path_len=*/-1);
    l2_leaf->SetKeyUsages({bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE});
    l2_leaf->SetSubjectAltName("l2_leaf");
    l2_leaf->SetCaIssuersUrl(base_url.Resolve("l1_interm.der"));
    ASSERT_TRUE(base::WriteFile(output_dir.AppendASCII("l2_leaf.der"),
                                l2_leaf->GetDER()));

    auto l3_leaf = std::make_unique<net::CertBuilder>(/*orig_cert=*/nullptr,
                                                      l2_interm.get());
    l3_leaf->SetValidity(not_before, not_after);
    l3_leaf->SetBasicConstraints(/*is_ca=*/false, /*path_len=*/-1);
    l3_leaf->SetKeyUsages({bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE});
    l3_leaf->SetSubjectAltName("l3_leaf");
    l3_leaf->SetCaIssuersUrl(base_url.Resolve("l2_interm.der"));
    ASSERT_TRUE(base::WriteFile(output_dir.AppendASCII("l3_leaf.der"),
                                l3_leaf->GetDER()));

    auto l4_leaf = std::make_unique<net::CertBuilder>(/*orig_cert=*/nullptr,
                                                      l3_interm.get());
    l4_leaf->SetValidity(not_before, not_after);
    l4_leaf->SetBasicConstraints(/*is_ca=*/false, /*path_len=*/-1);
    l4_leaf->SetKeyUsages({bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE});
    l4_leaf->SetSubjectAltName("l4_leaf");
    l4_leaf->SetCaIssuersUrl(base_url.Resolve("non_existing_file.der"));
    ASSERT_TRUE(base::WriteFile(output_dir.AppendASCII("l4_leaf.der"),
                                l4_leaf->GetDER()));
  }

  net::ScopedTestRoot scoped_test_root_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
  CertNetFetchStatus cert_net_fetch_status_ = CertNetFetchStatus::kDisabled;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest, PRE_Basic) {
  RunPreTest();
}

// At first interactively selects |client_cert1_|, |client_cert2_| and
// |client_cert3_| to grant permissions and afterwards runs more basic tests.
// After the initial two interactive calls, the simulated user does not select
// any cert.
IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest, Basic) {
  net::CertificateList certs;
  certs.push_back(nullptr);
  certs.push_back(client_cert3_);
  certs.push_back(client_cert2_);
  certs.push_back(client_cert1_);

  GetExtensionPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(certs));

  ASSERT_TRUE(RunPlatformKeysTest("basicTests")) << message_;
}

IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest,
                       PRE_BackgroundInteractiveTest) {
  RunPreTest();
}

// Tests that interactive calls are not allowed from the extension's
// background page. This test is simple and requires no certs or any
// particular setup.
IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest, BackgroundInteractiveTest) {
  // This needs to be set to false, since we're testing the actual error.
  extensions::PlatformKeysInternalSelectClientCertificatesFunction::
      SetSkipInteractiveCheckForTest(false);
  net::CertificateList certs;
  certs.push_back(nullptr);

  GetExtensionPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(certs));
  ASSERT_TRUE(RunPlatformKeysTest("backgroundInteractiveTest")) << message_;
}

IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest, PRE_Permissions) {
  RunPreTest();
}

// On interactive calls, the simulated user always selects |client_cert1_| if
// matching.
IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest, Permissions) {
  net::CertificateList certs;
  certs.push_back(client_cert1_);

  GetExtensionPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(certs));

  ASSERT_TRUE(RunPlatformKeysTest("permissionTests")) << message_;
}

INSTANTIATE_TEST_SUITE_P(
    PersistentBackground,
    UnmanagedPlatformKeysTest,
    Values(UnmanagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
               PlatformKeysTest::UserClientCertSlot::kPrivateSlot,
               ContextType::kPersistentBackground),
           UnmanagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
               PlatformKeysTest::UserClientCertSlot::kPrivateSlot,
               ContextType::kPersistentBackground),
           UnmanagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
               PlatformKeysTest::UserClientCertSlot::kPublicSlot,
               ContextType::kPersistentBackground),
           UnmanagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
               PlatformKeysTest::UserClientCertSlot::kPublicSlot,
               ContextType::kPersistentBackground)));

INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    UnmanagedPlatformKeysTest,
    Values(UnmanagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
               PlatformKeysTest::UserClientCertSlot::kPrivateSlot,
               ContextType::kServiceWorker),
           UnmanagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
               PlatformKeysTest::UserClientCertSlot::kPrivateSlot,
               ContextType::kServiceWorker),
           UnmanagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
               PlatformKeysTest::UserClientCertSlot::kPublicSlot,
               ContextType::kServiceWorker),
           UnmanagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
               PlatformKeysTest::UserClientCertSlot::kPublicSlot,
               ContextType::kServiceWorker)));

IN_PROC_BROWSER_TEST_P(ManagedWithoutPermissionPlatformKeysTest,
                       PRE_UserPermissionsBlocked) {
  RunPreTest();
}

IN_PROC_BROWSER_TEST_P(ManagedWithoutPermissionPlatformKeysTest,
                       UserPermissionsBlocked) {
  // To verify that the user is not prompted for any certificate selection,
  // set up a delegate that fails on any invocation.
  GetExtensionPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(net::CertificateList()));

  ASSERT_TRUE(RunPlatformKeysTest("managedProfile")) << message_;
}

IN_PROC_BROWSER_TEST_P(ManagedWithoutPermissionPlatformKeysTest,
                       PRE_CorporateKeyAccessBlocked) {
  RunPreTest();
}

// A corporate key must not be useable if there is no policy permitting it.
IN_PROC_BROWSER_TEST_P(ManagedWithoutPermissionPlatformKeysTest,
                       CorporateKeyAccessBlocked) {
  RegisterClient1AsCorporateKey();

  // To verify that the user is not prompted for any certificate selection,
  // set up a delegate that fails on any invocation.
  GetExtensionPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(net::CertificateList()));

  ASSERT_TRUE(RunPlatformKeysTest("corporateKeyWithoutPermissionTests"))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(
    PersistentBackground,
    ManagedWithoutPermissionPlatformKeysTest,
    Values(ManagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
               PlatformKeysTestBase::UserStatus::MANAGED_AFFILIATED_DOMAIN,
               ContextType::kPersistentBackground),
           ManagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
               PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN,
               ContextType::kPersistentBackground),
           ManagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
               PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN,
               ContextType::kPersistentBackground)));

INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    ManagedWithoutPermissionPlatformKeysTest,
    Values(ManagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
               PlatformKeysTestBase::UserStatus::MANAGED_AFFILIATED_DOMAIN,
               ContextType::kServiceWorker),
           ManagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
               PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN,
               ContextType::kServiceWorker),
           ManagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
               PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN,
               ContextType::kServiceWorker)));

IN_PROC_BROWSER_TEST_P(ManagedWithPermissionPlatformKeysTest,
                       PRE_PolicyGrantsAccessToCorporateKey) {
  RunPreTest();
}

IN_PROC_BROWSER_TEST_P(ManagedWithPermissionPlatformKeysTest,
                       PolicyGrantsAccessToCorporateKey) {
  RegisterClient1AsCorporateKey();

  // Set up the test SelectDelegate to select |client_cert1_| if available for
  // selection.
  net::CertificateList certs;
  certs.push_back(client_cert1_);

  GetExtensionPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(certs));

  ASSERT_TRUE(RunPlatformKeysTest("corporateKeyWithPermissionTests"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ManagedWithPermissionPlatformKeysTest,
                       PRE_PolicyDoesGrantAccessToNonCorporateKey) {
  RunPreTest();
}

IN_PROC_BROWSER_TEST_P(ManagedWithPermissionPlatformKeysTest,
                       PolicyDoesGrantAccessToNonCorporateKey) {
  // The policy grants access to corporate keys.
  // As the profile is managed, the user must not be able to grant any
  // certificate permission.
  // If the user is not affiliated, no corporate keys are available. Set up a
  // delegate that fails on any invocation. If the user is affiliated, client_2
  // on the system token will be available for selection, as it is implicitly
  // corporate.
  net::CertificateList certs;
  if (user_status() == UserStatus::MANAGED_AFFILIATED_DOMAIN) {
    certs.push_back(nullptr);
  }

  GetExtensionPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(certs));

  ASSERT_TRUE(RunPlatformKeysTest("policyDoesGrantAccessToNonCorporateKey"))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(
    PersistentBackground,
    ManagedWithPermissionPlatformKeysTest,
    Values(ManagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
               PlatformKeysTestBase::UserStatus::MANAGED_AFFILIATED_DOMAIN,
               ContextType::kPersistentBackground),
           ManagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
               PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN,
               ContextType::kPersistentBackground),
           ManagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
               PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN,
               ContextType::kPersistentBackground)));

INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    ManagedWithPermissionPlatformKeysTest,
    Values(ManagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
               PlatformKeysTestBase::UserStatus::MANAGED_AFFILIATED_DOMAIN,
               ContextType::kServiceWorker),
           ManagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
               PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN,
               ContextType::kServiceWorker),
           ManagedPlatformKeysTestParams(
               PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
               PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN,
               ContextType::kServiceWorker)));

IN_PROC_BROWSER_TEST_P(UnmanagedVerifyServerCertPlatformKeysTest,
                       PRE_VerifyServerCert) {
  RunPreTest();
}

IN_PROC_BROWSER_TEST_P(UnmanagedVerifyServerCertPlatformKeysTest,
                       VerifyServerCert) {
  ASSERT_TRUE(embedded_test_server()->Started());
  ASSERT_TRUE(RunPlatformKeysTest("verifyServerCertBasic")) << message_;
  switch (GetCertNetFetchStatus()) {
    case CertNetFetchStatus::kEnabled:
      ASSERT_TRUE(RunPlatformKeysTest("verifyServerCertAiaFetchEnabled"))
          << message_;
      break;
    case CertNetFetchStatus::kDisabled:
      ASSERT_TRUE(RunPlatformKeysTest("verifyServerCertAiaFetchDisabled"))
          << message_;
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    UnmanagedVerifyServerCertPlatformKeysTest,
    ConvertGenerator<UnmanagedVerifyServerCertPlatformKeysTestParams::TupleT>(
        Combine(Values(PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
                       PlatformKeysTestBase::EnrollmentStatus::ENROLLED),
                Values(PlatformKeysTest::UserClientCertSlot::kPrivateSlot,
                       PlatformKeysTest::UserClientCertSlot::kPublicSlot),
                Values(ContextType::kServiceWorker,
                       ContextType::kPersistentBackground),
                Values(CertNetFetchStatus::kEnabled,
                       CertNetFetchStatus::kDisabled))));
