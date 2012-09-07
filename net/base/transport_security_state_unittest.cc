// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/transport_security_state.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/file_path.h"
#include "base/sha1.h"
#include "base/string_piece.h"
#include "crypto/sha2.h"
#include "net/base/asn1_util.h"
#include "net/base/cert_test_util.h"
#include "net/base/cert_verifier.h"
#include "net/base/cert_verify_result.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/ssl_info.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_root_certs.h"
#include "net/base/x509_cert_types.h"
#include "net/base/x509_certificate.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_OPENSSL)
#include "crypto/openssl_util.h"
#else
#include "crypto/nss_util.h"
#endif

namespace net {

class TransportSecurityStateTest : public testing::Test {
  virtual void SetUp() {
#if defined(USE_OPENSSL)
    crypto::EnsureOpenSSLInit();
#else
    crypto::EnsureNSSInit();
#endif
  }
};

TEST_F(TransportSecurityStateTest, BogusHeaders) {
  TransportSecurityState::DomainState state;
  base::Time now = base::Time::Now();

  EXPECT_FALSE(state.ParseSTSHeader(now, ""));
  EXPECT_FALSE(state.ParseSTSHeader(now, "    "));
  EXPECT_FALSE(state.ParseSTSHeader(now, "abc"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "  abc"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "  abc   "));
  EXPECT_FALSE(state.ParseSTSHeader(now, "max-age"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "  max-age"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "  max-age  "));
  EXPECT_FALSE(state.ParseSTSHeader(now, "max-age="));
  EXPECT_FALSE(state.ParseSTSHeader(now, "   max-age="));
  EXPECT_FALSE(state.ParseSTSHeader(now, "   max-age  ="));
  EXPECT_FALSE(state.ParseSTSHeader(now, "   max-age=   "));
  EXPECT_FALSE(state.ParseSTSHeader(now, "   max-age  =     "));
  EXPECT_FALSE(state.ParseSTSHeader(now, "   max-age  =     xy"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "   max-age  =     3488a923"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "max-age=3488a923  "));
  EXPECT_FALSE(state.ParseSTSHeader(now, "max-ag=3488923"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "max-aged=3488923"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "max-age==3488923"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "amax-age=3488923"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "max-age=-3488923"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "max-age=3488923;"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "max-age=3488923     e"));
  EXPECT_FALSE(state.ParseSTSHeader(
      now, "max-age=3488923     includesubdomain"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "max-age=3488923includesubdomains"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "max-age=3488923=includesubdomains"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "max-age=3488923 includesubdomainx"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "max-age=3488923 includesubdomain="));
  EXPECT_FALSE(state.ParseSTSHeader(
      now, "max-age=3488923 includesubdomain=true"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "max-age=3488923 includesubdomainsx"));
  EXPECT_FALSE(state.ParseSTSHeader(
      now, "max-age=3488923 includesubdomains x"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "max-age=34889.23 includesubdomains"));
  EXPECT_FALSE(state.ParseSTSHeader(now, "max-age=34889 includesubdomains"));

  // Check that |state| was not updated by expecting the default
  // values for its predictable fields.
  EXPECT_EQ(state.upgrade_mode,
            TransportSecurityState::DomainState::MODE_FORCE_HTTPS);
  EXPECT_FALSE(state.include_subdomains);
}

static bool GetPublicKeyHash(const net::X509Certificate::OSCertHandle& cert,
                             HashValue* hash) {
  std::string der_bytes;
  if (!net::X509Certificate::GetDEREncoded(cert, &der_bytes))
    return false;

  base::StringPiece spki;
  if (!asn1::ExtractSPKIFromDERCert(der_bytes, &spki))
    return false;

  switch (hash->tag) {
    case HASH_VALUE_SHA1:
      base::SHA1HashBytes(reinterpret_cast<const unsigned char*>(spki.data()),
                          spki.size(), hash->data());
      break;
    case HASH_VALUE_SHA256:
      crypto::SHA256HashString(spki, hash->data(), crypto::kSHA256Length);
      break;
    default:
      NOTREACHED() << "Unknown HashValueTag " << hash->tag;
  }

  return true;
}

static std::string GetPinFromCert(X509Certificate* cert, HashValueTag tag) {
  HashValue spki_hash(tag);
  EXPECT_TRUE(GetPublicKeyHash(cert->os_cert_handle(), &spki_hash));

  std::string base64;
  base::Base64Encode(base::StringPiece(
      reinterpret_cast<char*>(spki_hash.data()), spki_hash.size()), &base64);

  std::string label;
  switch (tag) {
    case HASH_VALUE_SHA1:
      label = "pin-sha1=";
      break;
    case HASH_VALUE_SHA256:
      label = "pin-sha256=";
      break;
    default:
      NOTREACHED() << "Unknown HashValueTag " << tag;
  }

  return label + HttpUtil::Quote(base64);
}

static void TestBogusPinsHeaders(HashValueTag tag) {
  TransportSecurityState::DomainState state;
  SSLInfo ssl_info;
  ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "test_mail_google_com.pem");
  std::string good_pin = GetPinFromCert(ssl_info.cert, tag);
  base::Time now = base::Time::Now();

  // The backup pin is fake --- it just has to not be in the chain.
  std::string backup_pin = "pin-sha1=" +
      HttpUtil::Quote("6dcfXufJLW3J6S/9rRe4vUlBj5g=");

  EXPECT_FALSE(state.ParsePinsHeader(now, "", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "    ", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "abc", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "  abc", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "  abc   ", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "max-age", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "  max-age", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "  max-age  ", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "max-age=", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "   max-age=", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "   max-age  =", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "   max-age=   ", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "   max-age  =     ", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "   max-age  =     xy", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(
      now,
      "   max-age  =     3488a923",
      ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "max-age=3488a923  ", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now,
      "max-ag=3488923pins=" + good_pin + "," + backup_pin,
      ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "max-aged=3488923" + backup_pin,
                                     ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "max-aged=3488923; " + backup_pin,
                                     ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now,
      "max-aged=3488923; " + backup_pin + ";" + backup_pin,
      ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now,
      "max-aged=3488923; " + good_pin + ";" + good_pin,
      ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "max-aged=3488923; " + good_pin,
                                     ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "max-age==3488923", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "amax-age=3488923", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "max-age=-3488923", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "max-age=3488923;", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "max-age=3488923     e", ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(
      now,
      "max-age=3488923     includesubdomain",
      ssl_info));
  EXPECT_FALSE(state.ParsePinsHeader(now, "max-age=34889.23", ssl_info));

  // Check that |state| was not updated by expecting the default
  // values for its predictable fields.
  EXPECT_EQ(state.upgrade_mode,
            TransportSecurityState::DomainState::MODE_FORCE_HTTPS);
  EXPECT_FALSE(state.include_subdomains);
}

TEST_F(TransportSecurityStateTest, BogusPinsHeadersSHA1) {
  TestBogusPinsHeaders(HASH_VALUE_SHA1);
}

TEST_F(TransportSecurityStateTest, BogusPinsHeadersSHA256) {
  TestBogusPinsHeaders(HASH_VALUE_SHA256);
}

TEST_F(TransportSecurityStateTest, ValidSTSHeaders) {
  TransportSecurityState::DomainState state;
  base::Time expiry;
  base::Time now = base::Time::Now();

  EXPECT_TRUE(state.ParseSTSHeader(now, "max-age=243"));
  expiry = now + base::TimeDelta::FromSeconds(243);
  EXPECT_EQ(expiry, state.upgrade_expiry);
  EXPECT_FALSE(state.include_subdomains);

  EXPECT_TRUE(state.ParseSTSHeader(now, "  Max-agE    = 567"));
  expiry = now + base::TimeDelta::FromSeconds(567);
  EXPECT_EQ(expiry, state.upgrade_expiry);
  EXPECT_FALSE(state.include_subdomains);

  EXPECT_TRUE(state.ParseSTSHeader(now, "  mAx-aGe    = 890      "));
  expiry = now + base::TimeDelta::FromSeconds(890);
  EXPECT_EQ(expiry, state.upgrade_expiry);
  EXPECT_FALSE(state.include_subdomains);

  EXPECT_TRUE(state.ParseSTSHeader(now, "max-age=123;incLudesUbdOmains"));
  expiry = now + base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expiry, state.upgrade_expiry);
  EXPECT_TRUE(state.include_subdomains);

  EXPECT_TRUE(state.ParseSTSHeader(now, "max-age=394082;  incLudesUbdOmains"));
  expiry = now + base::TimeDelta::FromSeconds(394082);
  EXPECT_EQ(expiry, state.upgrade_expiry);
  EXPECT_TRUE(state.include_subdomains);

  EXPECT_TRUE(state.ParseSTSHeader(
      now, "max-age=39408299  ;incLudesUbdOmains"));
  expiry = now + base::TimeDelta::FromSeconds(
      std::min(TransportSecurityState::kMaxHSTSAgeSecs, 39408299l));
  EXPECT_EQ(expiry, state.upgrade_expiry);
  EXPECT_TRUE(state.include_subdomains);

  EXPECT_TRUE(state.ParseSTSHeader(
      now, "max-age=394082038  ; incLudesUbdOmains"));
  expiry = now + base::TimeDelta::FromSeconds(
      std::min(TransportSecurityState::kMaxHSTSAgeSecs, 394082038l));
  EXPECT_EQ(expiry, state.upgrade_expiry);
  EXPECT_TRUE(state.include_subdomains);

  EXPECT_TRUE(state.ParseSTSHeader(
      now, "  max-age=0  ;  incLudesUbdOmains   "));
  expiry = now + base::TimeDelta::FromSeconds(0);
  EXPECT_EQ(expiry, state.upgrade_expiry);
  EXPECT_TRUE(state.include_subdomains);

  EXPECT_TRUE(state.ParseSTSHeader(
      now,
      "  max-age=999999999999999999999999999999999999999999999  ;"
      "  incLudesUbdOmains   "));
  expiry = now + base::TimeDelta::FromSeconds(
      TransportSecurityState::kMaxHSTSAgeSecs);
  EXPECT_EQ(expiry, state.upgrade_expiry);
  EXPECT_TRUE(state.include_subdomains);
}

static void TestValidPinsHeaders(HashValueTag tag) {
  TransportSecurityState::DomainState state;
  base::Time expiry;
  base::Time now = base::Time::Now();

  // Set up a realistic SSLInfo with a realistic cert chain.
  FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> ee_cert =
      ImportCertFromFile(certs_dir, "2048-rsa-ee-by-2048-rsa-intermediate.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), ee_cert);
  scoped_refptr<X509Certificate> intermediate =
      ImportCertFromFile(certs_dir, "2048-rsa-intermediate.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), intermediate);
  X509Certificate::OSCertHandles intermediates;
  intermediates.push_back(intermediate->os_cert_handle());
  SSLInfo ssl_info;
  ssl_info.cert = X509Certificate::CreateFromHandle(ee_cert->os_cert_handle(),
                                                    intermediates);

  // Add the root that signed the intermediate for this test.
  scoped_refptr<X509Certificate> root_cert =
      ImportCertFromFile(certs_dir, "2048-rsa-root.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), root_cert);
  ScopedTestRoot scoped_root(root_cert);

  // Verify has the side-effect of populating public_key_hashes, which
  // ParsePinsHeader needs. (It wants to check pins against the validated
  // chain, not just the presented chain.)
  int rv = ERR_FAILED;
  CertVerifyResult result;
  scoped_ptr<CertVerifier> verifier(CertVerifier::CreateDefault());
  TestCompletionCallback callback;
  CertVerifier::RequestHandle handle = NULL;
  rv = verifier->Verify(ssl_info.cert, "127.0.0.1", 0, NULL, &result,
                        callback.callback(), &handle, BoundNetLog());
  rv = callback.GetResult(rv);
  ASSERT_EQ(OK, rv);
  // Normally, ssl_client_socket_nss would do this, but for a unit test we
  // fake it.
  ssl_info.public_key_hashes = result.public_key_hashes;
  std::string good_pin = GetPinFromCert(ssl_info.cert, /*tag*/HASH_VALUE_SHA1);
  DLOG(WARNING) << "good pin: " << good_pin;

  // The backup pin is fake --- we just need an SPKI hash that does not match
  // the hash of any SPKI in the certificate chain.
  std::string backup_pin = "pin-sha1=" +
      HttpUtil::Quote("6dcfXufJLW3J6S/9rRe4vUlBj5g=");

  EXPECT_TRUE(state.ParsePinsHeader(
      now,
      "max-age=243; " + good_pin + ";" + backup_pin,
      ssl_info));
  expiry = now + base::TimeDelta::FromSeconds(243);
  EXPECT_EQ(expiry, state.dynamic_spki_hashes_expiry);

  EXPECT_TRUE(state.ParsePinsHeader(
      now,
      "   " + good_pin + "; " + backup_pin + "  ; Max-agE    = 567",
      ssl_info));
  expiry = now + base::TimeDelta::FromSeconds(567);
  EXPECT_EQ(expiry, state.dynamic_spki_hashes_expiry);

  EXPECT_TRUE(state.ParsePinsHeader(
      now,
      good_pin + ";" + backup_pin + "  ; mAx-aGe    = 890      ",
      ssl_info));
  expiry = now + base::TimeDelta::FromSeconds(890);
  EXPECT_EQ(expiry, state.dynamic_spki_hashes_expiry);

  EXPECT_TRUE(state.ParsePinsHeader(
      now,
      good_pin + ";" + backup_pin + "; max-age=123;IGNORED;",
      ssl_info));
  expiry = now + base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expiry, state.dynamic_spki_hashes_expiry);

  EXPECT_TRUE(state.ParsePinsHeader(
      now,
      "max-age=394082;" + backup_pin + ";" + good_pin + ";  ",
      ssl_info));
  expiry = now + base::TimeDelta::FromSeconds(394082);
  EXPECT_EQ(expiry, state.dynamic_spki_hashes_expiry);

  EXPECT_TRUE(state.ParsePinsHeader(
      now,
      "max-age=39408299  ;" + backup_pin + ";" + good_pin + ";  ",
      ssl_info));
  expiry = now + base::TimeDelta::FromSeconds(
      std::min(TransportSecurityState::kMaxHSTSAgeSecs, 39408299l));
  EXPECT_EQ(expiry, state.dynamic_spki_hashes_expiry);

  EXPECT_TRUE(state.ParsePinsHeader(
      now,
      "max-age=39408038  ;    cybers=39408038  ;  " +
          good_pin + ";" + backup_pin + ";   ",
      ssl_info));
  expiry = now + base::TimeDelta::FromSeconds(
      std::min(TransportSecurityState::kMaxHSTSAgeSecs, 394082038l));
  EXPECT_EQ(expiry, state.dynamic_spki_hashes_expiry);

  EXPECT_TRUE(state.ParsePinsHeader(
      now,
      "  max-age=0  ;  " + good_pin + ";" + backup_pin,
      ssl_info));
  expiry = now + base::TimeDelta::FromSeconds(0);
  EXPECT_EQ(expiry, state.dynamic_spki_hashes_expiry);

  EXPECT_TRUE(state.ParsePinsHeader(
      now,
      "  max-age=999999999999999999999999999999999999999999999  ;  " +
          backup_pin + ";" + good_pin + ";   ",
      ssl_info));
  expiry = now +
      base::TimeDelta::FromSeconds(TransportSecurityState::kMaxHSTSAgeSecs);
  EXPECT_EQ(expiry, state.dynamic_spki_hashes_expiry);
}

TEST_F(TransportSecurityStateTest, ValidPinsHeadersSHA1) {
  TestValidPinsHeaders(HASH_VALUE_SHA1);
}

TEST_F(TransportSecurityStateTest, ValidPinsHeadersSHA256) {
  TestValidPinsHeaders(HASH_VALUE_SHA256);
}

TEST_F(TransportSecurityStateTest, SimpleMatches) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.GetDomainState("yahoo.com", true, &domain_state));
  domain_state.upgrade_expiry = expiry;
  state.EnableHost("yahoo.com", domain_state);
  EXPECT_TRUE(state.GetDomainState("yahoo.com", true, &domain_state));
}

TEST_F(TransportSecurityStateTest, MatchesCase1) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.GetDomainState("yahoo.com", true, &domain_state));
  domain_state.upgrade_expiry = expiry;
  state.EnableHost("YAhoo.coM", domain_state);
  EXPECT_TRUE(state.GetDomainState("yahoo.com", true, &domain_state));
}

TEST_F(TransportSecurityStateTest, MatchesCase2) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.GetDomainState("YAhoo.coM", true, &domain_state));
  domain_state.upgrade_expiry = expiry;
  state.EnableHost("yahoo.com", domain_state);
  EXPECT_TRUE(state.GetDomainState("YAhoo.coM", true, &domain_state));
}

TEST_F(TransportSecurityStateTest, SubdomainMatches) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.GetDomainState("yahoo.com", true, &domain_state));
  domain_state.upgrade_expiry = expiry;
  domain_state.include_subdomains = true;
  state.EnableHost("yahoo.com", domain_state);
  EXPECT_TRUE(state.GetDomainState("yahoo.com", true, &domain_state));
  EXPECT_TRUE(state.GetDomainState("foo.yahoo.com", true, &domain_state));
  EXPECT_TRUE(state.GetDomainState("foo.bar.yahoo.com", true, &domain_state));
  EXPECT_TRUE(state.GetDomainState("foo.bar.baz.yahoo.com", true,
                                   &domain_state));
  EXPECT_FALSE(state.GetDomainState("com", true, &domain_state));
}

TEST_F(TransportSecurityStateTest, DeleteSince) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  const base::Time older = current_time - base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.GetDomainState("yahoo.com", true, &domain_state));
  domain_state.upgrade_mode =
      TransportSecurityState::DomainState::MODE_FORCE_HTTPS;
  domain_state.upgrade_expiry = expiry;
  state.EnableHost("yahoo.com", domain_state);

  state.DeleteSince(expiry);
  EXPECT_TRUE(state.GetDomainState("yahoo.com", true, &domain_state));
  state.DeleteSince(older);
  EXPECT_FALSE(state.GetDomainState("yahoo.com", true, &domain_state));
}

TEST_F(TransportSecurityStateTest, DeleteHost) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  domain_state.upgrade_mode =
      TransportSecurityState::DomainState::MODE_FORCE_HTTPS;
  domain_state.upgrade_expiry = expiry;
  state.EnableHost("yahoo.com", domain_state);

  EXPECT_TRUE(state.GetDomainState("yahoo.com", true, &domain_state));
  EXPECT_FALSE(state.GetDomainState("example.com", true, &domain_state));
  EXPECT_TRUE(state.DeleteHost("yahoo.com"));
  EXPECT_FALSE(state.GetDomainState("yahoo.com", true, &domain_state));
}

TEST_F(TransportSecurityStateTest, IsPreloaded) {
  const std::string paypal =
      TransportSecurityState::CanonicalizeHost("paypal.com");
  const std::string www_paypal =
      TransportSecurityState::CanonicalizeHost("www.paypal.com");
  const std::string a_www_paypal =
      TransportSecurityState::CanonicalizeHost("a.www.paypal.com");
  const std::string abc_paypal =
      TransportSecurityState::CanonicalizeHost("a.b.c.paypal.com");
  const std::string example =
      TransportSecurityState::CanonicalizeHost("example.com");
  const std::string aypal =
      TransportSecurityState::CanonicalizeHost("aypal.com");

  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;

  EXPECT_FALSE(state.GetStaticDomainState(paypal, true, &domain_state));
  EXPECT_TRUE(state.GetStaticDomainState(www_paypal, true, &domain_state));
  EXPECT_FALSE(domain_state.include_subdomains);
  EXPECT_FALSE(state.GetStaticDomainState(a_www_paypal, true, &domain_state));
  EXPECT_FALSE(state.GetStaticDomainState(abc_paypal, true, &domain_state));
  EXPECT_FALSE(state.GetStaticDomainState(example, true, &domain_state));
  EXPECT_FALSE(state.GetStaticDomainState(aypal, true, &domain_state));
}

TEST_F(TransportSecurityStateTest, PreloadedDomainSet) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;

  // The domain wasn't being set, leading to a blank string in the
  // chrome://net-internals/#hsts UI. So test that.
  EXPECT_TRUE(state.GetDomainState("market.android.com", true, &domain_state));
  EXPECT_EQ(domain_state.domain, "market.android.com");
  EXPECT_TRUE(state.GetDomainState("sub.market.android.com", true,
                                   &domain_state));
  EXPECT_EQ(domain_state.domain, "market.android.com");
}

static bool ShouldRedirect(const char* hostname) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  return state.GetDomainState(hostname, true /* SNI ok */, &domain_state) &&
         domain_state.ShouldRedirectHTTPToHTTPS();
}

static bool HasState(const char* hostname) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  return state.GetDomainState(hostname, true /* SNI ok */, &domain_state);
}

static bool HasPins(const char* hostname, bool sni_enabled) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  if (!state.GetDomainState(hostname, sni_enabled, &domain_state))
    return false;

  return domain_state.HasPins();
}

static bool HasPins(const char* hostname) {
  return HasPins(hostname, true);
}

static bool OnlyPinning(const char *hostname) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  if (!state.GetDomainState(hostname, true /* SNI ok */, &domain_state))
    return false;

  return (domain_state.static_spki_hashes.size() > 0 ||
          domain_state.bad_static_spki_hashes.size() > 0 ||
          domain_state.dynamic_spki_hashes.size() > 0) &&
         !domain_state.ShouldRedirectHTTPToHTTPS();
}

TEST_F(TransportSecurityStateTest, Preloaded) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;

  // We do more extensive checks for the first domain.
  EXPECT_TRUE(state.GetDomainState("www.paypal.com", true, &domain_state));
  EXPECT_EQ(domain_state.upgrade_mode,
            TransportSecurityState::DomainState::MODE_FORCE_HTTPS);
  EXPECT_FALSE(domain_state.include_subdomains);

  EXPECT_FALSE(HasState("paypal.com"));
  EXPECT_FALSE(HasState("www2.paypal.com"));
  EXPECT_FALSE(HasState("www2.paypal.com"));

  // Google hosts:

  EXPECT_TRUE(ShouldRedirect("chrome.google.com"));
  EXPECT_TRUE(ShouldRedirect("checkout.google.com"));
  EXPECT_TRUE(ShouldRedirect("health.google.com"));
  EXPECT_TRUE(ShouldRedirect("docs.google.com"));
  EXPECT_TRUE(ShouldRedirect("sites.google.com"));
  EXPECT_TRUE(ShouldRedirect("drive.google.com"));
  EXPECT_TRUE(ShouldRedirect("spreadsheets.google.com"));
  EXPECT_TRUE(ShouldRedirect("appengine.google.com"));
  EXPECT_TRUE(ShouldRedirect("market.android.com"));
  EXPECT_TRUE(ShouldRedirect("encrypted.google.com"));
  EXPECT_TRUE(ShouldRedirect("accounts.google.com"));
  EXPECT_TRUE(ShouldRedirect("profiles.google.com"));
  EXPECT_TRUE(ShouldRedirect("mail.google.com"));
  EXPECT_TRUE(ShouldRedirect("chatenabled.mail.google.com"));
  EXPECT_TRUE(ShouldRedirect("talkgadget.google.com"));
  EXPECT_TRUE(ShouldRedirect("hostedtalkgadget.google.com"));
  EXPECT_TRUE(ShouldRedirect("talk.google.com"));
  EXPECT_TRUE(ShouldRedirect("plus.google.com"));
  EXPECT_TRUE(ShouldRedirect("groups.google.com"));
  EXPECT_TRUE(ShouldRedirect("apis.google.com"));
  EXPECT_FALSE(ShouldRedirect("chart.apis.google.com"));
  EXPECT_TRUE(ShouldRedirect("ssl.google-analytics.com"));
  EXPECT_TRUE(ShouldRedirect("gmail.com"));
  EXPECT_TRUE(ShouldRedirect("www.gmail.com"));
  EXPECT_TRUE(ShouldRedirect("googlemail.com"));
  EXPECT_TRUE(ShouldRedirect("www.googlemail.com"));
  EXPECT_TRUE(ShouldRedirect("googleplex.com"));
  EXPECT_TRUE(ShouldRedirect("www.googleplex.com"));
  EXPECT_FALSE(HasState("m.gmail.com"));
  EXPECT_FALSE(HasState("m.googlemail.com"));

  EXPECT_TRUE(OnlyPinning("www.google.com"));
  EXPECT_TRUE(OnlyPinning("foo.google.com"));
  EXPECT_TRUE(OnlyPinning("google.com"));
  EXPECT_TRUE(OnlyPinning("www.youtube.com"));
  EXPECT_TRUE(OnlyPinning("youtube.com"));
  EXPECT_TRUE(OnlyPinning("i.ytimg.com"));
  EXPECT_TRUE(OnlyPinning("ytimg.com"));
  EXPECT_TRUE(OnlyPinning("googleusercontent.com"));
  EXPECT_TRUE(OnlyPinning("www.googleusercontent.com"));
  EXPECT_TRUE(OnlyPinning("www.google-analytics.com"));
  EXPECT_TRUE(OnlyPinning("googleapis.com"));
  EXPECT_TRUE(OnlyPinning("googleadservices.com"));
  EXPECT_TRUE(OnlyPinning("googlecode.com"));
  EXPECT_TRUE(OnlyPinning("appspot.com"));
  EXPECT_TRUE(OnlyPinning("googlesyndication.com"));
  EXPECT_TRUE(OnlyPinning("doubleclick.net"));
  EXPECT_TRUE(OnlyPinning("googlegroups.com"));

  // Tests for domains that don't work without SNI.
  EXPECT_FALSE(state.GetDomainState("gmail.com", false, &domain_state));
  EXPECT_FALSE(state.GetDomainState("www.gmail.com", false, &domain_state));
  EXPECT_FALSE(state.GetDomainState("m.gmail.com", false, &domain_state));
  EXPECT_FALSE(state.GetDomainState("googlemail.com", false, &domain_state));
  EXPECT_FALSE(state.GetDomainState("www.googlemail.com", false,
                                    &domain_state));
  EXPECT_FALSE(state.GetDomainState("m.googlemail.com", false, &domain_state));

  // Other hosts:

  EXPECT_TRUE(ShouldRedirect("aladdinschools.appspot.com"));

  EXPECT_TRUE(ShouldRedirect("ottospora.nl"));
  EXPECT_TRUE(ShouldRedirect("www.ottospora.nl"));

  EXPECT_TRUE(ShouldRedirect("www.paycheckrecords.com"));

  EXPECT_TRUE(ShouldRedirect("lastpass.com"));
  EXPECT_TRUE(ShouldRedirect("www.lastpass.com"));
  EXPECT_FALSE(HasState("blog.lastpass.com"));

  EXPECT_TRUE(ShouldRedirect("keyerror.com"));
  EXPECT_TRUE(ShouldRedirect("www.keyerror.com"));

  EXPECT_TRUE(ShouldRedirect("entropia.de"));
  EXPECT_TRUE(ShouldRedirect("www.entropia.de"));
  EXPECT_FALSE(HasState("foo.entropia.de"));

  EXPECT_TRUE(ShouldRedirect("www.elanex.biz"));
  EXPECT_FALSE(HasState("elanex.biz"));
  EXPECT_FALSE(HasState("foo.elanex.biz"));

  EXPECT_TRUE(ShouldRedirect("sunshinepress.org"));
  EXPECT_TRUE(ShouldRedirect("www.sunshinepress.org"));
  EXPECT_TRUE(ShouldRedirect("a.b.sunshinepress.org"));

  EXPECT_TRUE(ShouldRedirect("www.noisebridge.net"));
  EXPECT_FALSE(HasState("noisebridge.net"));
  EXPECT_FALSE(HasState("foo.noisebridge.net"));

  EXPECT_TRUE(ShouldRedirect("neg9.org"));
  EXPECT_FALSE(HasState("www.neg9.org"));

  EXPECT_TRUE(ShouldRedirect("riseup.net"));
  EXPECT_TRUE(ShouldRedirect("foo.riseup.net"));

  EXPECT_TRUE(ShouldRedirect("factor.cc"));
  EXPECT_FALSE(HasState("www.factor.cc"));

  EXPECT_TRUE(ShouldRedirect("members.mayfirst.org"));
  EXPECT_TRUE(ShouldRedirect("support.mayfirst.org"));
  EXPECT_TRUE(ShouldRedirect("id.mayfirst.org"));
  EXPECT_TRUE(ShouldRedirect("lists.mayfirst.org"));
  EXPECT_FALSE(HasState("www.mayfirst.org"));

  EXPECT_TRUE(ShouldRedirect("romab.com"));
  EXPECT_TRUE(ShouldRedirect("www.romab.com"));
  EXPECT_TRUE(ShouldRedirect("foo.romab.com"));

  EXPECT_TRUE(ShouldRedirect("logentries.com"));
  EXPECT_TRUE(ShouldRedirect("www.logentries.com"));
  EXPECT_FALSE(HasState("foo.logentries.com"));

  EXPECT_TRUE(ShouldRedirect("stripe.com"));
  EXPECT_TRUE(ShouldRedirect("foo.stripe.com"));

  EXPECT_TRUE(ShouldRedirect("cloudsecurityalliance.org"));
  EXPECT_TRUE(ShouldRedirect("foo.cloudsecurityalliance.org"));

  EXPECT_TRUE(ShouldRedirect("login.sapo.pt"));
  EXPECT_TRUE(ShouldRedirect("foo.login.sapo.pt"));

  EXPECT_TRUE(ShouldRedirect("mattmccutchen.net"));
  EXPECT_TRUE(ShouldRedirect("foo.mattmccutchen.net"));

  EXPECT_TRUE(ShouldRedirect("betnet.fr"));
  EXPECT_TRUE(ShouldRedirect("foo.betnet.fr"));

  EXPECT_TRUE(ShouldRedirect("uprotect.it"));
  EXPECT_TRUE(ShouldRedirect("foo.uprotect.it"));

  EXPECT_TRUE(ShouldRedirect("squareup.com"));
  EXPECT_FALSE(HasState("foo.squareup.com"));

  EXPECT_TRUE(ShouldRedirect("cert.se"));
  EXPECT_TRUE(ShouldRedirect("foo.cert.se"));

  EXPECT_TRUE(ShouldRedirect("crypto.is"));
  EXPECT_TRUE(ShouldRedirect("foo.crypto.is"));

  EXPECT_TRUE(ShouldRedirect("simon.butcher.name"));
  EXPECT_TRUE(ShouldRedirect("foo.simon.butcher.name"));

  EXPECT_TRUE(ShouldRedirect("linx.net"));
  EXPECT_TRUE(ShouldRedirect("foo.linx.net"));

  EXPECT_TRUE(ShouldRedirect("dropcam.com"));
  EXPECT_TRUE(ShouldRedirect("www.dropcam.com"));
  EXPECT_FALSE(HasState("foo.dropcam.com"));

  EXPECT_TRUE(state.GetDomainState("torproject.org", false, &domain_state));
  EXPECT_FALSE(domain_state.static_spki_hashes.empty());
  EXPECT_TRUE(state.GetDomainState("www.torproject.org", false,
                                   &domain_state));
  EXPECT_FALSE(domain_state.static_spki_hashes.empty());
  EXPECT_TRUE(state.GetDomainState("check.torproject.org", false,
                                   &domain_state));
  EXPECT_FALSE(domain_state.static_spki_hashes.empty());
  EXPECT_TRUE(state.GetDomainState("blog.torproject.org", false,
                                   &domain_state));
  EXPECT_FALSE(domain_state.static_spki_hashes.empty());
  EXPECT_TRUE(ShouldRedirect("ebanking.indovinabank.com.vn"));
  EXPECT_TRUE(ShouldRedirect("foo.ebanking.indovinabank.com.vn"));

  EXPECT_TRUE(ShouldRedirect("epoxate.com"));
  EXPECT_FALSE(HasState("foo.epoxate.com"));

  EXPECT_TRUE(HasPins("torproject.org"));
  EXPECT_TRUE(HasPins("www.torproject.org"));
  EXPECT_TRUE(HasPins("check.torproject.org"));
  EXPECT_TRUE(HasPins("blog.torproject.org"));
  EXPECT_FALSE(HasState("foo.torproject.org"));

  EXPECT_TRUE(ShouldRedirect("www.moneybookers.com"));
  EXPECT_FALSE(HasState("moneybookers.com"));

  EXPECT_TRUE(ShouldRedirect("ledgerscope.net"));
  EXPECT_TRUE(ShouldRedirect("www.ledgerscope.net"));
  EXPECT_FALSE(HasState("status.ledgerscope.net"));

  EXPECT_TRUE(ShouldRedirect("kyps.net"));
  EXPECT_TRUE(ShouldRedirect("www.kyps.net"));
  EXPECT_FALSE(HasState("foo.kyps.net"));

  EXPECT_TRUE(ShouldRedirect("foo.app.recurly.com"));
  EXPECT_TRUE(ShouldRedirect("foo.api.recurly.com"));

  EXPECT_TRUE(ShouldRedirect("greplin.com"));
  EXPECT_TRUE(ShouldRedirect("www.greplin.com"));
  EXPECT_FALSE(HasState("foo.greplin.com"));

  EXPECT_TRUE(ShouldRedirect("luneta.nearbuysystems.com"));
  EXPECT_TRUE(ShouldRedirect("foo.luneta.nearbuysystems.com"));

  EXPECT_TRUE(ShouldRedirect("ubertt.org"));
  EXPECT_TRUE(ShouldRedirect("foo.ubertt.org"));

  EXPECT_TRUE(ShouldRedirect("pixi.me"));
  EXPECT_TRUE(ShouldRedirect("www.pixi.me"));

  EXPECT_TRUE(ShouldRedirect("grepular.com"));
  EXPECT_TRUE(ShouldRedirect("www.grepular.com"));

  EXPECT_TRUE(ShouldRedirect("mydigipass.com"));
  EXPECT_FALSE(ShouldRedirect("foo.mydigipass.com"));
  EXPECT_TRUE(ShouldRedirect("www.mydigipass.com"));
  EXPECT_FALSE(ShouldRedirect("foo.www.mydigipass.com"));
  EXPECT_TRUE(ShouldRedirect("developer.mydigipass.com"));
  EXPECT_FALSE(ShouldRedirect("foo.developer.mydigipass.com"));
  EXPECT_TRUE(ShouldRedirect("www.developer.mydigipass.com"));
  EXPECT_FALSE(ShouldRedirect("foo.www.developer.mydigipass.com"));
  EXPECT_TRUE(ShouldRedirect("sandbox.mydigipass.com"));
  EXPECT_FALSE(ShouldRedirect("foo.sandbox.mydigipass.com"));
  EXPECT_TRUE(ShouldRedirect("www.sandbox.mydigipass.com"));
  EXPECT_FALSE(ShouldRedirect("foo.www.sandbox.mydigipass.com"));

  EXPECT_TRUE(ShouldRedirect("crypto.cat"));
  EXPECT_TRUE(ShouldRedirect("foo.crypto.cat"));

  EXPECT_TRUE(ShouldRedirect("bigshinylock.minazo.net"));
  EXPECT_TRUE(ShouldRedirect("foo.bigshinylock.minazo.net"));

  EXPECT_TRUE(ShouldRedirect("crate.io"));
  EXPECT_TRUE(ShouldRedirect("foo.crate.io"));

  EXPECT_TRUE(HasPins("www.twitter.com"));
}

TEST_F(TransportSecurityStateTest, LongNames) {
  TransportSecurityState state;
  const char kLongName[] =
      "lookupByWaveIdHashAndWaveIdIdAndWaveIdDomainAndWaveletIdIdAnd"
      "WaveletIdDomainAndBlipBlipid";
  TransportSecurityState::DomainState domain_state;
  // Just checks that we don't hit a NOTREACHED.
  EXPECT_FALSE(state.GetDomainState(kLongName, true, &domain_state));
}

TEST_F(TransportSecurityStateTest, BuiltinCertPins) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;

  EXPECT_TRUE(state.GetDomainState("chrome.google.com", true, &domain_state));
  EXPECT_TRUE(HasPins("chrome.google.com"));

  HashValueVector hashes;
  // Checks that a built-in list does exist.
  EXPECT_FALSE(domain_state.IsChainOfPublicKeysPermitted(hashes));
  EXPECT_FALSE(HasPins("www.paypal.com"));

  EXPECT_TRUE(HasPins("docs.google.com"));
  EXPECT_TRUE(HasPins("1.docs.google.com"));
  EXPECT_TRUE(HasPins("sites.google.com"));
  EXPECT_TRUE(HasPins("drive.google.com"));
  EXPECT_TRUE(HasPins("spreadsheets.google.com"));
  EXPECT_TRUE(HasPins("health.google.com"));
  EXPECT_TRUE(HasPins("checkout.google.com"));
  EXPECT_TRUE(HasPins("appengine.google.com"));
  EXPECT_TRUE(HasPins("market.android.com"));
  EXPECT_TRUE(HasPins("encrypted.google.com"));
  EXPECT_TRUE(HasPins("accounts.google.com"));
  EXPECT_TRUE(HasPins("profiles.google.com"));
  EXPECT_TRUE(HasPins("mail.google.com"));
  EXPECT_TRUE(HasPins("chatenabled.mail.google.com"));
  EXPECT_TRUE(HasPins("talkgadget.google.com"));
  EXPECT_TRUE(HasPins("hostedtalkgadget.google.com"));
  EXPECT_TRUE(HasPins("talk.google.com"));
  EXPECT_TRUE(HasPins("plus.google.com"));
  EXPECT_TRUE(HasPins("groups.google.com"));
  EXPECT_TRUE(HasPins("apis.google.com"));

  EXPECT_TRUE(HasPins("ssl.gstatic.com"));
  EXPECT_FALSE(HasPins("www.gstatic.com"));
  EXPECT_TRUE(HasPins("ssl.google-analytics.com"));
  EXPECT_TRUE(HasPins("www.googleplex.com"));

  // Disabled in order to help track down pinning failures --agl
  EXPECT_TRUE(HasPins("twitter.com"));
  EXPECT_FALSE(HasPins("foo.twitter.com"));
  EXPECT_TRUE(HasPins("www.twitter.com"));
  EXPECT_TRUE(HasPins("api.twitter.com"));
  EXPECT_TRUE(HasPins("oauth.twitter.com"));
  EXPECT_TRUE(HasPins("mobile.twitter.com"));
  EXPECT_TRUE(HasPins("dev.twitter.com"));
  EXPECT_TRUE(HasPins("business.twitter.com"));
  EXPECT_TRUE(HasPins("platform.twitter.com"));
  EXPECT_TRUE(HasPins("si0.twimg.com"));
  EXPECT_TRUE(HasPins("twimg0-a.akamaihd.net"));
}

static bool AddHash(const std::string& type_and_base64,
                    HashValueVector* out) {
  HashValue hash;

  if (!TransportSecurityState::ParsePin(type_and_base64, &hash))
    return false;

  out->push_back(hash);
  return true;
}

TEST_F(TransportSecurityStateTest, PinValidationWithRejectedCerts) {
  // kGoodPath is plus.google.com via Google Internet Authority.
  static const char* kGoodPath[] = {
    "sha1/4BjDjn8v2lWeUFQnqSs0BgbIcrU=",
    "sha1/QMVAHW+MuvCLAO3vse6H0AWzuc0=",
    "sha1/SOZo+SvSspXXR9gjIBBPM5iQn9Q=",
    NULL,
  };

  // kBadPath is plus.google.com via Trustcenter, which contains a required
  // certificate (Equifax root), but also an excluded certificate
  // (Trustcenter).
  static const char* kBadPath[] = {
    "sha1/4BjDjn8v2lWeUFQnqSs0BgbIcrU=",
    "sha1/gzuEEAB/bkqdQS3EIjk2by7lW+k=",
    "sha1/SOZo+SvSspXXR9gjIBBPM5iQn9Q=",
    NULL,
  };

  HashValueVector good_hashes, bad_hashes;

  for (size_t i = 0; kGoodPath[i]; i++) {
    EXPECT_TRUE(AddHash(kGoodPath[i], &good_hashes));
  }
  for (size_t i = 0; kBadPath[i]; i++) {
    EXPECT_TRUE(AddHash(kBadPath[i], &bad_hashes));
  }

  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  EXPECT_TRUE(state.GetDomainState("plus.google.com", true, &domain_state));
  EXPECT_TRUE(domain_state.HasPins());

  EXPECT_TRUE(domain_state.IsChainOfPublicKeysPermitted(good_hashes));
  EXPECT_FALSE(domain_state.IsChainOfPublicKeysPermitted(bad_hashes));
}

TEST_F(TransportSecurityStateTest, PinValidationWithoutRejectedCerts) {
  // kGoodPath is blog.torproject.org.
  static const char* kGoodPath[] = {
    "sha1/m9lHYJYke9k0GtVZ+bXSQYE8nDI=",
    "sha1/o5OZxATDsgmwgcIfIWIneMJ0jkw=",
    "sha1/wHqYaI2J+6sFZAwRfap9ZbjKzE4=",
    NULL,
  };

  // kBadPath is plus.google.com via Trustcenter, which is utterly wrong for
  // torproject.org.
  static const char* kBadPath[] = {
    "sha1/4BjDjn8v2lWeUFQnqSs0BgbIcrU=",
    "sha1/gzuEEAB/bkqdQS3EIjk2by7lW+k=",
    "sha1/SOZo+SvSspXXR9gjIBBPM5iQn9Q=",
    NULL,
  };

  HashValueVector good_hashes, bad_hashes;

  for (size_t i = 0; kGoodPath[i]; i++) {
    EXPECT_TRUE(AddHash(kGoodPath[i], &good_hashes));
  }
  for (size_t i = 0; kBadPath[i]; i++) {
    EXPECT_TRUE(AddHash(kBadPath[i], &bad_hashes));
  }

  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  EXPECT_TRUE(state.GetDomainState("blog.torproject.org", true, &domain_state));
  EXPECT_TRUE(domain_state.HasPins());

  EXPECT_TRUE(domain_state.IsChainOfPublicKeysPermitted(good_hashes));
  EXPECT_FALSE(domain_state.IsChainOfPublicKeysPermitted(bad_hashes));
}

TEST_F(TransportSecurityStateTest, PinValidationWithRejectedCertsMixedHashes) {
  static const char* ee_sha1 = "sha1/4BjDjn8v2lWeUFQnqSs0BgbIcrU=";
  static const char* ee_sha256 =
      "sha256/sRJBQqWhpaKIGcc1NA7/jJ4vgWj+47oYfyU7waOS1+I=";
  static const char* google_1024_sha1 = "sha1/QMVAHW+MuvCLAO3vse6H0AWzuc0=";
  static const char* google_1024_sha256 =
      "sha256/trlUMquuV/4CDLK3T0+fkXPIxwivyecyrOIyeQR8bQU=";
  static const char* equifax_sha1 = "sha1/SOZo+SvSspXXR9gjIBBPM5iQn9Q=";
  static const char* equifax_sha256 =
      "sha256//1aAzXOlcD2gSBegdf1GJQanNQbEuBoVg+9UlHjSZHY=";
  static const char* trustcenter_sha1 = "sha1/gzuEEAB/bkqdQS3EIjk2by7lW+k=";
  static const char* trustcenter_sha256 =
      "sha256/Dq58KIA4NMLsboWMLU8/aTREzaAGEFW+EtUule8dd/M=";

  // Good chains for plus.google.com chain up through google_1024_sha{1,256}
  // to equifax_sha{1,256}. Bad chains chain up to Equifax through
  // trustcenter_sha{1,256}, which is a blacklisted key. Even though Equifax
  // and Google1024 are known-good, the blacklistedness of Trustcenter
  // should override and cause pin validation failure.

  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  EXPECT_TRUE(state.GetDomainState("plus.google.com", true, &domain_state));
  EXPECT_TRUE(domain_state.HasPins());

  // The statically-defined pins are all SHA-1, so we add some SHA-256 pins
  // manually:
  EXPECT_TRUE(AddHash(google_1024_sha256, &domain_state.static_spki_hashes));
  EXPECT_TRUE(AddHash(trustcenter_sha256,
                      &domain_state.bad_static_spki_hashes));

  // Try an all-good SHA1 chain.
  HashValueVector validated_chain;
  EXPECT_TRUE(AddHash(ee_sha1, &validated_chain));
  EXPECT_TRUE(AddHash(google_1024_sha1, &validated_chain));
  EXPECT_TRUE(AddHash(equifax_sha1, &validated_chain));
  EXPECT_TRUE(domain_state.IsChainOfPublicKeysPermitted(validated_chain));

  // Try an all-bad SHA1 chain.
  validated_chain.clear();
  EXPECT_TRUE(AddHash(ee_sha1, &validated_chain));
  EXPECT_TRUE(AddHash(trustcenter_sha1, &validated_chain));
  EXPECT_TRUE(AddHash(equifax_sha1, &validated_chain));
  EXPECT_FALSE(domain_state.IsChainOfPublicKeysPermitted(validated_chain));

  // Try an all-good SHA-256 chain.
  validated_chain.clear();
  EXPECT_TRUE(AddHash(ee_sha256, &validated_chain));
  EXPECT_TRUE(AddHash(google_1024_sha256, &validated_chain));
  EXPECT_TRUE(AddHash(equifax_sha256, &validated_chain));
  EXPECT_TRUE(domain_state.IsChainOfPublicKeysPermitted(validated_chain));

  // Try an all-bad SHA-256 chain.
  validated_chain.clear();
  EXPECT_TRUE(AddHash(ee_sha256, &validated_chain));
  EXPECT_TRUE(AddHash(trustcenter_sha256, &validated_chain));
  EXPECT_TRUE(AddHash(equifax_sha256, &validated_chain));
  EXPECT_FALSE(domain_state.IsChainOfPublicKeysPermitted(validated_chain));

  // Try a mixed-hash good chain.
  validated_chain.clear();
  EXPECT_TRUE(AddHash(ee_sha256, &validated_chain));
  EXPECT_TRUE(AddHash(google_1024_sha1, &validated_chain));
  EXPECT_TRUE(AddHash(equifax_sha256, &validated_chain));
  EXPECT_TRUE(domain_state.IsChainOfPublicKeysPermitted(validated_chain));

  // Try a mixed-hash bad chain.
  validated_chain.clear();
  EXPECT_TRUE(AddHash(ee_sha1, &validated_chain));
  EXPECT_TRUE(AddHash(trustcenter_sha256, &validated_chain));
  EXPECT_TRUE(AddHash(equifax_sha1, &validated_chain));
  EXPECT_FALSE(domain_state.IsChainOfPublicKeysPermitted(validated_chain));

  // Try a chain with all good hashes.
  validated_chain.clear();
  EXPECT_TRUE(AddHash(ee_sha1, &validated_chain));
  EXPECT_TRUE(AddHash(google_1024_sha1, &validated_chain));
  EXPECT_TRUE(AddHash(equifax_sha1, &validated_chain));
  EXPECT_TRUE(AddHash(ee_sha256, &validated_chain));
  EXPECT_TRUE(AddHash(google_1024_sha256, &validated_chain));
  EXPECT_TRUE(AddHash(equifax_sha256, &validated_chain));
  EXPECT_TRUE(domain_state.IsChainOfPublicKeysPermitted(validated_chain));

  // Try a chain with all bad hashes.
  validated_chain.clear();
  EXPECT_TRUE(AddHash(ee_sha1, &validated_chain));
  EXPECT_TRUE(AddHash(trustcenter_sha1, &validated_chain));
  EXPECT_TRUE(AddHash(equifax_sha1, &validated_chain));
  EXPECT_TRUE(AddHash(ee_sha256, &validated_chain));
  EXPECT_TRUE(AddHash(trustcenter_sha256, &validated_chain));
  EXPECT_TRUE(AddHash(equifax_sha256, &validated_chain));
  EXPECT_FALSE(domain_state.IsChainOfPublicKeysPermitted(validated_chain));
}

TEST_F(TransportSecurityStateTest, OptionalHSTSCertPins) {
  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;

  EXPECT_FALSE(ShouldRedirect("www.google-analytics.com"));

  EXPECT_FALSE(HasPins("www.google-analytics.com", false));
  EXPECT_TRUE(HasPins("www.google-analytics.com"));
  EXPECT_TRUE(HasPins("google.com"));
  EXPECT_TRUE(HasPins("www.google.com"));
  EXPECT_TRUE(HasPins("mail-attachment.googleusercontent.com"));
  EXPECT_TRUE(HasPins("www.youtube.com"));
  EXPECT_TRUE(HasPins("i.ytimg.com"));
  EXPECT_TRUE(HasPins("googleapis.com"));
  EXPECT_TRUE(HasPins("ajax.googleapis.com"));
  EXPECT_TRUE(HasPins("googleadservices.com"));
  EXPECT_TRUE(HasPins("pagead2.googleadservices.com"));
  EXPECT_TRUE(HasPins("googlecode.com"));
  EXPECT_TRUE(HasPins("kibbles.googlecode.com"));
  EXPECT_TRUE(HasPins("appspot.com"));
  EXPECT_TRUE(HasPins("googlesyndication.com"));
  EXPECT_TRUE(HasPins("doubleclick.net"));
  EXPECT_TRUE(HasPins("ad.doubleclick.net"));
  EXPECT_FALSE(HasPins("learn.doubleclick.net"));
  EXPECT_TRUE(HasPins("a.googlegroups.com"));
  EXPECT_FALSE(HasPins("a.googlegroups.com", false));
}

TEST_F(TransportSecurityStateTest, OverrideBuiltins) {
  EXPECT_TRUE(HasPins("google.com"));
  EXPECT_FALSE(ShouldRedirect("google.com"));
  EXPECT_FALSE(ShouldRedirect("www.google.com"));

  TransportSecurityState state;
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  domain_state.upgrade_expiry = expiry;
  state.EnableHost("www.google.com", domain_state);

  EXPECT_TRUE(state.GetDomainState("www.google.com", true, &domain_state));
}

static const uint8 kSidePinLeafSPKI[] = {
  0x30, 0x5c, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01,
  0x01, 0x01, 0x05, 0x00, 0x03, 0x4b, 0x00, 0x30, 0x48, 0x02, 0x41, 0x00, 0xe4,
  0x1d, 0xcc, 0xf2, 0x92, 0xe7, 0x7a, 0xc6, 0x36, 0xf7, 0x1a, 0x62, 0x31, 0x7d,
  0x37, 0xea, 0x0d, 0xa2, 0xa8, 0x12, 0x2b, 0xc2, 0x1c, 0x82, 0x3e, 0xa5, 0x70,
  0x4a, 0x83, 0x5d, 0x9b, 0x84, 0x82, 0x70, 0xa4, 0x88, 0x98, 0x98, 0x41, 0x29,
  0x31, 0xcb, 0x6e, 0x2a, 0x54, 0x65, 0x14, 0x60, 0xcc, 0x00, 0xe8, 0x10, 0x30,
  0x0a, 0x4a, 0xd1, 0xa7, 0x52, 0xfe, 0x2d, 0x31, 0x2a, 0x1d, 0x0d, 0x02, 0x03,
  0x01, 0x00, 0x01,
};

static const uint8 kSidePinInfo[] = {
  0x01, 0x00, 0x53, 0x50, 0x49, 0x4e, 0xa0, 0x00, 0x03, 0x00, 0x53, 0x49, 0x47,
  0x00, 0x50, 0x55, 0x42, 0x4b, 0x41, 0x4c, 0x47, 0x4f, 0x47, 0x00, 0x41, 0x00,
  0x04, 0x00, 0x30, 0x45, 0x02, 0x21, 0x00, 0xfb, 0x26, 0xd5, 0xe8, 0x76, 0x35,
  0x96, 0x6d, 0x91, 0x9b, 0x5b, 0x27, 0xe6, 0x09, 0x1c, 0x7b, 0x6c, 0xcd, 0xc8,
  0x10, 0x25, 0x95, 0xc0, 0xa5, 0xf6, 0x6c, 0x6f, 0xfb, 0x59, 0x1e, 0x2d, 0xf4,
  0x02, 0x20, 0x33, 0x0a, 0xf8, 0x8b, 0x3e, 0xc4, 0xca, 0x75, 0x28, 0xdf, 0x5f,
  0xab, 0xe4, 0x46, 0xa0, 0xdd, 0x2d, 0xe5, 0xad, 0xc3, 0x81, 0x44, 0x70, 0xb2,
  0x10, 0x87, 0xe8, 0xc3, 0xd6, 0x6e, 0x12, 0x5d, 0x04, 0x67, 0x0b, 0x7d, 0xf2,
  0x99, 0x75, 0x57, 0x99, 0x3a, 0x98, 0xf8, 0xe4, 0xdf, 0x79, 0xdf, 0x8e, 0x02,
  0x2c, 0xbe, 0xd8, 0xfd, 0x75, 0x80, 0x18, 0xb1, 0x6f, 0x43, 0xd9, 0x8a, 0x79,
  0xc3, 0x6e, 0x18, 0xdf, 0x79, 0xc0, 0x59, 0xab, 0xd6, 0x77, 0x37, 0x6a, 0x94,
  0x5a, 0x7e, 0xfb, 0xa9, 0xc5, 0x54, 0x14, 0x3a, 0x7b, 0x97, 0x17, 0x2a, 0xb6,
  0x1e, 0x59, 0x4f, 0x2f, 0xb1, 0x15, 0x1a, 0x34, 0x50, 0x32, 0x35, 0x36,
};

static const uint8 kSidePinExpectedHash[20] = {
  0xb5, 0x91, 0x66, 0x47, 0x43, 0x16, 0x62, 0x86, 0xd4, 0x1e, 0x5d, 0x36, 0xe1,
  0xc4, 0x09, 0x3d, 0x2d, 0x1d, 0xea, 0x1e,
};

TEST_F(TransportSecurityStateTest, GooglePinnedProperties) {
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.example.com", true));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.paypal.com", true));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "mail.twitter.com", true));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.google.com.int", true));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "jottit.com", true));
  // learn.doubleclick.net has a more specific match than
  // *.doubleclick.com, and has 0 or NULL for its required certs.
  // This test ensures that the exact-match-preferred behavior
  // works.
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "learn.doubleclick.net", true));

  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "encrypted.google.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "mail.google.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "accounts.google.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "doubleclick.net", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "ad.doubleclick.net", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "youtube.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "www.profiles.google.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "checkout.google.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "googleadservices.com", true));

  // Test with sni_enabled false:
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.example.com", false));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.paypal.com", false));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "checkout.google.com", false));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "googleadservices.com", false));

  // Test some SNI hosts:
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "gmail.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "googlegroups.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "www.googlegroups.com", true));
  // Expect to fail for SNI hosts when not searching the SNI list:
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "gmail.com", false));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "googlegroups.com", false));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.googlegroups.com", false));
}

}  // namespace net
