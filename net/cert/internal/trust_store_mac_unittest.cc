// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_mac.h"

#include <algorithm>
#include <set>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/synchronization/lock.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "crypto/mac_security_services_lock.h"
#include "crypto/sha2.h"
#include "net/base/features.h"
#include "net/cert/internal/trust_store_features.h"
#include "net/cert/pem.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/pki/test_helpers.h"
#include "net/cert/pki/trust_store.h"
#include "net/cert/test_keychain_search_list_mac.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_apple.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::UnorderedElementsAreArray;

namespace net {

namespace {

// The PEM block header used for DER certificates
const char kCertificateHeader[] = "CERTIFICATE";

// Parses a PEM encoded certificate from |file_name| and stores in |result|.
::testing::AssertionResult ReadTestCert(
    const std::string& file_name,
    std::shared_ptr<const ParsedCertificate>* result) {
  std::string der;
  const PemBlockMapping mappings[] = {
      {kCertificateHeader, &der},
  };

  ::testing::AssertionResult r = ReadTestDataFromPemFile(
      "net/data/ssl/certificates/" + file_name, mappings);
  if (!r)
    return r;

  CertErrors errors;
  *result = ParsedCertificate::Create(x509_util::CreateCryptoBuffer(der), {},
                                      &errors);
  if (!*result) {
    return ::testing::AssertionFailure()
           << "ParseCertificate::Create() failed:\n"
           << errors.ToDebugString();
  }
  return ::testing::AssertionSuccess();
}

// Returns the DER encodings of the ParsedCertificates in |list|.
std::vector<std::string> ParsedCertificateListAsDER(
    ParsedCertificateList list) {
  std::vector<std::string> result;
  for (const auto& it : list)
    result.push_back(it->der_cert().AsString());
  return result;
}

std::set<std::string> ParseFindCertificateOutputToDerCerts(std::string output) {
  std::set<std::string> certs;
  for (const std::string& hash_and_pem_partial : base::SplitStringUsingSubstr(
           output, "-----END CERTIFICATE-----", base::TRIM_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    // Re-add the PEM ending mark, since SplitStringUsingSubstr eats it.
    const std::string hash_and_pem =
        hash_and_pem_partial + "\n-----END CERTIFICATE-----\n";

    // Parse the PEM encoded text to DER bytes.
    PEMTokenizer pem_tokenizer(hash_and_pem, {kCertificateHeader});
    if (!pem_tokenizer.GetNext()) {
      ADD_FAILURE() << "!pem_tokenizer.GetNext()";
      continue;
    }
    std::string cert_der(pem_tokenizer.data());
    EXPECT_FALSE(pem_tokenizer.GetNext());
    certs.insert(cert_der);
  }
  return certs;
}

class DebugData : public base::SupportsUserData {
 public:
  ~DebugData() override = default;
};

const char* TrustImplTypeToString(TrustStoreMac::TrustImplType t) {
  switch (t) {
    case TrustStoreMac::TrustImplType::kSimple:
      return "Simple";
    case TrustStoreMac::TrustImplType::kDomainCacheFullCerts:
      return "DomainCacheFullCerts";
    case TrustStoreMac::TrustImplType::kKeychainCacheFullCerts:
      return "KeychainCacheFullCerts";
    case TrustStoreMac::TrustImplType::kUnknown:
      return "Unknown";
  }
}

}  // namespace

class TrustStoreMacImplTest
    : public testing::TestWithParam<
          std::tuple<TrustStoreMac::TrustImplType, bool, bool>> {
 public:
  TrustStoreMacImplTest()
      : scoped_enforce_local_anchor_constraints_(
            ExpectedEnforceLocalAnchorConstraintsEnabled()) {
    if (ExpectedTrustedLeafSupportEnabled()) {
      feature_list_.InitAndEnableFeature(
          features::kTrustStoreTrustedLeafSupport);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kTrustStoreTrustedLeafSupport);
    }
  }

  TrustStoreMac::TrustImplType GetImplParam() const {
    return std::get<0>(GetParam());
  }

  bool ExpectedTrustedLeafSupportEnabled() const {
    return std::get<1>(GetParam());
  }

  bool ExpectedEnforceLocalAnchorConstraintsEnabled() const {
    return std::get<2>(GetParam());
  }

  CertificateTrust ExpectedTrustForAnchor() const {
    CertificateTrust trust;

    if (ExpectedTrustedLeafSupportEnabled()) {
      trust =
          CertificateTrust::ForTrustAnchorOrLeaf().WithEnforceAnchorExpiry();
    } else {
      trust = CertificateTrust::ForTrustAnchor().WithEnforceAnchorExpiry();
    }

    if (ExpectedEnforceLocalAnchorConstraintsEnabled()) {
      trust = trust.WithEnforceAnchorConstraints()
                  .WithRequireAnchorBasicConstraints();
    }

    return trust;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ScopedLocalAnchorConstraintsEnforcementForTesting
      scoped_enforce_local_anchor_constraints_;
};

// Much of the Keychain API was marked deprecated as of the macOS 13 SDK.
// Removal of its use is tracked in https://crbug.com/1348251 but deprecation
// warnings are disabled in the meanwhile.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// Test the trust store using known test certificates in a keychain.  Tests
// that issuer searching returns the expected certificates, and that none of
// the certificates are trusted.
TEST_P(TrustStoreMacImplTest, MultiRootNotTrusted) {
  std::unique_ptr<TestKeychainSearchList> test_keychain_search_list(
      TestKeychainSearchList::Create());
  ASSERT_TRUE(test_keychain_search_list);
  base::FilePath keychain_path(
      GetTestCertsDirectory().AppendASCII("multi-root.keychain"));
  // SecKeychainOpen does not fail if the file doesn't exist, so assert it here
  // for easier debugging.
  ASSERT_TRUE(base::PathExists(keychain_path));
  base::ScopedCFTypeRef<SecKeychainRef> keychain;
  OSStatus status = SecKeychainOpen(keychain_path.MaybeAsASCII().c_str(),
                                    keychain.InitializeInto());
  ASSERT_EQ(errSecSuccess, status);
  ASSERT_TRUE(keychain);
  test_keychain_search_list->AddKeychain(keychain);

#pragma clang diagnostic pop

  const TrustStoreMac::TrustImplType trust_impl = GetImplParam();
  TrustStoreMac trust_store(kSecPolicyAppleSSL, trust_impl);

  std::shared_ptr<const ParsedCertificate> a_by_b, b_by_c, b_by_f, c_by_d,
      c_by_e, f_by_e, d_by_d, e_by_e;
  ASSERT_TRUE(ReadTestCert("multi-root-A-by-B.pem", &a_by_b));
  ASSERT_TRUE(ReadTestCert("multi-root-B-by-C.pem", &b_by_c));
  ASSERT_TRUE(ReadTestCert("multi-root-B-by-F.pem", &b_by_f));
  ASSERT_TRUE(ReadTestCert("multi-root-C-by-D.pem", &c_by_d));
  ASSERT_TRUE(ReadTestCert("multi-root-C-by-E.pem", &c_by_e));
  ASSERT_TRUE(ReadTestCert("multi-root-F-by-E.pem", &f_by_e));
  ASSERT_TRUE(ReadTestCert("multi-root-D-by-D.pem", &d_by_d));
  ASSERT_TRUE(ReadTestCert("multi-root-E-by-E.pem", &e_by_e));

  // Test that the untrusted keychain certs would be found during issuer
  // searching.
  {
    ParsedCertificateList found_issuers;
    trust_store.SyncGetIssuersOf(a_by_b.get(), &found_issuers);
    EXPECT_THAT(ParsedCertificateListAsDER(found_issuers),
                UnorderedElementsAreArray(
                    ParsedCertificateListAsDER({b_by_c, b_by_f})));
  }

  {
    ParsedCertificateList found_issuers;
    trust_store.SyncGetIssuersOf(b_by_c.get(), &found_issuers);
    EXPECT_THAT(ParsedCertificateListAsDER(found_issuers),
                UnorderedElementsAreArray(
                    ParsedCertificateListAsDER({c_by_d, c_by_e})));
  }

  {
    ParsedCertificateList found_issuers;
    trust_store.SyncGetIssuersOf(b_by_f.get(), &found_issuers);
    EXPECT_THAT(
        ParsedCertificateListAsDER(found_issuers),
        UnorderedElementsAreArray(ParsedCertificateListAsDER({f_by_e})));
  }

  {
    ParsedCertificateList found_issuers;
    trust_store.SyncGetIssuersOf(c_by_d.get(), &found_issuers);
    EXPECT_THAT(
        ParsedCertificateListAsDER(found_issuers),
        UnorderedElementsAreArray(ParsedCertificateListAsDER({d_by_d})));
  }

  {
    ParsedCertificateList found_issuers;
    trust_store.SyncGetIssuersOf(f_by_e.get(), &found_issuers);
    EXPECT_THAT(
        ParsedCertificateListAsDER(found_issuers),
        UnorderedElementsAreArray(ParsedCertificateListAsDER({e_by_e})));
  }

  // Verify that none of the added certificates are considered trusted (since
  // the test certs in the keychain aren't trusted, unless someone manually
  // added and trusted the test certs on the machine the test is being run on).
  for (const auto& cert :
       {a_by_b, b_by_c, b_by_f, c_by_d, c_by_e, f_by_e, d_by_d, e_by_e}) {
    DebugData debug_data;
    CertificateTrust trust = trust_store.GetTrust(cert.get(), &debug_data);
    EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
              trust.ToDebugString());
    // The combined_trust_debug_info should be 0 since no trust records
    // should exist for these test certs.
    const TrustStoreMac::ResultDebugData* trust_debug_data =
        TrustStoreMac::ResultDebugData::Get(&debug_data);
    ASSERT_TRUE(trust_debug_data);
    EXPECT_EQ(0, trust_debug_data->combined_trust_debug_info());
    EXPECT_EQ(trust_impl, trust_debug_data->trust_impl());
  }
}

// Test against all the certificates in the default keychains. Confirms that
// the computed trust value matches that of SecTrustEvaluateWithError.
TEST_P(TrustStoreMacImplTest, SystemCerts) {
  // Get the list of all certificates in the user & system keychains.
  // This may include both trusted and untrusted certificates.
  //
  // The output contains zero or more repetitions of:
  // "SHA-1 hash: <hash>\n<PEM encoded cert>\n"
  // Starting with macOS 10.15, it includes both SHA-256 and SHA-1 hashes:
  // "SHA-256 hash: <hash>\nSHA-1 hash: <hash>\n<PEM encoded cert>\n"
  std::string find_certificate_default_search_list_output;
  ASSERT_TRUE(
      base::GetAppOutput({"security", "find-certificate", "-a", "-p", "-Z"},
                         &find_certificate_default_search_list_output));
  // Get the list of all certificates in the system roots keychain.
  // (Same details as above.)
  std::string find_certificate_system_roots_output;
  ASSERT_TRUE(base::GetAppOutput(
      {"security", "find-certificate", "-a", "-p", "-Z",
       "/System/Library/Keychains/SystemRootCertificates.keychain"},
      &find_certificate_system_roots_output));

  std::set<std::string> find_certificate_default_search_list_certs =
      ParseFindCertificateOutputToDerCerts(
          find_certificate_default_search_list_output);
  std::set<std::string> find_certificate_system_roots_certs =
      ParseFindCertificateOutputToDerCerts(
          find_certificate_system_roots_output);

  const TrustStoreMac::TrustImplType trust_impl = GetImplParam();

  base::HistogramTester histogram_tester;
  TrustStoreMac trust_store(kSecPolicyAppleX509Basic, trust_impl);

  base::ScopedCFTypeRef<SecPolicyRef> sec_policy(SecPolicyCreateBasicX509());
  ASSERT_TRUE(sec_policy);
  std::vector<std::string> all_certs;
  std::set_union(find_certificate_default_search_list_certs.begin(),
                 find_certificate_default_search_list_certs.end(),
                 find_certificate_system_roots_certs.begin(),
                 find_certificate_system_roots_certs.end(),
                 std::back_inserter(all_certs));
  for (const std::string& cert_der : all_certs) {
    std::string hash = crypto::SHA256HashString(cert_der);
    std::string hash_text = base::HexEncode(hash.data(), hash.size());
    SCOPED_TRACE(hash_text);

    CertErrors errors;
    // Note: don't actually need to make a ParsedCertificate here, just need
    // the DER bytes. But parsing it here ensures the test can skip any certs
    // that won't be returned due to parsing failures inside TrustStoreMac.
    // The parsing options set here need to match the ones used in
    // trust_store_mac.cc.
    ParseCertificateOptions options;
    // For https://crt.sh/?q=D3EEFBCBBCF49867838626E23BB59CA01E305DB7:
    options.allow_invalid_serial_numbers = true;
    std::shared_ptr<const ParsedCertificate> cert = ParsedCertificate::Create(
        x509_util::CreateCryptoBuffer(cert_der), options, &errors);
    if (!cert) {
      LOG(WARNING) << "ParseCertificate::Create " << hash_text << " failed:\n"
                   << errors.ToDebugString();
      continue;
    }

    base::ScopedCFTypeRef<SecCertificateRef> cert_handle(
        x509_util::CreateSecCertificateFromBytes(cert->der_cert().UnsafeData(),
                                                 cert->der_cert().Length()));
    if (!cert_handle) {
      ADD_FAILURE() << "CreateCertBufferFromBytes " << hash_text;
      continue;
    }

    // Check if this cert is considered a trust anchor by TrustStoreMac.
    DebugData debug_data;
    CertificateTrust cert_trust = trust_store.GetTrust(cert.get(), &debug_data);
    bool is_trusted = cert_trust.IsTrustAnchor() || cert_trust.IsTrustLeaf();
    if (is_trusted) {
      EXPECT_EQ(ExpectedTrustForAnchor().ToDebugString(),
                cert_trust.ToDebugString());
    }

    // Check if this cert is considered a trust anchor by the OS.
    base::ScopedCFTypeRef<SecTrustRef> trust;
    {
      base::AutoLock lock(crypto::GetMacSecurityServicesLock());
      ASSERT_EQ(noErr,
                SecTrustCreateWithCertificates(cert_handle, sec_policy,
                                               trust.InitializeInto()));
      ASSERT_EQ(noErr,
                SecTrustSetOptions(trust, kSecTrustOptionLeafIsCA |
                                              kSecTrustOptionAllowExpired |
                                              kSecTrustOptionAllowExpiredRoot));

      if (find_certificate_default_search_list_certs.count(cert_der) &&
          find_certificate_system_roots_certs.count(cert_der)) {
        // If the same certificate is present in both the System and User/Admin
        // domains, and TrustStoreMac is only using trust settings from
        // User/Admin, then it's not possible for this test to know whether the
        // result from SecTrustEvaluate should match the TrustStoreMac result.
        // Just ignore such certificates.
      } else if (!find_certificate_default_search_list_certs.count(cert_der)) {
        // Cert is only in the system domain. It should be untrusted.
        EXPECT_FALSE(is_trusted);
      } else {
        bool trusted;
        if (__builtin_available(macOS 10.14, *)) {
          trusted = SecTrustEvaluateWithError(trust, nullptr);
        } else {
          SecTrustResultType trust_result;
          ASSERT_EQ(noErr, SecTrustEvaluate(trust, &trust_result));
          trusted = (trust_result == kSecTrustResultProceed) ||
                    (trust_result == kSecTrustResultUnspecified);
        }

        bool expected_trust_anchor =
            trusted && (SecTrustGetCertificateCount(trust) == 1);
        EXPECT_EQ(expected_trust_anchor, is_trusted);
      }
      auto* trust_debug_data = TrustStoreMac::ResultDebugData::Get(&debug_data);
      ASSERT_TRUE(trust_debug_data);
      if (is_trusted &&
          trust_impl != TrustStoreMac::TrustImplType::kKeychainCacheFullCerts) {
        // Since this test queries the real trust store, can't know exactly
        // what bits should be set in the trust debug info, but if it's trusted
        // it should at least have something set.
        EXPECT_NE(0, trust_debug_data->combined_trust_debug_info());
      }
      // The impl that was used should be specified in the debug data.
      EXPECT_EQ(trust_impl, trust_debug_data->trust_impl());
    }

    // Call GetTrust again on the same cert. This should exercise the code
    // that checks the trust value for a cert which has already been cached.
    DebugData debug_data2;
    CertificateTrust cert_trust2 =
        trust_store.GetTrust(cert.get(), &debug_data2);
    EXPECT_EQ(cert_trust.ToDebugString(), cert_trust2.ToDebugString());
    auto* trust_debug_data = TrustStoreMac::ResultDebugData::Get(&debug_data);
    ASSERT_TRUE(trust_debug_data);
    auto* trust_debug_data2 = TrustStoreMac::ResultDebugData::Get(&debug_data2);
    ASSERT_TRUE(trust_debug_data2);
    EXPECT_EQ(trust_debug_data->combined_trust_debug_info(),
              trust_debug_data2->combined_trust_debug_info());
    EXPECT_EQ(trust_debug_data->trust_impl(), trust_debug_data2->trust_impl());
  }

  // Since this is testing the actual platform certs and trust settings, we
  // don't know what values the histograms should be, so just verify that the
  // histogram is recorded (or not) depending on the requested trust impl.

  {
    // Histograms only logged by DomainCacheFullCerts impl:
    const int expected_count =
        (trust_impl == TrustStoreMac::TrustImplType::kDomainCacheFullCerts) ? 1
                                                                            : 0;
    histogram_tester.ExpectTotalCount(
        "Net.CertVerifier.MacTrustDomainCertCount.User", expected_count);
    histogram_tester.ExpectTotalCount(
        "Net.CertVerifier.MacTrustDomainCertCount.Admin", expected_count);
    histogram_tester.ExpectTotalCount(
        "Net.CertVerifier.MacTrustDomainCacheInitTime", expected_count);
    histogram_tester.ExpectTotalCount(
        "Net.CertVerifier.MacKeychainCerts.IntermediateCacheInitTime",
        expected_count);
  }

  {
    // Histograms only logged by KeychainCacheFullCerts impl:
    const int expected_count =
        (trust_impl == TrustStoreMac::TrustImplType::kKeychainCacheFullCerts)
            ? 1
            : 0;
    histogram_tester.ExpectTotalCount(
        "Net.CertVerifier.MacKeychainCerts.TrustCount", expected_count);
  }

  {
    // Histograms logged by both DomainCacheFullCerts and KeychainCacheFullCerts
    // impls:
    const int expected_count =
        (trust_impl == TrustStoreMac::TrustImplType::kDomainCacheFullCerts ||
         trust_impl == TrustStoreMac::TrustImplType::kKeychainCacheFullCerts)
            ? 1
            : 0;
    histogram_tester.ExpectTotalCount(
        "Net.CertVerifier.MacKeychainCerts.IntermediateCount", expected_count);
    histogram_tester.ExpectTotalCount(
        "Net.CertVerifier.MacKeychainCerts.TotalCount", expected_count);
    histogram_tester.ExpectTotalCount(
        "Net.CertVerifier.MacTrustImplCacheInitTime", expected_count);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Impl,
    TrustStoreMacImplTest,
    testing::Combine(
        testing::Values(TrustStoreMac::TrustImplType::kSimple,
                        TrustStoreMac::TrustImplType::kDomainCacheFullCerts,
                        TrustStoreMac::TrustImplType::kKeychainCacheFullCerts),
        testing::Bool(),
        testing::Bool()),
    [](const testing::TestParamInfo<TrustStoreMacImplTest::ParamType>& info) {
      return base::StrCat(
          {TrustImplTypeToString(std::get<0>(info.param)),
           std::get<1>(info.param) ? "TrustedLeafSupported" : "TrustAnchorOnly",
           std::get<2>(info.param) ? "EnforceLocalAnchorConstraints"
                                   : "NoLocalAnchorConstraints"});
    });

}  // namespace net
