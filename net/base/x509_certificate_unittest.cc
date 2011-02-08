// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/rsa_private_key.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_test_util.h"
#include "net/base/cert_verify_result.h"
#include "net/base/net_errors.h"
#include "net/base/test_certificate_data.h"
#include "net/base/test_root_certs.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

// Unit tests aren't allowed to access external resources. Unfortunately, to
// properly verify the EV-ness of a cert, we need to check for its revocation
// through online servers. If you're manually running unit tests, feel free to
// turn this on to test EV certs. But leave it turned off for the automated
// testing.
#define ALLOW_EXTERNAL_ACCESS 0

#if ALLOW_EXTERNAL_ACCESS && defined(OS_WIN)
#define TEST_EV 1  // Test CERT_STATUS_IS_EV
#endif

using base::Time;

namespace net {

// Certificates for test data. They're obtained with:
//
// $ openssl s_client -connect [host]:443 -showcerts > /tmp/host.pem < /dev/null
// $ openssl x509 -inform PEM -outform DER < /tmp/host.pem > /tmp/host.der
//
// For fingerprint
// $ openssl x509 -inform DER -fingerprint -noout < /tmp/host.der

// For valid_start, valid_expiry
// $ openssl x509 -inform DER -text -noout < /tmp/host.der |
//    grep -A 2 Validity
// $ date +%s -d '<date str>'

// Google's cert.
unsigned char google_fingerprint[] = {
  0xab, 0xbe, 0x5e, 0xb4, 0x93, 0x88, 0x4e, 0xe4, 0x60, 0xc6, 0xef, 0xf8,
  0xea, 0xd4, 0xb1, 0x55, 0x4b, 0xc9, 0x59, 0x3c
};

// webkit.org's cert.
unsigned char webkit_fingerprint[] = {
  0xa1, 0x4a, 0x94, 0x46, 0x22, 0x8e, 0x70, 0x66, 0x2b, 0x94, 0xf9, 0xf8,
  0x57, 0x83, 0x2d, 0xa2, 0xff, 0xbc, 0x84, 0xc2
};

// thawte.com's cert (it's EV-licious!).
unsigned char thawte_fingerprint[] = {
  0x85, 0x04, 0x2d, 0xfd, 0x2b, 0x0e, 0xc6, 0xc8, 0xaf, 0x2d, 0x77, 0xd6,
  0xa1, 0x3a, 0x64, 0x04, 0x27, 0x90, 0x97, 0x37
};

// A certificate for www.paypal.com with a NULL byte in the common name.
// From http://www.gossamer-threads.com/lists/fulldisc/full-disclosure/70363
unsigned char paypal_null_fingerprint[] = {
  0x4c, 0x88, 0x9e, 0x28, 0xd7, 0x7a, 0x44, 0x1e, 0x13, 0xf2, 0x6a, 0xba,
  0x1f, 0xe8, 0x1b, 0xd6, 0xab, 0x7b, 0xe8, 0xd7
};

// A certificate for https://www.unosoft.hu/, whose AIA extension contains
// an LDAP URL without a host name.
unsigned char unosoft_hu_fingerprint[] = {
  0x32, 0xff, 0xe3, 0xbe, 0x2c, 0x3b, 0xc7, 0xca, 0xbf, 0x2d, 0x64, 0xbd,
  0x25, 0x66, 0xf2, 0xec, 0x8b, 0x0f, 0xbf, 0xd8
};

// The fingerprint of the Google certificate used in the parsing tests,
// which is newer than the one included in the x509_certificate_data.h
unsigned char google_parse_fingerprint[] = {
  0x40, 0x50, 0x62, 0xe5, 0xbe, 0xfd, 0xe4, 0xaf, 0x97, 0xe9, 0x38, 0x2a,
  0xf1, 0x6c, 0xc8, 0x7c, 0x8f, 0xb7, 0xc4, 0xe2
};

// The fingerprint for the Thawte SGC certificate
unsigned char thawte_parse_fingerprint[] = {
  0xec, 0x07, 0x10, 0x03, 0xd8, 0xf5, 0xa3, 0x7f, 0x42, 0xc4, 0x55, 0x7f,
  0x65, 0x6a, 0xae, 0x86, 0x65, 0xfa, 0x4b, 0x02
};

// Dec 18 00:00:00 2009 GMT
const double kGoogleParseValidFrom = 1261094400;
// Dec 18 23:59:59 2011 GMT
const double kGoogleParseValidTo = 1324252799;

struct CertificateFormatTestData {
  const char* file_name;
  X509Certificate::Format format;
  unsigned char* chain_fingerprints[3];
};

const CertificateFormatTestData FormatTestData[] = {
  // DER Parsing - single certificate, DER encoded
  { "google.single.der", X509Certificate::FORMAT_SINGLE_CERTIFICATE,
    { google_parse_fingerprint,
      NULL, } },
  // DER parsing - single certificate, PEM encoded
  { "google.single.pem", X509Certificate::FORMAT_SINGLE_CERTIFICATE,
    { google_parse_fingerprint,
      NULL, } },
  // PEM parsing - single certificate, PEM encoded with a PEB of
  // "CERTIFICATE"
  { "google.single.pem", X509Certificate::FORMAT_PEM_CERT_SEQUENCE,
    { google_parse_fingerprint,
      NULL, } },
  // PEM parsing - sequence of certificates, PEM encoded with a PEB of
  // "CERTIFICATE"
  { "google.chain.pem", X509Certificate::FORMAT_PEM_CERT_SEQUENCE,
    { google_parse_fingerprint,
      thawte_parse_fingerprint,
      NULL, } },
  // PKCS#7 parsing - "degenerate" SignedData collection of certificates, DER
  // encoding
  { "google.binary.p7b", X509Certificate::FORMAT_PKCS7,
    { google_parse_fingerprint,
      thawte_parse_fingerprint,
      NULL, } },
  // PKCS#7 parsing - "degenerate" SignedData collection of certificates, PEM
  // encoded with a PEM PEB of "CERTIFICATE"
  { "google.pem_cert.p7b", X509Certificate::FORMAT_PKCS7,
    { google_parse_fingerprint,
      thawte_parse_fingerprint,
      NULL, } },
  // PKCS#7 parsing - "degenerate" SignedData collection of certificates, PEM
  // encoded with a PEM PEB of "PKCS7"
  { "google.pem_pkcs7.p7b", X509Certificate::FORMAT_PKCS7,
    { google_parse_fingerprint,
      thawte_parse_fingerprint,
      NULL, } },
  // All of the above, this time using auto-detection
  { "google.single.der", X509Certificate::FORMAT_AUTO,
    { google_parse_fingerprint,
      NULL, } },
  { "google.single.pem", X509Certificate::FORMAT_AUTO,
    { google_parse_fingerprint,
      NULL, } },
  { "google.chain.pem", X509Certificate::FORMAT_AUTO,
    { google_parse_fingerprint,
      thawte_parse_fingerprint,
      NULL, } },
  { "google.binary.p7b", X509Certificate::FORMAT_AUTO,
    { google_parse_fingerprint,
      thawte_parse_fingerprint,
      NULL, } },
  { "google.pem_cert.p7b", X509Certificate::FORMAT_AUTO,
    { google_parse_fingerprint,
      thawte_parse_fingerprint,
      NULL, } },
  { "google.pem_pkcs7.p7b", X509Certificate::FORMAT_AUTO,
    { google_parse_fingerprint,
      thawte_parse_fingerprint,
      NULL, } },
};

CertificateList CreateCertificateListFromFile(
    const FilePath& certs_dir,
    const std::string& cert_file,
    int format) {
  FilePath cert_path = certs_dir.AppendASCII(cert_file);
  std::string cert_data;
  if (!file_util::ReadFileToString(cert_path, &cert_data))
    return CertificateList();
  return X509Certificate::CreateCertificateListFromBytes(cert_data.data(),
                                                         cert_data.size(),
                                                         format);
}

void CheckGoogleCert(const scoped_refptr<X509Certificate>& google_cert,
                     unsigned char* expected_fingerprint,
                     double valid_from, double valid_to) {
  ASSERT_NE(static_cast<X509Certificate*>(NULL), google_cert);

  const CertPrincipal& subject = google_cert->subject();
  EXPECT_EQ("www.google.com", subject.common_name);
  EXPECT_EQ("Mountain View", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  EXPECT_EQ(0U, subject.street_addresses.size());
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Google Inc", subject.organization_names[0]);
  EXPECT_EQ(0U, subject.organization_unit_names.size());
  EXPECT_EQ(0U, subject.domain_components.size());

  const CertPrincipal& issuer = google_cert->issuer();
  EXPECT_EQ("Thawte SGC CA", issuer.common_name);
  EXPECT_EQ("", issuer.locality_name);
  EXPECT_EQ("", issuer.state_or_province_name);
  EXPECT_EQ("ZA", issuer.country_name);
  EXPECT_EQ(0U, issuer.street_addresses.size());
  ASSERT_EQ(1U, issuer.organization_names.size());
  EXPECT_EQ("Thawte Consulting (Pty) Ltd.", issuer.organization_names[0]);
  EXPECT_EQ(0U, issuer.organization_unit_names.size());
  EXPECT_EQ(0U, issuer.domain_components.size());

  // Use DoubleT because its epoch is the same on all platforms
  const Time& valid_start = google_cert->valid_start();
  EXPECT_EQ(valid_from, valid_start.ToDoubleT());

  const Time& valid_expiry = google_cert->valid_expiry();
  EXPECT_EQ(valid_to, valid_expiry.ToDoubleT());

  const SHA1Fingerprint& fingerprint = google_cert->fingerprint();
  for (size_t i = 0; i < 20; ++i)
    EXPECT_EQ(expected_fingerprint[i], fingerprint.data[i]);

  std::vector<std::string> dns_names;
  google_cert->GetDNSNames(&dns_names);
  ASSERT_EQ(1U, dns_names.size());
  EXPECT_EQ("www.google.com", dns_names[0]);

#if TEST_EV
  // TODO(avi): turn this on for the Mac once EV checking is implemented.
  CertVerifyResult verify_result;
  int flags = X509Certificate::VERIFY_REV_CHECKING_ENABLED |
                X509Certificate::VERIFY_EV_CERT;
  EXPECT_EQ(OK, google_cert->Verify("www.google.com", flags, &verify_result));
  EXPECT_EQ(0, verify_result.cert_status & CERT_STATUS_IS_EV);
#endif
}

TEST(X509CertificateTest, GoogleCertParsing) {
  scoped_refptr<X509Certificate> google_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(google_der), sizeof(google_der)));

  CheckGoogleCert(google_cert, google_fingerprint,
                  1238192407,   // Mar 27 22:20:07 2009 GMT
                  1269728407);  // Mar 27 22:20:07 2010 GMT
}

TEST(X509CertificateTest, WebkitCertParsing) {
  scoped_refptr<X509Certificate> webkit_cert(X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der)));

  ASSERT_NE(static_cast<X509Certificate*>(NULL), webkit_cert);

  const CertPrincipal& subject = webkit_cert->subject();
  EXPECT_EQ("Cupertino", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  EXPECT_EQ(0U, subject.street_addresses.size());
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Apple Inc.", subject.organization_names[0]);
  ASSERT_EQ(1U, subject.organization_unit_names.size());
  EXPECT_EQ("Mac OS Forge", subject.organization_unit_names[0]);
  EXPECT_EQ(0U, subject.domain_components.size());

  const CertPrincipal& issuer = webkit_cert->issuer();
  EXPECT_EQ("Go Daddy Secure Certification Authority", issuer.common_name);
  EXPECT_EQ("Scottsdale", issuer.locality_name);
  EXPECT_EQ("Arizona", issuer.state_or_province_name);
  EXPECT_EQ("US", issuer.country_name);
  EXPECT_EQ(0U, issuer.street_addresses.size());
  ASSERT_EQ(1U, issuer.organization_names.size());
  EXPECT_EQ("GoDaddy.com, Inc.", issuer.organization_names[0]);
  ASSERT_EQ(1U, issuer.organization_unit_names.size());
  EXPECT_EQ("http://certificates.godaddy.com/repository",
      issuer.organization_unit_names[0]);
  EXPECT_EQ(0U, issuer.domain_components.size());

  // Use DoubleT because its epoch is the same on all platforms
  const Time& valid_start = webkit_cert->valid_start();
  EXPECT_EQ(1205883319, valid_start.ToDoubleT());  // Mar 18 23:35:19 2008 GMT

  const Time& valid_expiry = webkit_cert->valid_expiry();
  EXPECT_EQ(1300491319, valid_expiry.ToDoubleT());  // Mar 18 23:35:19 2011 GMT

  const SHA1Fingerprint& fingerprint = webkit_cert->fingerprint();
  for (size_t i = 0; i < 20; ++i)
    EXPECT_EQ(webkit_fingerprint[i], fingerprint.data[i]);

  std::vector<std::string> dns_names;
  webkit_cert->GetDNSNames(&dns_names);
  ASSERT_EQ(2U, dns_names.size());
  EXPECT_EQ("*.webkit.org", dns_names[0]);
  EXPECT_EQ("webkit.org", dns_names[1]);

#if TEST_EV
  int flags = X509Certificate::VERIFY_REV_CHECKING_ENABLED |
                X509Certificate::VERIFY_EV_CERT;
  CertVerifyResult verify_result;
  EXPECT_EQ(OK, webkit_cert->Verify("webkit.org", flags, &verify_result));
  EXPECT_EQ(0, verify_result.cert_status & CERT_STATUS_IS_EV);
#endif
}

TEST(X509CertificateTest, ThawteCertParsing) {
  scoped_refptr<X509Certificate> thawte_cert(X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(thawte_der), sizeof(thawte_der)));

  ASSERT_NE(static_cast<X509Certificate*>(NULL), thawte_cert);

  const CertPrincipal& subject = thawte_cert->subject();
  EXPECT_EQ("www.thawte.com", subject.common_name);
  EXPECT_EQ("Mountain View", subject.locality_name);
  EXPECT_EQ("California", subject.state_or_province_name);
  EXPECT_EQ("US", subject.country_name);
  EXPECT_EQ(0U, subject.street_addresses.size());
  ASSERT_EQ(1U, subject.organization_names.size());
  EXPECT_EQ("Thawte Inc", subject.organization_names[0]);
  EXPECT_EQ(0U, subject.organization_unit_names.size());
  EXPECT_EQ(0U, subject.domain_components.size());

  const CertPrincipal& issuer = thawte_cert->issuer();
  EXPECT_EQ("thawte Extended Validation SSL CA", issuer.common_name);
  EXPECT_EQ("", issuer.locality_name);
  EXPECT_EQ("", issuer.state_or_province_name);
  EXPECT_EQ("US", issuer.country_name);
  EXPECT_EQ(0U, issuer.street_addresses.size());
  ASSERT_EQ(1U, issuer.organization_names.size());
  EXPECT_EQ("thawte, Inc.", issuer.organization_names[0]);
  ASSERT_EQ(1U, issuer.organization_unit_names.size());
  EXPECT_EQ("Terms of use at https://www.thawte.com/cps (c)06",
            issuer.organization_unit_names[0]);
  EXPECT_EQ(0U, issuer.domain_components.size());

  // Use DoubleT because its epoch is the same on all platforms
  const Time& valid_start = thawte_cert->valid_start();
  EXPECT_EQ(1227052800, valid_start.ToDoubleT());  // Nov 19 00:00:00 2008 GMT

  const Time& valid_expiry = thawte_cert->valid_expiry();
  EXPECT_EQ(1263772799, valid_expiry.ToDoubleT());  // Jan 17 23:59:59 2010 GMT

  const SHA1Fingerprint& fingerprint = thawte_cert->fingerprint();
  for (size_t i = 0; i < 20; ++i)
    EXPECT_EQ(thawte_fingerprint[i], fingerprint.data[i]);

  std::vector<std::string> dns_names;
  thawte_cert->GetDNSNames(&dns_names);
  ASSERT_EQ(1U, dns_names.size());
  EXPECT_EQ("www.thawte.com", dns_names[0]);

#if TEST_EV
  int flags = X509Certificate::VERIFY_REV_CHECKING_ENABLED |
                X509Certificate::VERIFY_EV_CERT;
  CertVerifyResult verify_result;
  // EV cert verification requires revocation checking.
  EXPECT_EQ(OK, thawte_cert->Verify("www.thawte.com", flags, &verify_result));
  EXPECT_NE(0, verify_result.cert_status & CERT_STATUS_IS_EV);
  // Consequently, if we don't have revocation checking enabled, we can't claim
  // any cert is EV.
  flags = X509Certificate::VERIFY_EV_CERT;
  EXPECT_EQ(OK, thawte_cert->Verify("www.thawte.com", flags, &verify_result));
  EXPECT_EQ(0, verify_result.cert_status & CERT_STATUS_IS_EV);
#endif
}

TEST(X509CertificateTest, PaypalNullCertParsing) {
  scoped_refptr<X509Certificate> paypal_null_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(paypal_null_der),
          sizeof(paypal_null_der)));

  ASSERT_NE(static_cast<X509Certificate*>(NULL), paypal_null_cert);

  const SHA1Fingerprint& fingerprint =
      paypal_null_cert->fingerprint();
  for (size_t i = 0; i < 20; ++i)
    EXPECT_EQ(paypal_null_fingerprint[i], fingerprint.data[i]);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = paypal_null_cert->Verify("www.paypal.com", flags,
                                       &verify_result);
  EXPECT_NE(OK, error);
  // Either the system crypto library should correctly report a certificate
  // name mismatch, or our certificate blacklist should cause us to report an
  // invalid certificate.
#if !defined(OS_MACOSX) && !defined(USE_OPENSSL)
  EXPECT_NE(0, verify_result.cert_status &
            (CERT_STATUS_COMMON_NAME_INVALID | CERT_STATUS_INVALID));
#endif
}

// A certificate whose AIA extension contains an LDAP URL without a host name.
// This certificate will expire on 2011-09-08.
TEST(X509CertificateTest, UnoSoftCertParsing) {
  FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> unosoft_hu_cert(
      ImportCertFromFile(certs_dir, "unosoft_hu_cert.der"));

  ASSERT_NE(static_cast<X509Certificate*>(NULL), unosoft_hu_cert);

  const SHA1Fingerprint& fingerprint =
      unosoft_hu_cert->fingerprint();
  for (size_t i = 0; i < 20; ++i)
    EXPECT_EQ(unosoft_hu_fingerprint[i], fingerprint.data[i]);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = unosoft_hu_cert->Verify("www.unosoft.hu", flags,
                                      &verify_result);
  EXPECT_NE(OK, error);
  EXPECT_NE(0, verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
}

// A regression test for http://crbug.com/31497.
// This certificate will expire on 2012-04-08.
TEST(X509CertificateTest, IntermediateCARequireExplicitPolicy) {
  FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "www_us_army_mil_cert.der");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), server_cert);

  // The intermediate CA certificate's policyConstraints extension has a
  // requireExplicitPolicy field with SkipCerts=0.
  scoped_refptr<X509Certificate> intermediate_cert =
      ImportCertFromFile(certs_dir, "dod_ca_17_cert.der");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate_cert);

  FilePath root_cert_path = certs_dir.AppendASCII("dod_root_ca_2_cert.der");
  TestRootCerts* root_certs = TestRootCerts::GetInstance();
  ASSERT_TRUE(root_certs->AddFromFile(root_cert_path));

  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(intermediate_cert->os_cert_handle());
  scoped_refptr<X509Certificate> cert_chain =
      X509Certificate::CreateFromHandle(server_cert->os_cert_handle(),
                                        X509Certificate::SOURCE_FROM_NETWORK,
                                        intermediates);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = cert_chain->Verify("www.us.army.mil", flags, &verify_result);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(0, verify_result.cert_status);
  root_certs->Clear();
}

// Tests X509Certificate::Cache via X509Certificate::CreateFromHandle.  We
// call X509Certificate::CreateFromHandle several times and observe whether
// it returns a cached or new X509Certificate object.
//
// All the OS certificate handles in this test are actually from the same
// source (the bytes of a lone certificate), but we pretend that some of them
// come from the network.
TEST(X509CertificateTest, Cache) {
  X509Certificate::OSCertHandle google_cert_handle;

  // Add a certificate from the source SOURCE_LONE_CERT_IMPORT to our
  // certificate cache.
  google_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  scoped_refptr<X509Certificate> cert1(X509Certificate::CreateFromHandle(
      google_cert_handle, X509Certificate::SOURCE_LONE_CERT_IMPORT,
      X509Certificate::OSCertHandles()));
  X509Certificate::FreeOSCertHandle(google_cert_handle);

  // Add a certificate from the same source (SOURCE_LONE_CERT_IMPORT).  This
  // should return the cached certificate (cert1).
  google_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  scoped_refptr<X509Certificate> cert2(X509Certificate::CreateFromHandle(
      google_cert_handle, X509Certificate::SOURCE_LONE_CERT_IMPORT,
      X509Certificate::OSCertHandles()));
  X509Certificate::FreeOSCertHandle(google_cert_handle);

  EXPECT_EQ(cert1, cert2);

  // Add a certificate from the network.  This should kick out the original
  // cached certificate (cert1) and return a new certificate.
  google_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  scoped_refptr<X509Certificate> cert3(X509Certificate::CreateFromHandle(
      google_cert_handle, X509Certificate::SOURCE_FROM_NETWORK,
      X509Certificate::OSCertHandles()));
  X509Certificate::FreeOSCertHandle(google_cert_handle);

  EXPECT_NE(cert1, cert3);

  // Add one certificate from each source.  Both should return the new cached
  // certificate (cert3).
  google_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  scoped_refptr<X509Certificate> cert4(X509Certificate::CreateFromHandle(
      google_cert_handle, X509Certificate::SOURCE_FROM_NETWORK,
      X509Certificate::OSCertHandles()));
  X509Certificate::FreeOSCertHandle(google_cert_handle);

  EXPECT_EQ(cert3, cert4);

  google_cert_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  scoped_refptr<X509Certificate> cert5(X509Certificate::CreateFromHandle(
      google_cert_handle, X509Certificate::SOURCE_FROM_NETWORK,
      X509Certificate::OSCertHandles()));
  X509Certificate::FreeOSCertHandle(google_cert_handle);

  EXPECT_EQ(cert3, cert5);
}

TEST(X509CertificateTest, Pickle) {
  scoped_refptr<X509Certificate> cert1(X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der)));

  Pickle pickle;
  cert1->Persist(&pickle);

  void* iter = NULL;
  scoped_refptr<X509Certificate> cert2(
      X509Certificate::CreateFromPickle(pickle, &iter));

  EXPECT_EQ(cert1, cert2);
}

TEST(X509CertificateTest, Policy) {
  scoped_refptr<X509Certificate> google_cert(X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der)));

  scoped_refptr<X509Certificate> webkit_cert(X509Certificate::CreateFromBytes(
      reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der)));

  CertPolicy policy;

  EXPECT_EQ(policy.Check(google_cert.get()), CertPolicy::UNKNOWN);
  EXPECT_EQ(policy.Check(webkit_cert.get()), CertPolicy::UNKNOWN);
  EXPECT_FALSE(policy.HasAllowedCert());
  EXPECT_FALSE(policy.HasDeniedCert());

  policy.Allow(google_cert.get());

  EXPECT_EQ(policy.Check(google_cert.get()), CertPolicy::ALLOWED);
  EXPECT_EQ(policy.Check(webkit_cert.get()), CertPolicy::UNKNOWN);
  EXPECT_TRUE(policy.HasAllowedCert());
  EXPECT_FALSE(policy.HasDeniedCert());

  policy.Deny(google_cert.get());

  EXPECT_EQ(policy.Check(google_cert.get()), CertPolicy::DENIED);
  EXPECT_EQ(policy.Check(webkit_cert.get()), CertPolicy::UNKNOWN);
  EXPECT_FALSE(policy.HasAllowedCert());
  EXPECT_TRUE(policy.HasDeniedCert());

  policy.Allow(webkit_cert.get());

  EXPECT_EQ(policy.Check(google_cert.get()), CertPolicy::DENIED);
  EXPECT_EQ(policy.Check(webkit_cert.get()), CertPolicy::ALLOWED);
  EXPECT_TRUE(policy.HasAllowedCert());
  EXPECT_TRUE(policy.HasDeniedCert());
}

#if defined(OS_MACOSX) || defined(OS_WIN)
TEST(X509CertificateTest, IntermediateCertificates) {
  scoped_refptr<X509Certificate> webkit_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der)));

  scoped_refptr<X509Certificate> thawte_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(thawte_der), sizeof(thawte_der)));

  scoped_refptr<X509Certificate> paypal_cert(
      X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(paypal_null_der),
          sizeof(paypal_null_der)));

  X509Certificate::OSCertHandle google_handle;
  // Create object with no intermediates:
  google_handle = X509Certificate::CreateOSCertHandleFromBytes(
      reinterpret_cast<const char*>(google_der), sizeof(google_der));
  X509Certificate::OSCertHandles intermediates1;
  scoped_refptr<X509Certificate> cert1;
  cert1 = X509Certificate::CreateFromHandle(
      google_handle, X509Certificate::SOURCE_FROM_NETWORK, intermediates1);
  EXPECT_TRUE(cert1->HasIntermediateCertificates(intermediates1));
  EXPECT_FALSE(cert1->HasIntermediateCertificate(
      webkit_cert->os_cert_handle()));

  // Create object with 2 intermediates:
  X509Certificate::OSCertHandles intermediates2;
  intermediates2.push_back(webkit_cert->os_cert_handle());
  intermediates2.push_back(thawte_cert->os_cert_handle());
  scoped_refptr<X509Certificate> cert2;
  cert2 = X509Certificate::CreateFromHandle(
      google_handle, X509Certificate::SOURCE_FROM_NETWORK, intermediates2);

  // The cache should have stored cert2 'cause it has more intermediates:
  EXPECT_NE(cert1, cert2);

  // Verify it has all the intermediates:
  EXPECT_TRUE(cert2->HasIntermediateCertificate(
      webkit_cert->os_cert_handle()));
  EXPECT_TRUE(cert2->HasIntermediateCertificate(
      thawte_cert->os_cert_handle()));
  EXPECT_FALSE(cert2->HasIntermediateCertificate(
      paypal_cert->os_cert_handle()));

  // Create object with 1 intermediate:
  X509Certificate::OSCertHandles intermediates3;
  intermediates2.push_back(thawte_cert->os_cert_handle());
  scoped_refptr<X509Certificate> cert3;
  cert3 = X509Certificate::CreateFromHandle(
      google_handle, X509Certificate::SOURCE_FROM_NETWORK, intermediates3);

  // The cache should have returned cert2 'cause it has more intermediates:
  EXPECT_EQ(cert3, cert2);

  // Cleanup
  X509Certificate::FreeOSCertHandle(google_handle);
}
#endif

#if defined(OS_MACOSX)
TEST(X509CertificateTest, IsIssuedBy) {
  FilePath certs_dir = GetTestCertsDirectory();

  // Test a client certificate from MIT.
  scoped_refptr<X509Certificate> mit_davidben_cert(
      ImportCertFromFile(certs_dir, "mit.davidben.der"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), mit_davidben_cert);

  CertPrincipal mit_issuer;
  mit_issuer.country_name = "US";
  mit_issuer.state_or_province_name = "Massachusetts";
  mit_issuer.organization_names.push_back(
      "Massachusetts Institute of Technology");
  mit_issuer.organization_unit_names.push_back("Client CA v1");

  // IsIssuedBy should return true even if it cannot build a chain
  // with that principal.
  std::vector<CertPrincipal> mit_issuers(1, mit_issuer);
  EXPECT_TRUE(mit_davidben_cert->IsIssuedBy(mit_issuers));

  // Test a client certificate from FOAF.ME.
  scoped_refptr<X509Certificate> foaf_me_chromium_test_cert(
      ImportCertFromFile(certs_dir, "foaf.me.chromium-test-cert.der"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), foaf_me_chromium_test_cert);

  CertPrincipal foaf_issuer;
  foaf_issuer.common_name = "FOAF.ME";
  foaf_issuer.locality_name = "Wimbledon";
  foaf_issuer.state_or_province_name = "LONDON";
  foaf_issuer.country_name = "GB";
  foaf_issuer.organization_names.push_back("FOAF.ME");

  std::vector<CertPrincipal> foaf_issuers(1, foaf_issuer);
  EXPECT_TRUE(foaf_me_chromium_test_cert->IsIssuedBy(foaf_issuers));

  // And test some combinations and mismatches.
  std::vector<CertPrincipal> both_issuers;
  both_issuers.push_back(mit_issuer);
  both_issuers.push_back(foaf_issuer);
  EXPECT_TRUE(foaf_me_chromium_test_cert->IsIssuedBy(both_issuers));
  EXPECT_TRUE(mit_davidben_cert->IsIssuedBy(both_issuers));
  EXPECT_FALSE(foaf_me_chromium_test_cert->IsIssuedBy(mit_issuers));
  EXPECT_FALSE(mit_davidben_cert->IsIssuedBy(foaf_issuers));
}
#endif  // defined(OS_MACOSX)

#if defined(USE_NSS) || defined(OS_WIN) || defined(OS_MACOSX)
// This test creates a self-signed cert from a private key and then verify the
// content of the certificate.
TEST(X509CertificateTest, CreateSelfSigned) {
  scoped_ptr<base::RSAPrivateKey> private_key(
      base::RSAPrivateKey::Create(1024));
  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateSelfSigned(
          private_key.get(), "CN=subject", 1, base::TimeDelta::FromDays(1));

  EXPECT_EQ("subject", cert->subject().GetDisplayName());
  EXPECT_FALSE(cert->HasExpired());

  const uint8 private_key_info[] = {
    0x30, 0x82, 0x02, 0x78, 0x02, 0x01, 0x00, 0x30,
    0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7,
    0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x04, 0x82,
    0x02, 0x62, 0x30, 0x82, 0x02, 0x5e, 0x02, 0x01,
    0x00, 0x02, 0x81, 0x81, 0x00, 0xb8, 0x7f, 0x2b,
    0x20, 0xdc, 0x7c, 0x9b, 0x0c, 0xdc, 0x51, 0x61,
    0x99, 0x0d, 0x36, 0x0f, 0xd4, 0x66, 0x88, 0x08,
    0x55, 0x84, 0xd5, 0x3a, 0xbf, 0x2b, 0xa4, 0x64,
    0x85, 0x7b, 0x0c, 0x04, 0x13, 0x3f, 0x8d, 0xf4,
    0xbc, 0x38, 0x0d, 0x49, 0xfe, 0x6b, 0xc4, 0x5a,
    0xb0, 0x40, 0x53, 0x3a, 0xd7, 0x66, 0x09, 0x0f,
    0x9e, 0x36, 0x74, 0x30, 0xda, 0x8a, 0x31, 0x4f,
    0x1f, 0x14, 0x50, 0xd7, 0xc7, 0x20, 0x94, 0x17,
    0xde, 0x4e, 0xb9, 0x57, 0x5e, 0x7e, 0x0a, 0xe5,
    0xb2, 0x65, 0x7a, 0x89, 0x4e, 0xb6, 0x47, 0xff,
    0x1c, 0xbd, 0xb7, 0x38, 0x13, 0xaf, 0x47, 0x85,
    0x84, 0x32, 0x33, 0xf3, 0x17, 0x49, 0xbf, 0xe9,
    0x96, 0xd0, 0xd6, 0x14, 0x6f, 0x13, 0x8d, 0xc5,
    0xfc, 0x2c, 0x72, 0xba, 0xac, 0xea, 0x7e, 0x18,
    0x53, 0x56, 0xa6, 0x83, 0xa2, 0xce, 0x93, 0x93,
    0xe7, 0x1f, 0x0f, 0xe6, 0x0f, 0x02, 0x03, 0x01,
    0x00, 0x01, 0x02, 0x81, 0x80, 0x03, 0x61, 0x89,
    0x37, 0xcb, 0xf2, 0x98, 0xa0, 0xce, 0xb4, 0xcb,
    0x16, 0x13, 0xf0, 0xe6, 0xaf, 0x5c, 0xc5, 0xa7,
    0x69, 0x71, 0xca, 0xba, 0x8d, 0xe0, 0x4d, 0xdd,
    0xed, 0xb8, 0x48, 0x8b, 0x16, 0x93, 0x36, 0x95,
    0xc2, 0x91, 0x40, 0x65, 0x17, 0xbd, 0x7f, 0xd6,
    0xad, 0x9e, 0x30, 0x28, 0x46, 0xe4, 0x3e, 0xcc,
    0x43, 0x78, 0xf9, 0xfe, 0x1f, 0x33, 0x23, 0x1e,
    0x31, 0x12, 0x9d, 0x3c, 0xa7, 0x08, 0x82, 0x7b,
    0x7d, 0x25, 0x4e, 0x5e, 0x19, 0xa8, 0x9b, 0xed,
    0x86, 0xb2, 0xcb, 0x3c, 0xfe, 0x4e, 0xa1, 0xfa,
    0x62, 0x87, 0x3a, 0x17, 0xf7, 0x60, 0xec, 0x38,
    0x29, 0xe8, 0x4f, 0x34, 0x9f, 0x76, 0x9d, 0xee,
    0xa3, 0xf6, 0x85, 0x6b, 0x84, 0x43, 0xc9, 0x1e,
    0x01, 0xff, 0xfd, 0xd0, 0x29, 0x4c, 0xfa, 0x8e,
    0x57, 0x0c, 0xc0, 0x71, 0xa5, 0xbb, 0x88, 0x46,
    0x29, 0x5c, 0xc0, 0x4f, 0x01, 0x02, 0x41, 0x00,
    0xf5, 0x83, 0xa4, 0x64, 0x4a, 0xf2, 0xdd, 0x8c,
    0x2c, 0xed, 0xa8, 0xd5, 0x60, 0x5a, 0xe4, 0xc7,
    0xcc, 0x61, 0xcd, 0x38, 0x42, 0x20, 0xd3, 0x82,
    0x18, 0xf2, 0x35, 0x00, 0x72, 0x2d, 0xf7, 0x89,
    0x80, 0x67, 0xb5, 0x93, 0x05, 0x5f, 0xdd, 0x42,
    0xba, 0x16, 0x1a, 0xea, 0x15, 0xc6, 0xf0, 0xb8,
    0x8c, 0xbc, 0xbf, 0x54, 0x9e, 0xf1, 0xc1, 0xb2,
    0xb3, 0x8b, 0xb6, 0x26, 0x02, 0x30, 0xc4, 0x81,
    0x02, 0x41, 0x00, 0xc0, 0x60, 0x62, 0x80, 0xe1,
    0x22, 0x78, 0xf6, 0x9d, 0x83, 0x18, 0xeb, 0x72,
    0x45, 0xd7, 0xc8, 0x01, 0x7f, 0xa9, 0xca, 0x8f,
    0x7d, 0xd6, 0xb8, 0x31, 0x2b, 0x84, 0x7f, 0x62,
    0xd9, 0xa9, 0x22, 0x17, 0x7d, 0x06, 0x35, 0x6c,
    0xf3, 0xc1, 0x94, 0x17, 0x85, 0x5a, 0xaf, 0x9c,
    0x5c, 0x09, 0x3c, 0xcf, 0x2f, 0x44, 0x9d, 0xb6,
    0x52, 0x68, 0x5f, 0xf9, 0x59, 0xc8, 0x84, 0x2b,
    0x39, 0x22, 0x8f, 0x02, 0x41, 0x00, 0xb2, 0x04,
    0xe2, 0x0e, 0x56, 0xca, 0x03, 0x1a, 0xc0, 0xf9,
    0x12, 0x92, 0xa5, 0x6b, 0x42, 0xb8, 0x1c, 0xda,
    0x4d, 0x93, 0x9d, 0x5f, 0x6f, 0xfd, 0xc5, 0x58,
    0xda, 0x55, 0x98, 0x74, 0xfc, 0x28, 0x17, 0x93,
    0x1b, 0x75, 0x9f, 0x50, 0x03, 0x7f, 0x7e, 0xae,
    0xc8, 0x95, 0x33, 0x75, 0x2c, 0xd6, 0xa4, 0x35,
    0xb8, 0x06, 0x03, 0xba, 0x08, 0x59, 0x2b, 0x17,
    0x02, 0xdc, 0x4c, 0x7a, 0x50, 0x01, 0x02, 0x41,
    0x00, 0x9d, 0xdb, 0x39, 0x59, 0x09, 0xe4, 0x30,
    0xa0, 0x24, 0xf5, 0xdb, 0x2f, 0xf0, 0x2f, 0xf1,
    0x75, 0x74, 0x0d, 0x5e, 0xb5, 0x11, 0x73, 0xb0,
    0x0a, 0xaa, 0x86, 0x4c, 0x0d, 0xff, 0x7e, 0x1d,
    0xb4, 0x14, 0xd4, 0x09, 0x91, 0x33, 0x5a, 0xfd,
    0xa0, 0x58, 0x80, 0x9b, 0xbe, 0x78, 0x2e, 0x69,
    0x82, 0x15, 0x7c, 0x72, 0xf0, 0x7b, 0x18, 0x39,
    0xff, 0x6e, 0xeb, 0xc6, 0x86, 0xf5, 0xb4, 0xc7,
    0x6f, 0x02, 0x41, 0x00, 0x8d, 0x1a, 0x37, 0x0f,
    0x76, 0xc4, 0x82, 0xfa, 0x5c, 0xc3, 0x79, 0x35,
    0x3e, 0x70, 0x8a, 0xbf, 0x27, 0x49, 0xb0, 0x99,
    0x63, 0xcb, 0x77, 0x5f, 0xa8, 0x82, 0x65, 0xf6,
    0x03, 0x52, 0x51, 0xf1, 0xae, 0x2e, 0x05, 0xb3,
    0xc6, 0xa4, 0x92, 0xd1, 0xce, 0x6c, 0x72, 0xfb,
    0x21, 0xb3, 0x02, 0x87, 0xe4, 0xfd, 0x61, 0xca,
    0x00, 0x42, 0x19, 0xf0, 0xda, 0x5a, 0x53, 0xe3,
    0xb1, 0xc5, 0x15, 0xf3
  };

  std::vector<uint8> input;
  input.resize(sizeof(private_key_info));
  memcpy(&input.front(), private_key_info, sizeof(private_key_info));

  private_key.reset(base::RSAPrivateKey::CreateFromPrivateKeyInfo(input));
  ASSERT_TRUE(private_key.get());

  cert = net::X509Certificate::CreateSelfSigned(
      private_key.get(), "CN=subject", 1, base::TimeDelta::FromDays(1));

  EXPECT_EQ("subject", cert->subject().GetDisplayName());
  EXPECT_FALSE(cert->HasExpired());
}

TEST(X509CertificateTest, GetDEREncoded) {
  scoped_ptr<base::RSAPrivateKey> private_key(
      base::RSAPrivateKey::Create(1024));
  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateSelfSigned(
          private_key.get(), "CN=subject", 0, base::TimeDelta::FromDays(1));

  std::string der_cert;
  EXPECT_TRUE(cert->GetDEREncoded(&der_cert));
  EXPECT_FALSE(der_cert.empty());
}
#endif

class X509CertificateParseTest
    : public testing::TestWithParam<CertificateFormatTestData> {
 public:
  virtual ~X509CertificateParseTest() {}
  virtual void SetUp() {
    test_data_ = GetParam();
  }
  virtual void TearDown() {}

 protected:
  CertificateFormatTestData test_data_;
};

TEST_P(X509CertificateParseTest, CanParseFormat) {
  FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, test_data_.file_name, test_data_.format);
  ASSERT_FALSE(certs.empty());
  ASSERT_LE(certs.size(), arraysize(test_data_.chain_fingerprints));
  CheckGoogleCert(certs.front(), google_parse_fingerprint,
                  kGoogleParseValidFrom, kGoogleParseValidTo);

  size_t i;
  for (i = 0; i < arraysize(test_data_.chain_fingerprints); ++i) {
    if (test_data_.chain_fingerprints[i] == NULL) {
      // No more test certificates expected - make sure no more were
      // returned before marking this test a success.
      EXPECT_EQ(i, certs.size());
      break;
    }

    // A cert is expected - make sure that one was parsed.
    ASSERT_LT(i, certs.size());

    // Compare the parsed certificate with the expected certificate, by
    // comparing fingerprints.
    const X509Certificate* cert = certs[i];
    const SHA1Fingerprint& actual_fingerprint = cert->fingerprint();
    unsigned char* expected_fingerprint = test_data_.chain_fingerprints[i];

    for (size_t j = 0; j < 20; ++j)
      EXPECT_EQ(expected_fingerprint[j], actual_fingerprint.data[j]);
  }
}

INSTANTIATE_TEST_CASE_P(, X509CertificateParseTest,
                        testing::ValuesIn(FormatTestData));

}  // namespace net
