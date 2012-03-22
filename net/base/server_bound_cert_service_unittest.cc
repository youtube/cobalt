// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/server_bound_cert_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/ec_private_key.h"
#include "net/base/asn1_util.h"
#include "net/base/default_server_bound_cert_store.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

void FailTest(int /* result */) {
  FAIL();
}

TEST(ServerBoundCertServiceTest, GetDomainForHost) {
  EXPECT_EQ("google.com",
            ServerBoundCertService::GetDomainForHost("google.com"));
  EXPECT_EQ("google.com",
            ServerBoundCertService::GetDomainForHost("www.google.com"));
  // NOTE(rch): we would like to segregate cookies and certificates for
  // *.appspot.com, but currently we can not do that becaues we want to
  // allow direct navigation to appspot.com.
  EXPECT_EQ("appspot.com",
            ServerBoundCertService::GetDomainForHost("foo.appspot.com"));
  EXPECT_EQ("google.com",
            ServerBoundCertService::GetDomainForHost("www.mail.google.com"));
  EXPECT_EQ("goto",
            ServerBoundCertService::GetDomainForHost("goto"));
  EXPECT_EQ("127.0.0.1",
            ServerBoundCertService::GetDomainForHost("127.0.0.1"));
}

// See http://crbug.com/91512 - implement OpenSSL version of CreateSelfSigned.
#if !defined(USE_OPENSSL)

TEST(ServerBoundCertServiceTest, CacheHit) {
  scoped_ptr<ServerBoundCertService> service(
      new ServerBoundCertService(new DefaultServerBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com:443");

  int error;
  std::vector<uint8> types;
  types.push_back(CLIENT_CERT_ECDSA_SIGN);
  TestCompletionCallback callback;
  ServerBoundCertService::RequestHandle request_handle;

  // Asynchronous completion.
  SSLClientCertType type1;
  std::string private_key_info1, der_cert1;
  EXPECT_EQ(0, service->cert_count());
  error = service->GetDomainBoundCert(
      origin, types, &type1, &private_key_info1, &der_cert1,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service->cert_count());
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type1);
  EXPECT_FALSE(private_key_info1.empty());
  EXPECT_FALSE(der_cert1.empty());

  // Synchronous completion.
  SSLClientCertType type2;
  std::string private_key_info2, der_cert2;
  error = service->GetDomainBoundCert(
      origin, types, &type2, &private_key_info2, &der_cert2,
      callback.callback(), &request_handle);
  EXPECT_TRUE(request_handle == NULL);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service->cert_count());
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type2);
  EXPECT_EQ(private_key_info1, private_key_info2);
  EXPECT_EQ(der_cert1, der_cert2);

  EXPECT_EQ(2u, service->requests());
  EXPECT_EQ(1u, service->cert_store_hits());
  EXPECT_EQ(0u, service->inflight_joins());
}

TEST(ServerBoundCertServiceTest, UnsupportedTypes) {
  scoped_ptr<ServerBoundCertService> service(
      new ServerBoundCertService(new DefaultServerBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com:443");

  int error;
  std::vector<uint8> types;
  TestCompletionCallback callback;
  ServerBoundCertService::RequestHandle request_handle;

  // Empty requested_types.
  SSLClientCertType type1;
  std::string private_key_info1, der_cert1;
  error = service->GetDomainBoundCert(
      origin, types, &type1, &private_key_info1, &der_cert1,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_INVALID_ARGUMENT, error);
  EXPECT_TRUE(request_handle == NULL);

  // No supported types in requested_types.
  types.push_back(CLIENT_CERT_RSA_SIGN);
  types.push_back(2);
  types.push_back(3);
  error = service->GetDomainBoundCert(
      origin, types, &type1, &private_key_info1, &der_cert1,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_CLIENT_AUTH_CERT_TYPE_UNSUPPORTED, error);
  EXPECT_TRUE(request_handle == NULL);

  // Supported types after unsupported ones in requested_types.
  types.push_back(CLIENT_CERT_ECDSA_SIGN);
  // Asynchronous completion.
  EXPECT_EQ(0, service->cert_count());
  error = service->GetDomainBoundCert(
      origin, types, &type1, &private_key_info1, &der_cert1,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service->cert_count());
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type1);
  EXPECT_FALSE(private_key_info1.empty());
  EXPECT_FALSE(der_cert1.empty());

  // Now that the cert is created, doing requests for unsupported types
  // shouldn't affect the created cert.
  // Empty requested_types.
  types.clear();
  SSLClientCertType type2;
  std::string private_key_info2, der_cert2;
  error = service->GetDomainBoundCert(
      origin, types, &type2, &private_key_info2, &der_cert2,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_INVALID_ARGUMENT, error);
  EXPECT_TRUE(request_handle == NULL);

  // No supported types in requested_types.
  types.push_back(CLIENT_CERT_RSA_SIGN);
  types.push_back(2);
  types.push_back(3);
  error = service->GetDomainBoundCert(
      origin, types, &type2, &private_key_info2, &der_cert2,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_CLIENT_AUTH_CERT_TYPE_UNSUPPORTED, error);
  EXPECT_TRUE(request_handle == NULL);

  // If we request EC, the cert we created before should still be there.
  types.push_back(CLIENT_CERT_ECDSA_SIGN);
  error = service->GetDomainBoundCert(
      origin, types, &type2, &private_key_info2, &der_cert2,
      callback.callback(), &request_handle);
  EXPECT_TRUE(request_handle == NULL);
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service->cert_count());
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type2);
  EXPECT_EQ(private_key_info1, private_key_info2);
  EXPECT_EQ(der_cert1, der_cert2);
}

TEST(ServerBoundCertServiceTest, StoreCerts) {
  scoped_ptr<ServerBoundCertService> service(
      new ServerBoundCertService(new DefaultServerBoundCertStore(NULL)));
  int error;
  std::vector<uint8> types;
  types.push_back(CLIENT_CERT_ECDSA_SIGN);
  TestCompletionCallback callback;
  ServerBoundCertService::RequestHandle request_handle;

  std::string origin1("https://encrypted.google.com:443");
  SSLClientCertType type1;
  std::string private_key_info1, der_cert1;
  EXPECT_EQ(0, service->cert_count());
  error = service->GetDomainBoundCert(
      origin1, types, &type1, &private_key_info1, &der_cert1,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service->cert_count());

  std::string origin2("https://www.verisign.com:443");
  SSLClientCertType type2;
  std::string private_key_info2, der_cert2;
  error = service->GetDomainBoundCert(
      origin2, types, &type2, &private_key_info2, &der_cert2,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(2, service->cert_count());

  std::string origin3("https://www.twitter.com:443");
  SSLClientCertType type3;
  std::string private_key_info3, der_cert3;
  error = service->GetDomainBoundCert(
      origin3, types, &type3, &private_key_info3, &der_cert3,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(3, service->cert_count());

  EXPECT_NE(private_key_info1, private_key_info2);
  EXPECT_NE(der_cert1, der_cert2);
  EXPECT_NE(private_key_info1, private_key_info3);
  EXPECT_NE(der_cert1, der_cert3);
  EXPECT_NE(private_key_info2, private_key_info3);
  EXPECT_NE(der_cert2, der_cert3);
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type1);
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type2);
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type3);
}

// Tests an inflight join.
TEST(ServerBoundCertServiceTest, InflightJoin) {
  scoped_ptr<ServerBoundCertService> service(
      new ServerBoundCertService(new DefaultServerBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com:443");
  int error;
  std::vector<uint8> types;
  types.push_back(CLIENT_CERT_ECDSA_SIGN);

  SSLClientCertType type1;
  std::string private_key_info1, der_cert1;
  TestCompletionCallback callback1;
  ServerBoundCertService::RequestHandle request_handle1;

  SSLClientCertType type2;
  std::string private_key_info2, der_cert2;
  TestCompletionCallback callback2;
  ServerBoundCertService::RequestHandle request_handle2;

  error = service->GetDomainBoundCert(
      origin, types, &type1, &private_key_info1, &der_cert1,
      callback1.callback(), &request_handle1);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle1 != NULL);
  // If we request RSA and EC in the 2nd request, should still join with the
  // original request.
  types.insert(types.begin(), CLIENT_CERT_RSA_SIGN);
  error = service->GetDomainBoundCert(
      origin, types, &type2, &private_key_info2, &der_cert2,
      callback2.callback(), &request_handle2);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle2 != NULL);

  error = callback1.WaitForResult();
  EXPECT_EQ(OK, error);
  error = callback2.WaitForResult();
  EXPECT_EQ(OK, error);

  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type1);
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type2);
  EXPECT_EQ(2u, service->requests());
  EXPECT_EQ(0u, service->cert_store_hits());
  EXPECT_EQ(1u, service->inflight_joins());
}

TEST(ServerBoundCertServiceTest, ExtractValuesFromBytesEC) {
  scoped_ptr<ServerBoundCertService> service(
      new ServerBoundCertService(new DefaultServerBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com:443");
  SSLClientCertType type;
  std::string private_key_info, der_cert;
  int error;
  std::vector<uint8> types;
  types.push_back(CLIENT_CERT_ECDSA_SIGN);
  TestCompletionCallback callback;
  ServerBoundCertService::RequestHandle request_handle;

  error = service->GetDomainBoundCert(
      origin, types, &type, &private_key_info, &der_cert, callback.callback(),
      &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);

  base::StringPiece spki_piece;
  ASSERT_TRUE(asn1::ExtractSPKIFromDERCert(der_cert, &spki_piece));
  std::vector<uint8> spki(
      spki_piece.data(),
      spki_piece.data() + spki_piece.size());

  // Check that we can retrieve the key from the bytes.
  std::vector<uint8> key_vec(private_key_info.begin(), private_key_info.end());
  scoped_ptr<crypto::ECPrivateKey> private_key(
      crypto::ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
          ServerBoundCertService::kEPKIPassword, key_vec, spki));
  EXPECT_TRUE(private_key != NULL);

  // Check that we can retrieve the cert from the bytes.
  scoped_refptr<X509Certificate> x509cert(
      X509Certificate::CreateFromBytes(der_cert.data(), der_cert.size()));
  EXPECT_TRUE(x509cert != NULL);
}

// Tests that the callback of a canceled request is never made.
TEST(ServerBoundCertServiceTest, CancelRequest) {
  scoped_ptr<ServerBoundCertService> service(
      new ServerBoundCertService(new DefaultServerBoundCertStore(NULL)));
  std::string origin("https://encrypted.google.com:443");
  SSLClientCertType type;
  std::string private_key_info, der_cert;
  int error;
  std::vector<uint8> types;
  types.push_back(CLIENT_CERT_ECDSA_SIGN);
  ServerBoundCertService::RequestHandle request_handle;

  error = service->GetDomainBoundCert(origin,
                                      types,
                                      &type,
                                      &private_key_info,
                                      &der_cert,
                                      base::Bind(&FailTest),
                                      &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  service->CancelRequest(request_handle);

  // Issue a few more requests to the worker pool and wait for their
  // completion, so that the task of the canceled request (which runs on a
  // worker thread) is likely to complete by the end of this test.
  TestCompletionCallback callback;
  for (int i = 0; i < 5; ++i) {
    error = service->GetDomainBoundCert(
        "https://foo" + std::string(1, (char) ('1' + i)),
        types,
        &type,
        &private_key_info,
        &der_cert,
        callback.callback(),
        &request_handle);
    EXPECT_EQ(ERR_IO_PENDING, error);
    EXPECT_TRUE(request_handle != NULL);
    error = callback.WaitForResult();
  }

  // Even though the original request was cancelled, the service will still
  // store the result, it just doesn't call the callback.
  EXPECT_EQ(6, service->cert_count());
}

TEST(ServerBoundCertServiceTest, Expiration) {
  ServerBoundCertStore* store = new DefaultServerBoundCertStore(NULL);
  base::Time now = base::Time::Now();
  store->SetServerBoundCert("good",
                            CLIENT_CERT_ECDSA_SIGN,
                            now,
                            now + base::TimeDelta::FromDays(1),
                            "a",
                            "b");
  store->SetServerBoundCert("expired",
                            CLIENT_CERT_ECDSA_SIGN,
                            now - base::TimeDelta::FromDays(2),
                            now - base::TimeDelta::FromDays(1),
                            "c",
                            "d");
  ServerBoundCertService service(store);
  EXPECT_EQ(2, service.cert_count());

  int error;
  std::vector<uint8> types;
  types.push_back(CLIENT_CERT_ECDSA_SIGN);
  TestCompletionCallback callback;
  ServerBoundCertService::RequestHandle request_handle;

  // Cert still valid - synchronous completion.
  SSLClientCertType type1;
  std::string private_key_info1, der_cert1;
  error = service.GetDomainBoundCert(
      "https://good", types, &type1, &private_key_info1, &der_cert1,
      callback.callback(), &request_handle);
  EXPECT_EQ(OK, error);
  EXPECT_TRUE(request_handle == NULL);
  EXPECT_EQ(2, service.cert_count());
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type1);
  EXPECT_STREQ("a", private_key_info1.c_str());
  EXPECT_STREQ("b", der_cert1.c_str());

  // Cert expired - New cert will be generated, asynchronous completion.
  SSLClientCertType type2;
  std::string private_key_info2, der_cert2;
  error = service.GetDomainBoundCert(
      "https://expired", types, &type2, &private_key_info2, &der_cert2,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle != NULL);
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(2, service.cert_count());
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type2);
  EXPECT_LT(1U, private_key_info2.size());
  EXPECT_LT(1U, der_cert2.size());
}

#endif  // !defined(USE_OPENSSL)

}  // namespace

}  // namespace net
