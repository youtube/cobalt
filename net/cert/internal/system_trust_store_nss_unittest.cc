// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "net/cert/internal/system_trust_store.h"

#include <cert.h>
#include <certdb.h>

#include <memory>

#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/internal/system_trust_store_nss.h"
#include "net/cert/internal/trust_store_features.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace net {

namespace {

// Parses |x509_cert| as a ParsedCertificate and stores the output in
// *|out_parsed_cert|. Wrap in ASSERT_NO_FATAL_FAILURE on callsites.
::testing::AssertionResult ParseX509Certificate(
    const scoped_refptr<X509Certificate>& x509_cert,
    std::shared_ptr<const ParsedCertificate>* out_parsed_cert) {
  CertErrors parsing_errors;
  *out_parsed_cert = ParsedCertificate::Create(
      bssl::UpRef(x509_cert->cert_buffer()),
      x509_util::DefaultParseCertificateOptions(), &parsing_errors);
  if (!*out_parsed_cert) {
    return ::testing::AssertionFailure()
           << "ParseCertificate::Create() failed:\n"
           << parsing_errors.ToDebugString();
  }
  return ::testing::AssertionSuccess();
}

class SystemTrustStoreNSSTest : public ::testing::Test {
 public:
  SystemTrustStoreNSSTest() : test_root_certs_(TestRootCerts::GetInstance()) {}

  SystemTrustStoreNSSTest(const SystemTrustStoreNSSTest&) = delete;
  SystemTrustStoreNSSTest& operator=(const SystemTrustStoreNSSTest&) = delete;

  ~SystemTrustStoreNSSTest() override = default;

  void SetUp() override {
    ::testing::Test::SetUp();

    root_cert_ =
        ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
    ASSERT_TRUE(root_cert_);
    ASSERT_NO_FATAL_FAILURE(
        ParseX509Certificate(root_cert_, &parsed_root_cert_));
    nss_root_cert_ =
        x509_util::CreateCERTCertificateFromX509Certificate(root_cert_.get());
    ASSERT_TRUE(nss_root_cert_);

    ASSERT_TRUE(test_nssdb_.is_open());
    ASSERT_TRUE(other_test_nssdb_.is_open());
  }

 protected:
  // Imports |nss_root_cert_| into |slot| and sets trust flags so that it is a
  // trusted CA for SSL.
  void ImportRootCertAsTrusted(PK11SlotInfo* slot) {
    SECStatus srv = PK11_ImportCert(slot, nss_root_cert_.get(),
                                    CK_INVALID_HANDLE, "nickname_root_cert",
                                    PR_FALSE /* includeTrust (unused) */);
    ASSERT_EQ(SECSuccess, srv);

    CERTCertTrust trust = {0};
    trust.sslFlags = CERTDB_TRUSTED_CA | CERTDB_VALID_CA;
    srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), nss_root_cert_.get(),
                               &trust);
    ASSERT_EQ(SECSuccess, srv);
  }

  crypto::ScopedTestNSSDB test_nssdb_;
  crypto::ScopedTestNSSDB other_test_nssdb_;

  raw_ptr<TestRootCerts> test_root_certs_;

  scoped_refptr<X509Certificate> root_cert_;
  std::shared_ptr<const ParsedCertificate> parsed_root_cert_;
  ScopedCERTCertificate nss_root_cert_;
};

// Tests that SystemTrustStore created for NSS with a user-slot restriction
// allows certificates stored on the specified user slot to be trusted.
TEST_F(SystemTrustStoreNSSTest, UserSlotRestrictionAllows) {
  ScopedLocalAnchorConstraintsEnforcementForTesting
      scoped_enforce_local_anchor_constraints(true);
  std::unique_ptr<SystemTrustStore> system_trust_store =
      CreateSslSystemTrustStoreNSSWithUserSlotRestriction(
          crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())));

  ASSERT_NO_FATAL_FAILURE(ImportRootCertAsTrusted(test_nssdb_.slot()));

  CertificateTrust trust =
      system_trust_store->GetTrustStore()->GetTrust(parsed_root_cert_.get(),
                                                    /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrust::ForTrustAnchor()
                .WithEnforceAnchorConstraints()
                .WithEnforceAnchorExpiry()
                .ToDebugString(),
            trust.ToDebugString());
}

TEST_F(SystemTrustStoreNSSTest,
       UserSlotRestrictionAllowsWithAnchorConstraintsDisabled) {
  ScopedLocalAnchorConstraintsEnforcementForTesting
      scoped_enforce_local_anchor_constraints(false);
  std::unique_ptr<SystemTrustStore> system_trust_store =
      CreateSslSystemTrustStoreNSSWithUserSlotRestriction(
          crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())));

  ASSERT_NO_FATAL_FAILURE(ImportRootCertAsTrusted(test_nssdb_.slot()));

  CertificateTrust trust =
      system_trust_store->GetTrustStore()->GetTrust(parsed_root_cert_.get(),
                                                    /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrust::ForTrustAnchor().ToDebugString(),
            trust.ToDebugString());
}

// Tests that SystemTrustStore created for NSS with a user-slot restriction
// does not allows certificates stored only on user slots different from the one
// specified to be trusted.
TEST_F(SystemTrustStoreNSSTest, UserSlotRestrictionDisallows) {
  std::unique_ptr<SystemTrustStore> system_trust_store =
      CreateSslSystemTrustStoreNSSWithUserSlotRestriction(
          crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())));

  ASSERT_NO_FATAL_FAILURE(ImportRootCertAsTrusted(other_test_nssdb_.slot()));

  CertificateTrust trust =
      system_trust_store->GetTrustStore()->GetTrust(parsed_root_cert_.get(),
                                                    /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

// Tests that SystemTrustStore created for NSS without allowing trust for
// certificate stored on user slots.
TEST_F(SystemTrustStoreNSSTest, NoUserSlots) {
  std::unique_ptr<SystemTrustStore> system_trust_store =
      CreateSslSystemTrustStoreNSSWithUserSlotRestriction(nullptr);

  ASSERT_NO_FATAL_FAILURE(ImportRootCertAsTrusted(test_nssdb_.slot()));

  CertificateTrust trust =
      system_trust_store->GetTrustStore()->GetTrust(parsed_root_cert_.get(),
                                                    /*debug_data=*/nullptr);
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

}  // namespace

}  // namespace net
