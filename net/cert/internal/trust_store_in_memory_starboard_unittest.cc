// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "net/cert/internal/trust_store_in_memory_starboard.h"

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "net/cert/internal/test_helpers.h"
#include "net/cert/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/cert_issuer_source_static.h"
#include "third_party/boringssl/src/pki/certificate_policies.h"
#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/parse_values.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "third_party/boringssl/src/pki/path_builder.h"
#include "third_party/boringssl/src/pki/simple_path_builder_delegate.h"
#include "third_party/boringssl/src/pki/trust_store.h"

namespace net {

namespace {

// Uses the key rollover test chains, which contain several distinct root
// certificates that all have the subject "CN=Root":
//
//    oldroot-------->newrootrollover  newroot
//       |                      |        |
//       v                      v        v
// oldintermediate           newintermediate
//       |                          |
//       +------------+-------------+
//                    |
//                    v
//                  target
class TrustStoreInMemoryStarboardTest : public testing::Test {
 public:
  TrustStoreInMemoryStarboardTest()
      : delegate_(
            1024,
            bssl::SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1) {}

  void SetUp() override {
    bssl::ParsedCertificateList chain;
    ReadCertChainFromFile(
        "net/data/verify_certificate_chain_unittest/key-rollover/oldchain.pem",
        &chain);
    ASSERT_EQ(3U, chain.size());
    target_ = chain[0];
    oldintermediate_ = chain[1];
    oldroot_ = chain[2];

    ReadCertChainFromFile(
        "net/data/verify_certificate_chain_unittest/"
        "key-rollover/longrolloverchain.pem",
        &chain);
    ASSERT_EQ(5U, chain.size());
    newintermediate_ = chain[1];
    newroot_ = chain[2];
    newrootrollover_ = chain[3];

    ASSERT_TRUE(target_);
    ASSERT_TRUE(oldintermediate_);
    ASSERT_TRUE(oldroot_);
    ASSERT_TRUE(newintermediate_);
    ASSERT_TRUE(newroot_);
    ASSERT_TRUE(newrootrollover_);

    ASSERT_EQ(oldroot_->normalized_subject(), newroot_->normalized_subject());
    ASSERT_EQ(oldroot_->normalized_subject(),
              newrootrollover_->normalized_subject());

    ASSERT_TRUE(cert_dir_.CreateUniqueTempDir());
  }

 protected:
  // Writes |cert| in PEM format to |file_name| in the certificate directory.
  // Bundled root certificates are named "<8 hex digits>.0".
  void WriteCert(const std::shared_ptr<const bssl::ParsedCertificate>& cert,
                 const std::string& file_name) {
    std::string pem;
    ASSERT_TRUE(X509Certificate::GetPEMEncodedFromDER(
        cert->der_cert().AsStringView(), &pem));
    ASSERT_TRUE(base::WriteFile(cert_dir_.GetPath().Append(file_name), pem));
  }

  std::unique_ptr<TrustStoreInMemoryStarboard> CreateTrustStore() {
    return std::make_unique<TrustStoreInMemoryStarboard>(cert_dir_.GetPath());
  }

  bool BuildsValidPath(TrustStoreInMemoryStarboard* store,
                       const bssl::ParsedCertificateList& intermediates) {
    bssl::CertIssuerSourceStatic issuer_source;
    for (const auto& cert : intermediates) {
      issuer_source.AddCert(cert);
    }
    // Within the validity period of the key rollover test certificates.
    const bssl::der::GeneralizedTime time = {2015, 3, 2, 12, 0, 0};
    bssl::CertPathBuilder path_builder(
        target_, store, &delegate_, time, bssl::KeyPurpose::ANY_EKU,
        bssl::InitialExplicitPolicy::kFalse,
        {bssl::der::Input(bssl::kAnyPolicyOid)},
        bssl::InitialPolicyMappingInhibit::kFalse,
        bssl::InitialAnyPolicyInhibit::kFalse);
    path_builder.AddCertIssuerSource(&issuer_source);
    return path_builder.Run().HasValidPath();
  }

  base::ScopedTempDir cert_dir_;

  std::shared_ptr<const bssl::ParsedCertificate> target_;
  std::shared_ptr<const bssl::ParsedCertificate> oldintermediate_;
  std::shared_ptr<const bssl::ParsedCertificate> newintermediate_;
  std::shared_ptr<const bssl::ParsedCertificate> oldroot_;
  std::shared_ptr<const bssl::ParsedCertificate> newroot_;
  std::shared_ptr<const bssl::ParsedCertificate> newrootrollover_;

  bssl::SimplePathBuilderDelegate delegate_;
};

TEST_F(TrustStoreInMemoryStarboardTest, EmptyDirectory) {
  auto store = CreateTrustStore();

  EXPECT_TRUE(store->GetTrust(newroot_.get()).HasUnspecifiedTrust());
  EXPECT_FALSE(store->Contains(newroot_.get()));
}

TEST_F(TrustStoreInMemoryStarboardTest, LoadsCertificatesFromDirectory) {
  ASSERT_NO_FATAL_FAILURE(WriteCert(newroot_, "00000001.0"));
  ASSERT_NO_FATAL_FAILURE(WriteCert(newintermediate_, "00000002.0"));
  auto store = CreateTrustStore();

  EXPECT_TRUE(store->Contains(newroot_.get()));
  EXPECT_TRUE(store->Contains(newintermediate_.get()));
  EXPECT_TRUE(store->GetTrust(newroot_.get()).IsTrustAnchor());
  EXPECT_TRUE(store->GetTrust(newintermediate_.get()).IsTrustAnchor());
}

TEST_F(TrustStoreInMemoryStarboardTest, IgnoresFilesWithUnexpectedNames) {
  ASSERT_NO_FATAL_FAILURE(WriteCert(newroot_, "newroot.pem"));
  ASSERT_NO_FATAL_FAILURE(WriteCert(oldroot_, "0000001.0"));
  auto store = CreateTrustStore();

  EXPECT_FALSE(store->Contains(newroot_.get()));
  EXPECT_FALSE(store->Contains(oldroot_.get()));
}

TEST_F(TrustStoreInMemoryStarboardTest,
       CertificateNotInDirectoryHasUnspecifiedTrust) {
  ASSERT_NO_FATAL_FAILURE(WriteCert(newroot_, "00000001.0"));
  auto store = CreateTrustStore();

  EXPECT_TRUE(store->GetTrust(target_.get()).HasUnspecifiedTrust());
  EXPECT_TRUE(store->GetTrust(newintermediate_.get()).HasUnspecifiedTrust());
}

TEST_F(TrustStoreInMemoryStarboardTest,
       DistinguishesCertificatesWithSameSubject) {
  ASSERT_NO_FATAL_FAILURE(WriteCert(newroot_, "00000001.0"));
  auto store = CreateTrustStore();

  EXPECT_TRUE(store->GetTrust(newroot_.get()).IsTrustAnchor());
  EXPECT_TRUE(store->GetTrust(oldroot_.get()).HasUnspecifiedTrust());
  EXPECT_TRUE(store->GetTrust(newrootrollover_.get()).HasUnspecifiedTrust());
  EXPECT_FALSE(store->Contains(oldroot_.get()));
  EXPECT_FALSE(store->Contains(newrootrollover_.get()));
}

TEST_F(TrustStoreInMemoryStarboardTest, SyncGetIssuersOf) {
  ASSERT_NO_FATAL_FAILURE(WriteCert(newroot_, "00000001.0"));
  auto store = CreateTrustStore();

  bssl::ParsedCertificateList issuers;
  store->SyncGetIssuersOf(newintermediate_.get(), &issuers);
  ASSERT_EQ(1U, issuers.size());
  EXPECT_EQ(newroot_->der_cert(), issuers[0]->der_cert());

  issuers.clear();
  store->SyncGetIssuersOf(newroot_.get(), &issuers);
  ASSERT_EQ(1U, issuers.size());
  EXPECT_EQ(newroot_->der_cert(), issuers[0]->der_cert());

  issuers.clear();
  store->SyncGetIssuersOf(target_.get(), &issuers);
  EXPECT_TRUE(issuers.empty());
}

TEST_F(TrustStoreInMemoryStarboardTest, BuildsPathsToCertificatesInDirectory) {
  ASSERT_NO_FATAL_FAILURE(WriteCert(newroot_, "00000001.0"));
  auto store = CreateTrustStore();

  EXPECT_TRUE(BuildsValidPath(store.get(), {newintermediate_}));
  EXPECT_FALSE(BuildsValidPath(store.get(), {oldintermediate_, oldroot_}));
}

TEST_F(TrustStoreInMemoryStarboardTest, HasNoUserAddedCerts) {
  ASSERT_NO_FATAL_FAILURE(WriteCert(newroot_, "00000001.0"));
  auto store = CreateTrustStore();

  EXPECT_TRUE(store->GetAllUserAddedCerts().empty());
}

}  // namespace

}  // namespace net
