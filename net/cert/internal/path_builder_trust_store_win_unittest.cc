// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/pki/path_builder.h"

#include <algorithm>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "net/cert/pem.h"
#include "net/cert/pki/cert_error_params.h"
#include "net/cert/pki/cert_issuer_source_static.h"
#include "net/cert/pki/common_cert_errors.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/pki/simple_path_builder_delegate.h"
#include "net/cert/pki/test_helpers.h"
#include "net/cert/pki/trust_store_collection.h"
#include "net/cert/pki/trust_store_in_memory.h"
#include "net/cert/pki/verify_certificate_chain.h"
#include "net/der/input.h"
#include "net/net_buildflags.h"
#include "net/test/test_certificate_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

#include "base/win/wincrypt_shim.h"
#include "crypto/scoped_capi_types.h"
#include "net/cert/internal/trust_store_win.h"

namespace net {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

class DeadlineTestingPathBuilderDelegate : public SimplePathBuilderDelegate {
 public:
  DeadlineTestingPathBuilderDelegate(size_t min_rsa_modulus_length_bits,
                                     DigestPolicy digest_policy)
      : SimplePathBuilderDelegate(min_rsa_modulus_length_bits, digest_policy) {}

  bool IsDeadlineExpired() override { return deadline_is_expired_; }

  void SetDeadlineExpiredForTesting(bool deadline_is_expired) {
    deadline_is_expired_ = deadline_is_expired;
  }

 private:
  bool deadline_is_expired_ = false;
};

// AsyncCertIssuerSourceStatic always returns its certs asynchronously.
class AsyncCertIssuerSourceStatic : public CertIssuerSource {
 public:
  class StaticAsyncRequest : public Request {
   public:
    explicit StaticAsyncRequest(ParsedCertificateList&& issuers) {
      issuers_.swap(issuers);
      issuers_iter_ = issuers_.begin();
    }

    StaticAsyncRequest(const StaticAsyncRequest&) = delete;
    StaticAsyncRequest& operator=(const StaticAsyncRequest&) = delete;

    ~StaticAsyncRequest() override = default;

    void GetNext(ParsedCertificateList* out_certs) override {
      if (issuers_iter_ != issuers_.end())
        out_certs->push_back(std::move(*issuers_iter_++));
    }

    ParsedCertificateList issuers_;
    ParsedCertificateList::iterator issuers_iter_;
  };

  ~AsyncCertIssuerSourceStatic() override = default;

  void SetAsyncGetCallback(base::RepeatingClosure closure) {
    async_get_callback_ = std::move(closure);
  }

  void AddCert(std::shared_ptr<const ParsedCertificate> cert) {
    static_cert_issuer_source_.AddCert(std::move(cert));
  }

  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override {}
  void AsyncGetIssuersOf(const ParsedCertificate* cert,
                         std::unique_ptr<Request>* out_req) override {
    num_async_gets_++;
    ParsedCertificateList issuers;
    static_cert_issuer_source_.SyncGetIssuersOf(cert, &issuers);
    auto req = std::make_unique<StaticAsyncRequest>(std::move(issuers));
    *out_req = std::move(req);
    if (!async_get_callback_.is_null())
      async_get_callback_.Run();
  }
  int num_async_gets() const { return num_async_gets_; }

 private:
  CertIssuerSourceStatic static_cert_issuer_source_;

  int num_async_gets_ = 0;
  base::RepeatingClosure async_get_callback_;
};

::testing::AssertionResult ReadTestPem(const std::string& file_name,
                                       const std::string& block_name,
                                       std::string* result) {
  const PemBlockMapping mappings[] = {
      {block_name.c_str(), result},
  };

  return ReadTestDataFromPemFile(file_name, mappings);
}

::testing::AssertionResult ReadTestCert(
    const std::string& file_name,
    std::shared_ptr<const ParsedCertificate>* result) {
  std::string der;
  ::testing::AssertionResult r = ReadTestPem(
      "net/data/ssl/certificates/" + file_name, "CERTIFICATE", &der);
  if (!r)
    return r;
  CertErrors errors;
  *result = ParsedCertificate::Create(
      bssl::UniquePtr<CRYPTO_BUFFER>(CRYPTO_BUFFER_new(
          reinterpret_cast<const uint8_t*>(der.data()), der.size(), nullptr)),
      {}, &errors);
  if (!*result) {
    return ::testing::AssertionFailure()
           << "ParseCertificate::Create() failed:\n"
           << errors.ToDebugString();
  }
  return ::testing::AssertionSuccess();
}

const void* kKey = &kKey;
class TrustStoreThatStoresUserData : public TrustStore {
 public:
  class Data : public base::SupportsUserData::Data {
   public:
    explicit Data(int value) : value(value) {}

    int value = 0;
  };

  // TrustStore implementation:
  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override {}
  CertificateTrust GetTrust(const ParsedCertificate* cert,
                            base::SupportsUserData* debug_data) override {
    debug_data->SetUserData(kKey, std::make_unique<Data>(1234));
    return CertificateTrust::ForUnspecified();
  }
};

TEST(PathBuilderResultUserDataTest, ModifyUserDataInConstructor) {
  std::shared_ptr<const ParsedCertificate> a_by_b;
  ASSERT_TRUE(ReadTestCert("multi-root-A-by-B.pem", &a_by_b));
  SimplePathBuilderDelegate delegate(
      1024, SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1);
  der::GeneralizedTime verify_time = {2017, 3, 1, 0, 0, 0};
  TrustStoreThatStoresUserData trust_store;

  // |trust_store| will unconditionally store user data in the
  // CertPathBuilder::Result. This ensures that the Result object has been
  // initialized before the first GetTrust call occurs (otherwise the test will
  // crash or fail on ASAN bots).
  CertPathBuilder path_builder(
      a_by_b, &trust_store, &delegate, verify_time, KeyPurpose::ANY_EKU,
      InitialExplicitPolicy::kFalse, {der::Input(kAnyPolicyOid)},
      InitialPolicyMappingInhibit::kFalse, InitialAnyPolicyInhibit::kFalse);
  CertPathBuilder::Result result = path_builder.Run();
  auto* data = static_cast<TrustStoreThatStoresUserData::Data*>(
      result.GetUserData(kKey));
  ASSERT_TRUE(data);
  EXPECT_EQ(1234, data->value);
}

class PathBuilderMultiRootWindowsTest : public ::testing::Test {
 public:
  PathBuilderMultiRootWindowsTest()
      : delegate_(
            1024,
            DeadlineTestingPathBuilderDelegate::DigestPolicy::kWeakAllowSha1) {}

  void SetUp() override {
    ASSERT_TRUE(ReadTestCert("multi-root-A-by-B.pem", &a_by_b_));
    ASSERT_TRUE(ReadTestCert("multi-root-B-by-C.pem", &b_by_c_));
    ASSERT_TRUE(ReadTestCert("multi-root-B-by-F.pem", &b_by_f_));
    ASSERT_TRUE(ReadTestCert("multi-root-C-by-D.pem", &c_by_d_));
    ASSERT_TRUE(ReadTestCert("multi-root-C-by-E.pem", &c_by_e_));
    ASSERT_TRUE(ReadTestCert("multi-root-D-by-D.pem", &d_by_d_));
    ASSERT_TRUE(ReadTestCert("multi-root-E-by-E.pem", &e_by_e_));
    ASSERT_TRUE(ReadTestCert("multi-root-F-by-E.pem", &f_by_e_));
  }

 protected:
  std::shared_ptr<const ParsedCertificate> a_by_b_, b_by_c_, b_by_f_, c_by_d_,
      c_by_e_, d_by_d_, e_by_e_, f_by_e_;

  DeadlineTestingPathBuilderDelegate delegate_;
  der::GeneralizedTime time_ = {2017, 3, 1, 0, 0, 0};

  const InitialExplicitPolicy initial_explicit_policy_ =
      InitialExplicitPolicy::kFalse;
  const std::set<der::Input> user_initial_policy_set_ = {
      der::Input(kAnyPolicyOid)};
  const InitialPolicyMappingInhibit initial_policy_mapping_inhibit_ =
      InitialPolicyMappingInhibit::kFalse;
  const InitialAnyPolicyInhibit initial_any_policy_inhibit_ =
      InitialAnyPolicyInhibit::kFalse;
};

void AddToStoreWithEKURestriction(
    HCERTSTORE store,
    const std::shared_ptr<const ParsedCertificate>& cert,
    LPCSTR usage_identifier) {
  crypto::ScopedPCCERT_CONTEXT os_cert(CertCreateCertificateContext(
      X509_ASN_ENCODING, cert->der_cert().UnsafeData(),
      cert->der_cert().Length()));

  CERT_ENHKEY_USAGE usage;
  memset(&usage, 0, sizeof(usage));
  CertSetEnhancedKeyUsage(os_cert.get(), &usage);
  if (usage_identifier) {
    CertAddEnhancedKeyUsageIdentifier(os_cert.get(), usage_identifier);
  }
  CertAddCertificateContextToStore(store, os_cert.get(), CERT_STORE_ADD_ALWAYS,
                                   nullptr);
}

bool AreCertsEq(const std::shared_ptr<const ParsedCertificate> cert_1,
                const std::shared_ptr<const ParsedCertificate> cert_2) {
  return cert_1 && cert_2 && cert_1->der_cert() == cert_2->der_cert();
}

// Test to ensure that path building stops when an intermediate cert is
// encountered that is not usable for TLS because it is explicitly distrusted.
TEST_F(PathBuilderMultiRootWindowsTest, TrustStoreWinOnlyFindTrustedTLSPath) {
  TrustStoreWin::CertStores stores =
      TrustStoreWin::CertStores::CreateInMemoryStoresForTesting();

  AddToStoreWithEKURestriction(stores.roots.get(), d_by_d_,
                               szOID_PKIX_KP_SERVER_AUTH);
  AddToStoreWithEKURestriction(stores.roots.get(), e_by_e_,
                               szOID_PKIX_KP_SERVER_AUTH);
  AddToStoreWithEKURestriction(stores.intermediates.get(), c_by_e_,
                               szOID_PKIX_KP_SERVER_AUTH);
  AddToStoreWithEKURestriction(stores.disallowed.get(), c_by_d_, nullptr);

  std::unique_ptr<TrustStoreWin> trust_store =
      TrustStoreWin::CreateForTesting(std::move(stores));

  CertPathBuilder path_builder(
      b_by_c_, trust_store.get(), &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);

  // Check all paths.
  path_builder.SetExploreAllPaths(true);

  auto result = path_builder.Run();
  ASSERT_TRUE(result.HasValidPath());
  ASSERT_EQ(1U, result.paths.size());
  const auto& path = *result.GetBestValidPath();
  ASSERT_EQ(3U, path.certs.size());
  EXPECT_TRUE(AreCertsEq(b_by_c_, path.certs[0]));
  EXPECT_TRUE(AreCertsEq(c_by_e_, path.certs[1]));
  EXPECT_TRUE(AreCertsEq(e_by_e_, path.certs[2]));

  // Should only be one valid path, the one above.
  const int valid_paths = std::count_if(
      result.paths.begin(), result.paths.end(),
      [](const auto& candidate_path) { return candidate_path->IsValid(); });
  ASSERT_EQ(1, valid_paths);
}

// Test that if an intermediate is untrusted, and it is the only
// path, then path building should fail, even if the root is enabled for
// TLS.
TEST_F(PathBuilderMultiRootWindowsTest, TrustStoreWinNoPathEKURestrictions) {
  TrustStoreWin::CertStores stores =
      TrustStoreWin::CertStores::CreateInMemoryStoresForTesting();

  AddToStoreWithEKURestriction(stores.roots.get(), d_by_d_,
                               szOID_PKIX_KP_SERVER_AUTH);
  AddToStoreWithEKURestriction(stores.disallowed.get(), c_by_d_, nullptr);
  std::unique_ptr<TrustStoreWin> trust_store =
      TrustStoreWin::CreateForTesting(std::move(stores));

  CertPathBuilder path_builder(
      b_by_c_, trust_store.get(), &delegate_, time_, KeyPurpose::ANY_EKU,
      initial_explicit_policy_, user_initial_policy_set_,
      initial_policy_mapping_inhibit_, initial_any_policy_inhibit_);

  auto result = path_builder.Run();
  ASSERT_FALSE(result.HasValidPath());
}

}  // namespace

}  // namespace net
