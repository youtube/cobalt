// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_util.h"
#include "net/base/x509_util_nss.h"

#include <cert.h>
#include <secoid.h>

#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "crypto/rsa_private_key.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

CERTCertificate* CreateNSSCertHandleFromBytes(const char* data, size_t length) {
  SECItem der_cert;
  der_cert.data = reinterpret_cast<unsigned char*>(const_cast<char*>(data));
  der_cert.len  = length;
  der_cert.type = siDERCertBuffer;

  // Parse into a certificate structure.
  return CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &der_cert, NULL,
                                 PR_FALSE, PR_TRUE);
}

}  // namespace

namespace net {

// This test creates an origin-bound cert from a private key and
// then verifies the content of the certificate.
TEST(X509UtilNSSTest, CreateOriginBoundCert) {
  // Origin Bound Cert OID.
  static const char oid_string[] = "1.3.6.1.4.1.11129.2.1.6";

  // Create a sample ASCII weborigin.
  std::string origin = "http://weborigin.com:443";

  // Create object neccessary for extension lookup call.
  SECItem extension_object = {
    siAsciiString,
    (unsigned char*)origin.data(),
    origin.size()
  };

  scoped_ptr<crypto::RSAPrivateKey> private_key(
      crypto::RSAPrivateKey::Create(1024));
  std::string der_cert;
  ASSERT_TRUE(x509_util::CreateOriginBoundCert(private_key.get(),
                                               origin, 1,
                                               base::TimeDelta::FromDays(1),
                                               &der_cert));

  scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromBytes(
      der_cert.data(), der_cert.size());

  EXPECT_EQ("anonymous.invalid", cert->subject().GetDisplayName());
  EXPECT_FALSE(cert->HasExpired());

  // IA5Encode and arena allocate SECItem.
  PLArenaPool* arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  SECItem* expected = SEC_ASN1EncodeItem(arena,
                                         NULL,
                                         &extension_object,
                                         SEC_ASN1_GET(SEC_IA5StringTemplate));

  ASSERT_NE(static_cast<SECItem*>(NULL), expected);

  // Create OID SECItem.
  SECItem ob_cert_oid = { siDEROID, NULL, 0 };
  SECStatus ok = SEC_StringToOID(arena, &ob_cert_oid,
                                 oid_string, 0);

  ASSERT_EQ(SECSuccess, ok);

  SECOidTag ob_cert_oid_tag = SECOID_FindOIDTag(&ob_cert_oid);

  ASSERT_NE(SEC_OID_UNKNOWN, ob_cert_oid_tag);

  // This test is run on Mac and Win where X509Certificate::os_cert_handle isn't
  // an NSS type, so we have to manually create a NSS certificate object so we
  // can use CERT_FindCertExtension.
  CERTCertificate* nss_cert = CreateNSSCertHandleFromBytes(
      der_cert.data(), der_cert.size());
  // Lookup Origin Bound Cert extension in generated cert.
  SECItem actual = { siBuffer, NULL, 0 };
  ok = CERT_FindCertExtension(nss_cert,
                              ob_cert_oid_tag,
                              &actual);
  CERT_DestroyCertificate(nss_cert);
  ASSERT_EQ(SECSuccess, ok);

  // Compare expected and actual extension values.
  PRBool result = SECITEM_ItemsAreEqual(expected, &actual);
  ASSERT_TRUE(result);

  // Do Cleanup.
  SECITEM_FreeItem(&actual, PR_FALSE);
  PORT_FreeArena(arena, PR_FALSE);
}

}  // namespace net
