// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_verify_proc.h"

#include <vector>

#include "base/file_path.h"
#include "base/string_number_conversions.h"
#include "base/sha1.h"
#include "net/base/asn1_util.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_test_util.h"
#include "net/base/cert_verifier.h"
#include "net/base/cert_verify_result.h"
#include "net/base/crl_set.h"
#include "net/base/net_errors.h"
#include "net/base/test_certificate_data.h"
#include "net/base/test_root_certs.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

using base::HexEncode;

namespace net {

namespace {

// A certificate for www.paypal.com with a NULL byte in the common name.
// From http://www.gossamer-threads.com/lists/fulldisc/full-disclosure/70363
unsigned char paypal_null_fingerprint[] = {
  0x4c, 0x88, 0x9e, 0x28, 0xd7, 0x7a, 0x44, 0x1e, 0x13, 0xf2, 0x6a, 0xba,
  0x1f, 0xe8, 0x1b, 0xd6, 0xab, 0x7b, 0xe8, 0xd7
};

}  // namespace

class CertVerifyProcTest : public testing::Test {
 public:
  CertVerifyProcTest()
      : verify_proc_(CertVerifyProc::CreateDefault()) {
  }
  virtual ~CertVerifyProcTest() {}

 protected:
  int Verify(X509Certificate* cert,
             const std::string& hostname,
             int flags,
             CRLSet* crl_set,
             CertVerifyResult* verify_result) {
    return verify_proc_->Verify(cert, hostname, flags, crl_set,
                                verify_result);
  }

 private:
  scoped_refptr<CertVerifyProc> verify_proc_;
};

TEST_F(CertVerifyProcTest, WithoutRevocationChecking) {
  // Check that verification without revocation checking works.
  CertificateList certs = CreateCertificateListFromFile(
      GetTestCertsDirectory(),
      "googlenew.chain.pem",
      X509Certificate::FORMAT_PEM_CERT_SEQUENCE);

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(certs[1]->os_cert_handle());

  scoped_refptr<X509Certificate> google_full_chain =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);

  CertVerifyResult verify_result;
  EXPECT_EQ(OK, Verify(google_full_chain, "www.google.com", 0 /* flags */,
                       NULL, &verify_result));
}

#if defined(OS_ANDROID) || defined(USE_OPENSSL)
// TODO(jnd): http://crbug.com/117478 - EV verification is not yet supported.
#define MAYBE_EVVerification DISABLED_EVVerification
#else
#define MAYBE_EVVerification EVVerification
#endif
TEST_F(CertVerifyProcTest, MAYBE_EVVerification) {
  // This certificate will expire Jun 21, 2013.
  CertificateList certs = CreateCertificateListFromFile(
      GetTestCertsDirectory(),
      "comodo.chain.pem",
      X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_EQ(3U, certs.size());

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(certs[1]->os_cert_handle());
  intermediates.push_back(certs[2]->os_cert_handle());

  scoped_refptr<X509Certificate> comodo_chain =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);

  scoped_refptr<CRLSet> crl_set(CRLSet::EmptyCRLSetForTesting());
  CertVerifyResult verify_result;
  int flags = CertVerifier::VERIFY_EV_CERT;
  int error = Verify(comodo_chain, "comodo.com", flags, crl_set.get(),
                     &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_IS_EV);
}

TEST_F(CertVerifyProcTest, PaypalNullCertParsing) {
  scoped_refptr<X509Certificate> paypal_null_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(paypal_null_der),
          sizeof(paypal_null_der)));

  ASSERT_NE(static_cast<X509Certificate*>(NULL), paypal_null_cert);

  const SHA1HashValue& fingerprint =
      paypal_null_cert->fingerprint();
  for (size_t i = 0; i < 20; ++i)
    EXPECT_EQ(paypal_null_fingerprint[i], fingerprint.data[i]);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(paypal_null_cert, "www.paypal.com", flags, NULL,
                     &verify_result);
#if defined(USE_NSS)
  EXPECT_EQ(ERR_CERT_COMMON_NAME_INVALID, error);
#else
  // TOOD(bulach): investigate why macosx and win aren't returning
  // ERR_CERT_INVALID or ERR_CERT_COMMON_NAME_INVALID.
  EXPECT_EQ(ERR_CERT_AUTHORITY_INVALID, error);
#endif
  // Either the system crypto library should correctly report a certificate
  // name mismatch, or our certificate blacklist should cause us to report an
  // invalid certificate.
#if defined(USE_NSS) || defined(OS_WIN)
  EXPECT_TRUE(verify_result.cert_status &
              (CERT_STATUS_COMMON_NAME_INVALID | CERT_STATUS_INVALID));
#endif
}

// A regression test for http://crbug.com/31497.
// This certificate will expire on 2012-04-08. The test will still
// pass if error == ERR_CERT_DATE_INVALID.  TODO(wtc): generate test
// certificates for this unit test. http://crbug.com/111742
TEST_F(CertVerifyProcTest, IntermediateCARequireExplicitPolicy) {
  FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "www_us_army_mil_cert.der");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), server_cert);

  // The intermediate CA certificate's policyConstraints extension has a
  // requireExplicitPolicy field with SkipCerts=0.
  scoped_refptr<X509Certificate> intermediate_cert =
      ImportCertFromFile(certs_dir, "dod_ca_17_cert.der");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate_cert);

  scoped_refptr<X509Certificate> root_cert =
      ImportCertFromFile(certs_dir, "dod_root_ca_2_cert.der");
  ScopedTestRoot scoped_root(root_cert);

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(intermediate_cert->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert_chain, "www.us.army.mil", flags, NULL,
                     &verify_result);
  if (error == OK) {
    EXPECT_EQ(0U, verify_result.cert_status);
  } else {
    EXPECT_EQ(ERR_CERT_DATE_INVALID, error);
    EXPECT_EQ(CERT_STATUS_DATE_INVALID, verify_result.cert_status);
  }
}


// Test for bug 58437.
// This certificate will expire on 2011-12-21. The test will still
// pass if error == ERR_CERT_DATE_INVALID.
// This test is DISABLED because it appears that we cannot do
// certificate revocation checking when running all of the net unit tests.
// This test passes when run individually, but when run with all of the net
// unit tests, the call to PKIXVerifyCert returns the NSS error -8180, which is
// SEC_ERROR_REVOKED_CERTIFICATE. This indicates a lack of revocation
// status, i.e. that the revocation check is failing for some reason.
TEST_F(CertVerifyProcTest, DISABLED_GlobalSignR3EVTest) {
  FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "2029_globalsign_com_cert.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), server_cert);

  scoped_refptr<X509Certificate> intermediate_cert =
      ImportCertFromFile(certs_dir, "globalsign_ev_sha256_ca_cert.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate_cert);

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(intermediate_cert->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);

  CertVerifyResult verify_result;
  int flags = CertVerifier::VERIFY_REV_CHECKING_ENABLED |
              CertVerifier::VERIFY_EV_CERT;
  int error = Verify(cert_chain, "2029.globalsign.com", flags, NULL,
                     &verify_result);
  if (error == OK)
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_IS_EV);
  else
    EXPECT_EQ(ERR_CERT_DATE_INVALID, error);
}

// Test that verifying an ECDSA certificate doesn't crash on XP. (See
// crbug.com/144466).
TEST_F(CertVerifyProcTest, ECDSA_RSA) {
  FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir,
                         "prime256v1-ecdsa-ee-by-1024-rsa-intermediate.pem");

  CertVerifyResult verify_result;
  Verify(cert, "127.0.0.1", 0, NULL, &verify_result);

  // We don't check verify_result because the certificate is signed by an
  // unknown CA and will be considered invalid on XP because of the ECDSA
  // public key.
}

// Currently, only RSA and DSA keys are checked for weakness, and our example
// weak size is 768. These could change in the future.
//
// Note that this means there may be false negatives: keys for other
// algorithms and which are weak will pass this test.
static bool IsWeakKeyType(const std::string& key_type) {
  size_t pos = key_type.find("-");
  std::string size = key_type.substr(0, pos);
  std::string type = key_type.substr(pos + 1);

  if (type == "rsa" || type == "dsa")
    return size == "768";

  return false;
}

TEST_F(CertVerifyProcTest, RejectWeakKeys) {
  FilePath certs_dir = GetTestCertsDirectory();
  typedef std::vector<std::string> Strings;
  Strings key_types;

  // generate-weak-test-chains.sh currently has:
  //     key_types="768-rsa 1024-rsa 2048-rsa prime256v1-ecdsa"
  // We must use the same key types here. The filenames generated look like:
  //     2048-rsa-ee-by-768-rsa-intermediate.pem
  key_types.push_back("768-rsa");
  key_types.push_back("1024-rsa");
  key_types.push_back("2048-rsa");

  bool use_ecdsa = true;
#if defined(OS_WIN)
  use_ecdsa = base::win::GetVersion() > base::win::VERSION_XP;
#endif

  if (use_ecdsa)
    key_types.push_back("prime256v1-ecdsa");

  // Add the root that signed the intermediates for this test.
  scoped_refptr<X509Certificate> root_cert =
      ImportCertFromFile(certs_dir, "2048-rsa-root.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), root_cert);
  ScopedTestRoot scoped_root(root_cert);

  // Now test each chain.
  for (Strings::const_iterator ee_type = key_types.begin();
       ee_type != key_types.end(); ++ee_type) {
    for (Strings::const_iterator signer_type = key_types.begin();
         signer_type != key_types.end(); ++signer_type) {
      std::string basename = *ee_type + "-ee-by-" + *signer_type +
          "-intermediate.pem";
      SCOPED_TRACE(basename);
      scoped_refptr<X509Certificate> ee_cert =
          ImportCertFromFile(certs_dir, basename);
      ASSERT_NE(static_cast<X509Certificate*>(NULL), ee_cert);

      basename = *signer_type + "-intermediate.pem";
      scoped_refptr<X509Certificate> intermediate =
          ImportCertFromFile(certs_dir, basename);
      ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate);

      X509Certificate::OSCertHandles intermediates;
      intermediates.push_back(intermediate->os_cert_handle());
      scoped_refptr<X509Certificate> cert_chain =
          X509Certificate::CreateFromHandle(ee_cert->os_cert_handle(),
                                            intermediates);

      CertVerifyResult verify_result;
      int error = Verify(cert_chain, "127.0.0.1", 0, NULL, &verify_result);

      if (IsWeakKeyType(*ee_type) || IsWeakKeyType(*signer_type)) {
        EXPECT_NE(OK, error);
        EXPECT_EQ(CERT_STATUS_WEAK_KEY,
                  verify_result.cert_status & CERT_STATUS_WEAK_KEY);
        EXPECT_NE(CERT_STATUS_INVALID,
                  verify_result.cert_status & CERT_STATUS_INVALID);
      } else {
        EXPECT_EQ(OK, error);
        EXPECT_EQ(0U, verify_result.cert_status & CERT_STATUS_WEAK_KEY);
      }
    }
  }
}

// Test for bug 108514.
// The certificate will expire on 2012-07-20. The test will still
// pass if error == ERR_CERT_DATE_INVALID.  TODO(rsleevi): generate test
// certificates for this unit test.  http://crbug.com/111730
TEST_F(CertVerifyProcTest, ExtraneousMD5RootCert) {
  FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "images_etrade_wallst_com.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), server_cert);

  scoped_refptr<X509Certificate> intermediate_cert =
      ImportCertFromFile(certs_dir, "globalsign_orgv1_ca.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate_cert);

  scoped_refptr<X509Certificate> md5_root_cert =
      ImportCertFromFile(certs_dir, "globalsign_root_ca_md5.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), md5_root_cert);

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(intermediate_cert->os_cert_handle());
  intermediates.push_back(md5_root_cert->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);

  CertVerifyResult verify_result;
  int flags = 0;
  int error = Verify(cert_chain, "images.etrade.wallst.com", flags, NULL,
                     &verify_result);
  if (error != OK)
    EXPECT_EQ(ERR_CERT_DATE_INVALID, error);

  EXPECT_FALSE(verify_result.has_md5);
  EXPECT_FALSE(verify_result.has_md5_ca);
}

// Test for bug 94673.
TEST_F(CertVerifyProcTest, GoogleDigiNotarTest) {
  FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "google_diginotar.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), server_cert);

  scoped_refptr<X509Certificate> intermediate_cert =
      ImportCertFromFile(certs_dir, "diginotar_public_ca_2025.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate_cert);

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(intermediate_cert->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        intermediates);

  CertVerifyResult verify_result;
  int flags = CertVerifier::VERIFY_REV_CHECKING_ENABLED;
  int error = Verify(cert_chain, "mail.google.com", flags, NULL,
                     &verify_result);
  EXPECT_NE(OK, error);

  // Now turn off revocation checking.  Certificate verification should still
  // fail.
  flags = 0;
  error = Verify(cert_chain, "mail.google.com", flags, NULL, &verify_result);
  EXPECT_NE(OK, error);
}

TEST_F(CertVerifyProcTest, DigiNotarCerts) {
  static const char* const kDigiNotarFilenames[] = {
    "diginotar_root_ca.pem",
    "diginotar_cyber_ca.pem",
    "diginotar_services_1024_ca.pem",
    "diginotar_pkioverheid.pem",
    "diginotar_pkioverheid_g2.pem",
    NULL,
  };

  FilePath certs_dir = GetTestCertsDirectory();

  for (size_t i = 0; kDigiNotarFilenames[i]; i++) {
    scoped_refptr<X509Certificate> diginotar_cert =
        ImportCertFromFile(certs_dir, kDigiNotarFilenames[i]);
    std::string der_bytes;
    ASSERT_TRUE(X509Certificate::GetDEREncoded(
        diginotar_cert->os_cert_handle(), &der_bytes));

    base::StringPiece spki;
    ASSERT_TRUE(asn1::ExtractSPKIFromDERCert(der_bytes, &spki));

    std::string spki_sha1 = base::SHA1HashString(spki.as_string());

    HashValueVector public_keys;
    HashValue hash(HASH_VALUE_SHA1);
    ASSERT_EQ(hash.size(), spki_sha1.size());
    memcpy(hash.data(), spki_sha1.data(), spki_sha1.size());
    public_keys.push_back(hash);

    EXPECT_TRUE(CertVerifyProc::IsPublicKeyBlacklisted(public_keys)) <<
        "Public key not blocked for " << kDigiNotarFilenames[i];
  }
}

TEST_F(CertVerifyProcTest, TestKnownRoot) {
  FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "certse.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(certs[1]->os_cert_handle());
  intermediates.push_back(certs[2]->os_cert_handle());

  scoped_refptr<X509Certificate> cert_chain =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);

  int flags = 0;
  CertVerifyResult verify_result;
  // This will blow up, June 8th, 2014. Sorry! Please disable and file a bug
  // against agl. See also PublicKeyHashes.
  int error = Verify(cert_chain, "cert.se", flags, NULL, &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(0U, verify_result.cert_status);
  EXPECT_TRUE(verify_result.is_issued_by_known_root);
}

TEST_F(CertVerifyProcTest, PublicKeyHashes) {
  FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "certse.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(certs[1]->os_cert_handle());
  intermediates.push_back(certs[2]->os_cert_handle());

  scoped_refptr<X509Certificate> cert_chain =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);
  int flags = 0;
  CertVerifyResult verify_result;

  // This will blow up, June 8th, 2014. Sorry! Please disable and file a bug
  // against agl. See also TestKnownRoot.
  int error = Verify(cert_chain, "cert.se", flags, NULL, &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(0U, verify_result.cert_status);
  ASSERT_LE(3u, verify_result.public_key_hashes.size());

  HashValueVector sha1_hashes;
  for (unsigned i = 0; i < verify_result.public_key_hashes.size(); ++i) {
    if (verify_result.public_key_hashes[i].tag != HASH_VALUE_SHA1)
      continue;
    sha1_hashes.push_back(verify_result.public_key_hashes[i]);
  }
  ASSERT_LE(3u, sha1_hashes.size());

  for (unsigned i = 0; i < 3; ++i) {
    EXPECT_EQ(HexEncode(kCertSESPKIs[i], base::kSHA1Length),
              HexEncode(sha1_hashes[i].data(), base::kSHA1Length));
  }
}

// A regression test for http://crbug.com/70293.
// The Key Usage extension in this RSA SSL server certificate does not have
// the keyEncipherment bit.
TEST_F(CertVerifyProcTest, InvalidKeyUsage) {
  FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "invalid_key_usage_cert.der");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), server_cert);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(server_cert, "jira.aquameta.com", flags, NULL,
                     &verify_result);
#if defined(USE_OPENSSL)
  // This certificate has two errors: "invalid key usage" and "untrusted CA".
  // However, OpenSSL returns only one (the latter), and we can't detect
  // the other errors.
  EXPECT_EQ(ERR_CERT_AUTHORITY_INVALID, error);
#else
  EXPECT_EQ(ERR_CERT_INVALID, error);
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
#endif
  // TODO(wtc): fix http://crbug.com/75520 to get all the certificate errors
  // from NSS.
#if !defined(USE_NSS)
  // The certificate is issued by an unknown CA.
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
#endif
}

// Basic test for returning the chain in CertVerifyResult. Note that the
// returned chain may just be a reflection of the originally supplied chain;
// that is, if any errors occur, the default chain returned is an exact copy
// of the certificate to be verified. The remaining VerifyReturn* tests are
// used to ensure that the actual, verified chain is being returned by
// Verify().
TEST_F(CertVerifyProcTest, VerifyReturnChainBasic) {
  FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "x509_verify_results.chain.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(certs[1]->os_cert_handle());
  intermediates.push_back(certs[2]->os_cert_handle());

  ScopedTestRoot scoped_root(certs[2]);

  scoped_refptr<X509Certificate> google_full_chain =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_full_chain);
  ASSERT_EQ(2U, google_full_chain->GetIntermediateCertificates().size());

  CertVerifyResult verify_result;
  EXPECT_EQ(static_cast<X509Certificate*>(NULL), verify_result.verified_cert);
  int error = Verify(google_full_chain, "127.0.0.1", 0, NULL, &verify_result);
  EXPECT_EQ(OK, error);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), verify_result.verified_cert);

  EXPECT_NE(google_full_chain, verify_result.verified_cert);
  EXPECT_TRUE(X509Certificate::IsSameOSCert(
      google_full_chain->os_cert_handle(),
      verify_result.verified_cert->os_cert_handle()));
  const X509Certificate::OSCertHandles& return_intermediates =
      verify_result.verified_cert->GetIntermediateCertificates();
  ASSERT_EQ(2U, return_intermediates.size());
  EXPECT_TRUE(X509Certificate::IsSameOSCert(return_intermediates[0],
                                            certs[1]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(return_intermediates[1],
                                            certs[2]->os_cert_handle()));
}

// Test that the certificate returned in CertVerifyResult is able to reorder
// certificates that are not ordered from end-entity to root. While this is
// a protocol violation if sent during a TLS handshake, if multiple sources
// of intermediate certificates are combined, it's possible that order may
// not be maintained.
TEST_F(CertVerifyProcTest, VerifyReturnChainProperlyOrdered) {
  FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "x509_verify_results.chain.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  // Construct the chain out of order.
  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(certs[2]->os_cert_handle());
  intermediates.push_back(certs[1]->os_cert_handle());

  ScopedTestRoot scoped_root(certs[2]);

  scoped_refptr<X509Certificate> google_full_chain =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_full_chain);
  ASSERT_EQ(2U, google_full_chain->GetIntermediateCertificates().size());

  CertVerifyResult verify_result;
  EXPECT_EQ(static_cast<X509Certificate*>(NULL), verify_result.verified_cert);
  int error = Verify(google_full_chain, "127.0.0.1", 0, NULL, &verify_result);
  EXPECT_EQ(OK, error);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), verify_result.verified_cert);

  EXPECT_NE(google_full_chain, verify_result.verified_cert);
  EXPECT_TRUE(X509Certificate::IsSameOSCert(
      google_full_chain->os_cert_handle(),
      verify_result.verified_cert->os_cert_handle()));
  const X509Certificate::OSCertHandles& return_intermediates =
      verify_result.verified_cert->GetIntermediateCertificates();
  ASSERT_EQ(2U, return_intermediates.size());
  EXPECT_TRUE(X509Certificate::IsSameOSCert(return_intermediates[0],
                                            certs[1]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(return_intermediates[1],
                                            certs[2]->os_cert_handle()));
}

// Test that Verify() filters out certificates which are not related to
// or part of the certificate chain being verified.
TEST_F(CertVerifyProcTest, VerifyReturnChainFiltersUnrelatedCerts) {
  FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "x509_verify_results.chain.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());
  ScopedTestRoot scoped_root(certs[2]);

  scoped_refptr<X509Certificate> unrelated_dod_certificate =
      ImportCertFromFile(certs_dir, "dod_ca_17_cert.der");
  scoped_refptr<X509Certificate> unrelated_dod_certificate2 =
      ImportCertFromFile(certs_dir, "dod_root_ca_2_cert.der");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), unrelated_dod_certificate);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), unrelated_dod_certificate2);

  // Interject unrelated certificates into the list of intermediates.
  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(unrelated_dod_certificate->os_cert_handle());
  intermediates.push_back(certs[1]->os_cert_handle());
  intermediates.push_back(unrelated_dod_certificate2->os_cert_handle());
  intermediates.push_back(certs[2]->os_cert_handle());

  scoped_refptr<X509Certificate> google_full_chain =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_full_chain);
  ASSERT_EQ(4U, google_full_chain->GetIntermediateCertificates().size());

  CertVerifyResult verify_result;
  EXPECT_EQ(static_cast<X509Certificate*>(NULL), verify_result.verified_cert);
  int error = Verify(google_full_chain, "127.0.0.1", 0, NULL, &verify_result);
  EXPECT_EQ(OK, error);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), verify_result.verified_cert);

  EXPECT_NE(google_full_chain, verify_result.verified_cert);
  EXPECT_TRUE(X509Certificate::IsSameOSCert(
      google_full_chain->os_cert_handle(),
      verify_result.verified_cert->os_cert_handle()));
  const X509Certificate::OSCertHandles& return_intermediates =
      verify_result.verified_cert->GetIntermediateCertificates();
  ASSERT_EQ(2U, return_intermediates.size());
  EXPECT_TRUE(X509Certificate::IsSameOSCert(return_intermediates[0],
                                            certs[1]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(return_intermediates[1],
                                            certs[2]->os_cert_handle()));
}

#if defined(USE_NSS) || defined(OS_WIN) || defined(OS_MACOSX)
static const uint8 kCRLSetThawteSPKIBlocked[] = {
  0x8e, 0x00, 0x7b, 0x22, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x22, 0x3a,
  0x30, 0x2c, 0x22, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x54, 0x79, 0x70,
  0x65, 0x22, 0x3a, 0x22, 0x43, 0x52, 0x4c, 0x53, 0x65, 0x74, 0x22, 0x2c, 0x22,
  0x53, 0x65, 0x71, 0x75, 0x65, 0x6e, 0x63, 0x65, 0x22, 0x3a, 0x30, 0x2c, 0x22,
  0x44, 0x65, 0x6c, 0x74, 0x61, 0x46, 0x72, 0x6f, 0x6d, 0x22, 0x3a, 0x30, 0x2c,
  0x22, 0x4e, 0x75, 0x6d, 0x50, 0x61, 0x72, 0x65, 0x6e, 0x74, 0x73, 0x22, 0x3a,
  0x30, 0x2c, 0x22, 0x42, 0x6c, 0x6f, 0x63, 0x6b, 0x65, 0x64, 0x53, 0x50, 0x4b,
  0x49, 0x73, 0x22, 0x3a, 0x5b, 0x22, 0x36, 0x58, 0x36, 0x4d, 0x78, 0x52, 0x37,
  0x58, 0x70, 0x4d, 0x51, 0x4b, 0x78, 0x49, 0x41, 0x39, 0x50, 0x6a, 0x36, 0x37,
  0x36, 0x38, 0x76, 0x74, 0x55, 0x6b, 0x6b, 0x7a, 0x48, 0x79, 0x7a, 0x41, 0x6f,
  0x6d, 0x6f, 0x4f, 0x68, 0x4b, 0x55, 0x6e, 0x7a, 0x73, 0x55, 0x3d, 0x22, 0x5d,
  0x7d,
};

static const uint8 kCRLSetThawteSerialBlocked[] = {
  0x60, 0x00, 0x7b, 0x22, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x22, 0x3a,
  0x30, 0x2c, 0x22, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x54, 0x79, 0x70,
  0x65, 0x22, 0x3a, 0x22, 0x43, 0x52, 0x4c, 0x53, 0x65, 0x74, 0x22, 0x2c, 0x22,
  0x53, 0x65, 0x71, 0x75, 0x65, 0x6e, 0x63, 0x65, 0x22, 0x3a, 0x30, 0x2c, 0x22,
  0x44, 0x65, 0x6c, 0x74, 0x61, 0x46, 0x72, 0x6f, 0x6d, 0x22, 0x3a, 0x30, 0x2c,
  0x22, 0x4e, 0x75, 0x6d, 0x50, 0x61, 0x72, 0x65, 0x6e, 0x74, 0x73, 0x22, 0x3a,
  0x31, 0x2c, 0x22, 0x42, 0x6c, 0x6f, 0x63, 0x6b, 0x65, 0x64, 0x53, 0x50, 0x4b,
  0x49, 0x73, 0x22, 0x3a, 0x5b, 0x5d, 0x7d, 0xb1, 0x12, 0x41, 0x42, 0xa5, 0xa1,
  0xa5, 0xa2, 0x88, 0x19, 0xc7, 0x35, 0x34, 0x0e, 0xff, 0x8c, 0x9e, 0x2f, 0x81,
  0x68, 0xfe, 0xe3, 0xba, 0x18, 0x7f, 0x25, 0x3b, 0xc1, 0xa3, 0x92, 0xd7, 0xe2,
  // Note that this is actually blocking two serial numbers because on XP and
  // Vista, CryptoAPI finds a different Thawte certificate.
  0x02, 0x00, 0x00, 0x00,
  0x04, 0x30, 0x00, 0x00, 0x02,
  0x04, 0x30, 0x00, 0x00, 0x06,
};

static const uint8 kCRLSetGoogleSerialBlocked[] = {
  0x60, 0x00, 0x7b, 0x22, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x22, 0x3a,
  0x30, 0x2c, 0x22, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x54, 0x79, 0x70,
  0x65, 0x22, 0x3a, 0x22, 0x43, 0x52, 0x4c, 0x53, 0x65, 0x74, 0x22, 0x2c, 0x22,
  0x53, 0x65, 0x71, 0x75, 0x65, 0x6e, 0x63, 0x65, 0x22, 0x3a, 0x30, 0x2c, 0x22,
  0x44, 0x65, 0x6c, 0x74, 0x61, 0x46, 0x72, 0x6f, 0x6d, 0x22, 0x3a, 0x30, 0x2c,
  0x22, 0x4e, 0x75, 0x6d, 0x50, 0x61, 0x72, 0x65, 0x6e, 0x74, 0x73, 0x22, 0x3a,
  0x31, 0x2c, 0x22, 0x42, 0x6c, 0x6f, 0x63, 0x6b, 0x65, 0x64, 0x53, 0x50, 0x4b,
  0x49, 0x73, 0x22, 0x3a, 0x5b, 0x5d, 0x7d, 0xe9, 0x7e, 0x8c, 0xc5, 0x1e, 0xd7,
  0xa4, 0xc4, 0x0a, 0xc4, 0x80, 0x3d, 0x3e, 0x3e, 0xbb, 0xeb, 0xcb, 0xed, 0x52,
  0x49, 0x33, 0x1f, 0x2c, 0xc0, 0xa2, 0x6a, 0x0e, 0x84, 0xa5, 0x27, 0xce, 0xc5,
  0x01, 0x00, 0x00, 0x00, 0x10, 0x4f, 0x9d, 0x96, 0xd9, 0x66, 0xb0, 0x99, 0x2b,
  0x54, 0xc2, 0x95, 0x7c, 0xb4, 0x15, 0x7d, 0x4d,
};

// Test that CRLSets are effective in making a certificate appear to be
// revoked.
TEST_F(CertVerifyProcTest, CRLSet) {
  CertificateList certs = CreateCertificateListFromFile(
      GetTestCertsDirectory(),
      "googlenew.chain.pem",
      X509Certificate::FORMAT_PEM_CERT_SEQUENCE);

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(certs[1]->os_cert_handle());

  scoped_refptr<X509Certificate> google_full_chain =
      X509Certificate::CreateFromHandle(certs[0]->os_cert_handle(),
                                        intermediates);

  CertVerifyResult verify_result;
  int error = Verify(google_full_chain, "www.google.com", 0, NULL,
                     &verify_result);
  EXPECT_EQ(OK, error);

  // First test blocking by SPKI.
  base::StringPiece crl_set_bytes(
      reinterpret_cast<const char*>(kCRLSetThawteSPKIBlocked),
      sizeof(kCRLSetThawteSPKIBlocked));
  scoped_refptr<CRLSet> crl_set;
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  error = Verify(google_full_chain, "www.google.com", 0, crl_set.get(),
                 &verify_result);
  EXPECT_EQ(ERR_CERT_REVOKED, error);

  // Second, test revocation by serial number of a cert directly under the
  // root.
  crl_set_bytes = base::StringPiece(
      reinterpret_cast<const char*>(kCRLSetThawteSerialBlocked),
      sizeof(kCRLSetThawteSerialBlocked));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  error = Verify(google_full_chain, "www.google.com", 0, crl_set.get(),
                 &verify_result);
  EXPECT_EQ(ERR_CERT_REVOKED, error);

  // Lastly, test revocation by serial number of a certificate not under the
  // root.
  crl_set_bytes = base::StringPiece(
      reinterpret_cast<const char*>(kCRLSetGoogleSerialBlocked),
      sizeof(kCRLSetGoogleSerialBlocked));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  error = Verify(google_full_chain, "www.google.com", 0, crl_set.get(),
                 &verify_result);
  EXPECT_EQ(ERR_CERT_REVOKED, error);
}
#endif

struct WeakDigestTestData {
  const char* root_cert_filename;
  const char* intermediate_cert_filename;
  const char* ee_cert_filename;
  bool expected_has_md5;
  bool expected_has_md4;
  bool expected_has_md2;
  bool expected_has_md5_ca;
  bool expected_has_md2_ca;
};

// GTest 'magic' pretty-printer, so that if/when a test fails, it knows how
// to output the parameter that was passed. Without this, it will simply
// attempt to print out the first twenty bytes of the object, which depending
// on platform and alignment, may result in an invalid read.
void PrintTo(const WeakDigestTestData& data, std::ostream* os) {
  *os << "root: "
      << (data.root_cert_filename ? data.root_cert_filename : "none")
      << "; intermediate: " << data.intermediate_cert_filename
      << "; end-entity: " << data.ee_cert_filename;
}

class CertVerifyProcWeakDigestTest
    : public CertVerifyProcTest,
      public testing::WithParamInterface<WeakDigestTestData> {
 public:
  CertVerifyProcWeakDigestTest() {}
  virtual ~CertVerifyProcWeakDigestTest() {}
};

TEST_P(CertVerifyProcWeakDigestTest, Verify) {
  WeakDigestTestData data = GetParam();
  FilePath certs_dir = GetTestCertsDirectory();

  ScopedTestRoot test_root;
  if (data.root_cert_filename) {
     scoped_refptr<X509Certificate> root_cert =
         ImportCertFromFile(certs_dir, data.root_cert_filename);
     ASSERT_NE(static_cast<X509Certificate*>(NULL), root_cert);
     test_root.Reset(root_cert);
  }

  scoped_refptr<X509Certificate> intermediate_cert =
      ImportCertFromFile(certs_dir, data.intermediate_cert_filename);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate_cert);
  scoped_refptr<X509Certificate> ee_cert =
      ImportCertFromFile(certs_dir, data.ee_cert_filename);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), ee_cert);

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(intermediate_cert->os_cert_handle());

  scoped_refptr<X509Certificate> ee_chain =
      X509Certificate::CreateFromHandle(ee_cert->os_cert_handle(),
                                        intermediates);
  ASSERT_NE(static_cast<X509Certificate*>(NULL), ee_chain);

  int flags = 0;
  CertVerifyResult verify_result;
  int rv = Verify(ee_chain, "127.0.0.1", flags, NULL, &verify_result);
  EXPECT_EQ(data.expected_has_md5, verify_result.has_md5);
  EXPECT_EQ(data.expected_has_md4, verify_result.has_md4);
  EXPECT_EQ(data.expected_has_md2, verify_result.has_md2);
  EXPECT_EQ(data.expected_has_md5_ca, verify_result.has_md5_ca);
  EXPECT_EQ(data.expected_has_md2_ca, verify_result.has_md2_ca);

  // Ensure that MD4 and MD2 are tagged as invalid.
  if (data.expected_has_md4 || data.expected_has_md2) {
    EXPECT_EQ(CERT_STATUS_INVALID,
              verify_result.cert_status & CERT_STATUS_INVALID);
  }

  // Ensure that MD5 is flagged as weak.
  if (data.expected_has_md5) {
    EXPECT_EQ(
        CERT_STATUS_WEAK_SIGNATURE_ALGORITHM,
        verify_result.cert_status & CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);
  }

  // If a root cert is present, then check that the chain was rejected if any
  // weak algorithms are present. This is only checked when a root cert is
  // present because the error reported for incomplete chains with weak
  // algorithms depends on which implementation was used to validate (NSS,
  // OpenSSL, CryptoAPI, Security.framework) and upon which weak algorithm
  // present (MD2, MD4, MD5).
  if (data.root_cert_filename) {
    if (data.expected_has_md4 || data.expected_has_md2) {
      EXPECT_EQ(ERR_CERT_INVALID, rv);
    } else if (data.expected_has_md5) {
      EXPECT_EQ(ERR_CERT_WEAK_SIGNATURE_ALGORITHM, rv);
    } else {
      EXPECT_EQ(OK, rv);
    }
  }
}

// Unlike TEST/TEST_F, which are macros that expand to further macros,
// INSTANTIATE_TEST_CASE_P is a macro that expands directly to code that
// stringizes the arguments. As a result, macros passed as parameters (such as
// prefix or test_case_name) will not be expanded by the preprocessor. To work
// around this, indirect the macro for INSTANTIATE_TEST_CASE_P, so that the
// pre-processor will expand macros such as MAYBE_test_name before
// instantiating the test.
#define WRAPPED_INSTANTIATE_TEST_CASE_P(prefix, test_case_name, generator) \
    INSTANTIATE_TEST_CASE_P(prefix, test_case_name, generator)

// The signature algorithm of the root CA should not matter.
const WeakDigestTestData kVerifyRootCATestData[] = {
  { "weak_digest_md5_root.pem", "weak_digest_sha1_intermediate.pem",
    "weak_digest_sha1_ee.pem", false, false, false, false, false },
#if defined(USE_OPENSSL) || defined(OS_WIN)
  // MD4 is not supported by OS X / NSS
  { "weak_digest_md4_root.pem", "weak_digest_sha1_intermediate.pem",
    "weak_digest_sha1_ee.pem", false, false, false, false, false },
#endif
  { "weak_digest_md2_root.pem", "weak_digest_sha1_intermediate.pem",
    "weak_digest_sha1_ee.pem", false, false, false, false, false },
};
INSTANTIATE_TEST_CASE_P(VerifyRoot, CertVerifyProcWeakDigestTest,
                        testing::ValuesIn(kVerifyRootCATestData));

// The signature algorithm of intermediates should be properly detected.
const WeakDigestTestData kVerifyIntermediateCATestData[] = {
  { "weak_digest_sha1_root.pem", "weak_digest_md5_intermediate.pem",
    "weak_digest_sha1_ee.pem", true, false, false, true, false },
#if defined(USE_OPENSSL) || defined(OS_WIN)
  // MD4 is not supported by OS X / NSS
  { "weak_digest_sha1_root.pem", "weak_digest_md4_intermediate.pem",
    "weak_digest_sha1_ee.pem", false, true, false, false, false },
#endif
#if !defined(USE_NSS)  // MD2 is disabled by default.
  { "weak_digest_sha1_root.pem", "weak_digest_md2_intermediate.pem",
    "weak_digest_sha1_ee.pem", false, false, true, false, true },
#endif
};
INSTANTIATE_TEST_CASE_P(VerifyIntermediate, CertVerifyProcWeakDigestTest,
                        testing::ValuesIn(kVerifyIntermediateCATestData));

// The signature algorithm of end-entity should be properly detected.
const WeakDigestTestData kVerifyEndEntityTestData[] = {
  { "weak_digest_sha1_root.pem", "weak_digest_sha1_intermediate.pem",
    "weak_digest_md5_ee.pem", true, false, false, false, false },
#if defined(USE_OPENSSL) || defined(OS_WIN)
  // MD4 is not supported by OS X / NSS
  { "weak_digest_sha1_root.pem", "weak_digest_sha1_intermediate.pem",
    "weak_digest_md4_ee.pem", false, true, false, false, false },
#endif
#if !defined(USE_NSS)  // MD2 is disabled by default.
  { "weak_digest_sha1_root.pem", "weak_digest_sha1_intermediate.pem",
    "weak_digest_md2_ee.pem", false, false, true, false, false },
#endif
};
// Disabled on NSS - NSS caches chains/signatures in such a way that cannot
// be cleared until NSS is cleanly shutdown, which is not presently supported
// in Chromium.
#if defined(USE_NSS)
#define MAYBE_VerifyEndEntity DISABLED_VerifyEndEntity
#else
#define MAYBE_VerifyEndEntity VerifyEndEntity
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(MAYBE_VerifyEndEntity,
                                CertVerifyProcWeakDigestTest,
                                testing::ValuesIn(kVerifyEndEntityTestData));

// Incomplete chains should still report the status of the intermediate.
const WeakDigestTestData kVerifyIncompleteIntermediateTestData[] = {
  { NULL, "weak_digest_md5_intermediate.pem", "weak_digest_sha1_ee.pem",
    true, false, false, true, false },
#if defined(USE_OPENSSL) || defined(OS_WIN)
  // MD4 is not supported by OS X / NSS
  { NULL, "weak_digest_md4_intermediate.pem", "weak_digest_sha1_ee.pem",
    false, true, false, false, false },
#endif
  { NULL, "weak_digest_md2_intermediate.pem", "weak_digest_sha1_ee.pem",
    false, false, true, false, true },
};
// Disabled on NSS - libpkix does not return constructed chains on error,
// preventing us from detecting/inspecting the verified chain.
#if defined(USE_NSS)
#define MAYBE_VerifyIncompleteIntermediate \
    DISABLED_VerifyIncompleteIntermediate
#else
#define MAYBE_VerifyIncompleteIntermediate VerifyIncompleteIntermediate
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_VerifyIncompleteIntermediate,
    CertVerifyProcWeakDigestTest,
    testing::ValuesIn(kVerifyIncompleteIntermediateTestData));

// Incomplete chains should still report the status of the end-entity.
const WeakDigestTestData kVerifyIncompleteEETestData[] = {
  { NULL, "weak_digest_sha1_intermediate.pem", "weak_digest_md5_ee.pem",
    true, false, false, false, false },
#if defined(USE_OPENSSL) || defined(OS_WIN)
  // MD4 is not supported by OS X / NSS
  { NULL, "weak_digest_sha1_intermediate.pem", "weak_digest_md4_ee.pem",
    false, true, false, false, false },
#endif
  { NULL, "weak_digest_sha1_intermediate.pem", "weak_digest_md2_ee.pem",
    false, false, true, false, false },
};
// Disabled on NSS - libpkix does not return constructed chains on error,
// preventing us from detecting/inspecting the verified chain.
#if defined(USE_NSS)
#define MAYBE_VerifyIncompleteEndEntity DISABLED_VerifyIncompleteEndEntity
#else
#define MAYBE_VerifyIncompleteEndEntity VerifyIncompleteEndEntity
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_VerifyIncompleteEndEntity,
    CertVerifyProcWeakDigestTest,
    testing::ValuesIn(kVerifyIncompleteEETestData));

// Differing algorithms between the intermediate and the EE should still be
// reported.
const WeakDigestTestData kVerifyMixedTestData[] = {
  { "weak_digest_sha1_root.pem", "weak_digest_md5_intermediate.pem",
    "weak_digest_md2_ee.pem", true, false, true, true, false },
  { "weak_digest_sha1_root.pem", "weak_digest_md2_intermediate.pem",
    "weak_digest_md5_ee.pem", true, false, true, false, true },
#if defined(USE_OPENSSL) || defined(OS_WIN)
  // MD4 is not supported by OS X / NSS
  { "weak_digest_sha1_root.pem", "weak_digest_md4_intermediate.pem",
    "weak_digest_md2_ee.pem", false, true, true, false, false },
#endif
};
// NSS does not support MD4 and does not enable MD2 by default, making all
// permutations invalid.
#if defined(USE_NSS)
#define MAYBE_VerifyMixed DISABLED_VerifyMixed
#else
#define MAYBE_VerifyMixed VerifyMixed
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_VerifyMixed,
    CertVerifyProcWeakDigestTest,
    testing::ValuesIn(kVerifyMixedTestData));

}  // namespace net
