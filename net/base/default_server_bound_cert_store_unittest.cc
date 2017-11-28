// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/default_server_bound_cert_store.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class MockPersistentStore
    : public DefaultServerBoundCertStore::PersistentStore {
 public:
  MockPersistentStore();

  // DefaultServerBoundCertStore::PersistentStore implementation.
  virtual bool Load(
      std::vector<DefaultServerBoundCertStore::ServerBoundCert*>* certs)
          override;
  virtual void AddServerBoundCert(
      const DefaultServerBoundCertStore::ServerBoundCert& cert) override;
  virtual void DeleteServerBoundCert(
      const DefaultServerBoundCertStore::ServerBoundCert& cert) override;
  virtual void SetForceKeepSessionState() override;
  virtual void Flush(const base::Closure& completion_task) override;

 protected:
  virtual ~MockPersistentStore();

 private:
  typedef std::map<std::string, DefaultServerBoundCertStore::ServerBoundCert>
      ServerBoundCertMap;

  ServerBoundCertMap origin_certs_;
};

MockPersistentStore::MockPersistentStore() {}

bool MockPersistentStore::Load(
    std::vector<DefaultServerBoundCertStore::ServerBoundCert*>* certs) {
  ServerBoundCertMap::iterator it;

  for (it = origin_certs_.begin(); it != origin_certs_.end(); ++it) {
    certs->push_back(
        new DefaultServerBoundCertStore::ServerBoundCert(it->second));
  }

  return true;
}

void MockPersistentStore::AddServerBoundCert(
    const DefaultServerBoundCertStore::ServerBoundCert& cert) {
  origin_certs_[cert.server_identifier()] = cert;
}

void MockPersistentStore::DeleteServerBoundCert(
    const DefaultServerBoundCertStore::ServerBoundCert& cert) {
  origin_certs_.erase(cert.server_identifier());
}

void MockPersistentStore::SetForceKeepSessionState() {}

void MockPersistentStore::Flush(const base::Closure& completion_task) {
  NOTREACHED();
}

MockPersistentStore::~MockPersistentStore() {}

TEST(DefaultServerBoundCertStoreTest, TestLoading) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);

  persistent_store->AddServerBoundCert(
      DefaultServerBoundCertStore::ServerBoundCert(
          "google.com",
          CLIENT_CERT_RSA_SIGN,
          base::Time(),
          base::Time(),
          "a", "b"));
  persistent_store->AddServerBoundCert(
      DefaultServerBoundCertStore::ServerBoundCert(
          "verisign.com",
          CLIENT_CERT_ECDSA_SIGN,
          base::Time(),
          base::Time(),
          "c", "d"));

  // Make sure certs load properly.
  DefaultServerBoundCertStore store(persistent_store.get());
  EXPECT_EQ(2, store.GetCertCount());
  store.SetServerBoundCert(
      "verisign.com",
      CLIENT_CERT_RSA_SIGN,
      base::Time(),
      base::Time(),
      "e", "f");
  EXPECT_EQ(2, store.GetCertCount());
  store.SetServerBoundCert(
      "twitter.com",
      CLIENT_CERT_RSA_SIGN,
      base::Time(),
      base::Time(),
      "g", "h");
  EXPECT_EQ(3, store.GetCertCount());
}

TEST(DefaultServerBoundCertStoreTest, TestSettingAndGetting) {
  DefaultServerBoundCertStore store(NULL);
  SSLClientCertType type;
  base::Time creation_time;
  base::Time expiration_time;
  std::string private_key, cert;
  EXPECT_EQ(0, store.GetCertCount());
  EXPECT_FALSE(store.GetServerBoundCert("verisign.com",
                                        &type,
                                        &creation_time,
                                        &expiration_time,
                                        &private_key,
                                        &cert));
  EXPECT_TRUE(private_key.empty());
  EXPECT_TRUE(cert.empty());
  store.SetServerBoundCert(
      "verisign.com",
      CLIENT_CERT_RSA_SIGN,
      base::Time::FromInternalValue(123),
      base::Time::FromInternalValue(456),
      "i", "j");
  EXPECT_TRUE(store.GetServerBoundCert("verisign.com",
                                       &type,
                                       &creation_time,
                                       &expiration_time,
                                       &private_key,
                                       &cert));
  EXPECT_EQ(CLIENT_CERT_RSA_SIGN, type);
  EXPECT_EQ(123, creation_time.ToInternalValue());
  EXPECT_EQ(456, expiration_time.ToInternalValue());
  EXPECT_EQ("i", private_key);
  EXPECT_EQ("j", cert);
}

TEST(DefaultServerBoundCertStoreTest, TestDuplicateCerts) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultServerBoundCertStore store(persistent_store.get());

  SSLClientCertType type;
  base::Time creation_time;
  base::Time expiration_time;
  std::string private_key, cert;
  EXPECT_EQ(0, store.GetCertCount());
  store.SetServerBoundCert(
      "verisign.com",
      CLIENT_CERT_RSA_SIGN,
      base::Time::FromInternalValue(123),
      base::Time::FromInternalValue(1234),
      "a", "b");
  store.SetServerBoundCert(
      "verisign.com",
      CLIENT_CERT_ECDSA_SIGN,
      base::Time::FromInternalValue(456),
      base::Time::FromInternalValue(4567),
      "c", "d");

  EXPECT_EQ(1, store.GetCertCount());
  EXPECT_TRUE(store.GetServerBoundCert("verisign.com",
                                       &type,
                                       &creation_time,
                                       &expiration_time,
                                       &private_key,
                                       &cert));
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, type);
  EXPECT_EQ(456, creation_time.ToInternalValue());
  EXPECT_EQ(4567, expiration_time.ToInternalValue());
  EXPECT_EQ("c", private_key);
  EXPECT_EQ("d", cert);
}

TEST(DefaultServerBoundCertStoreTest, TestDeleteAll) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultServerBoundCertStore store(persistent_store.get());

  EXPECT_EQ(0, store.GetCertCount());
  store.SetServerBoundCert(
      "verisign.com",
      CLIENT_CERT_RSA_SIGN,
      base::Time(),
      base::Time(),
      "a", "b");
  store.SetServerBoundCert(
      "google.com",
      CLIENT_CERT_RSA_SIGN,
      base::Time(),
      base::Time(),
      "c", "d");
  store.SetServerBoundCert(
      "harvard.com",
      CLIENT_CERT_RSA_SIGN,
      base::Time(),
      base::Time(),
      "e", "f");

  EXPECT_EQ(3, store.GetCertCount());
  store.DeleteAll();
  EXPECT_EQ(0, store.GetCertCount());
}

TEST(DefaultServerBoundCertStoreTest, TestDelete) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultServerBoundCertStore store(persistent_store.get());

  SSLClientCertType type;
  base::Time creation_time;
  base::Time expiration_time;
  std::string private_key, cert;
  EXPECT_EQ(0, store.GetCertCount());
  store.SetServerBoundCert(
      "verisign.com",
      CLIENT_CERT_RSA_SIGN,
      base::Time(),
      base::Time(),
      "a", "b");
  store.SetServerBoundCert(
      "google.com",
      CLIENT_CERT_ECDSA_SIGN,
      base::Time(),
      base::Time(),
      "c", "d");

  EXPECT_EQ(2, store.GetCertCount());
  store.DeleteServerBoundCert("verisign.com");
  EXPECT_EQ(1, store.GetCertCount());
  EXPECT_FALSE(store.GetServerBoundCert("verisign.com",
                                        &type,
                                        &creation_time,
                                        &expiration_time,
                                        &private_key,
                                        &cert));
  EXPECT_TRUE(store.GetServerBoundCert("google.com",
                                       &type,
                                       &creation_time,
                                       &expiration_time,
                                       &private_key,
                                       &cert));
  store.DeleteServerBoundCert("google.com");
  EXPECT_EQ(0, store.GetCertCount());
  EXPECT_FALSE(store.GetServerBoundCert("google.com",
                                        &type,
                                        &creation_time,
                                        &expiration_time,
                                        &private_key,
                                        &cert));
}

TEST(DefaultServerBoundCertStoreTest, TestGetAll) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultServerBoundCertStore store(persistent_store.get());

  EXPECT_EQ(0, store.GetCertCount());
  store.SetServerBoundCert(
      "verisign.com",
      CLIENT_CERT_RSA_SIGN,
      base::Time(),
      base::Time(),
      "a", "b");
  store.SetServerBoundCert(
      "google.com",
      CLIENT_CERT_ECDSA_SIGN,
      base::Time(),
      base::Time(),
      "c", "d");
  store.SetServerBoundCert(
      "harvard.com",
      CLIENT_CERT_RSA_SIGN,
      base::Time(),
      base::Time(),
      "e", "f");
  store.SetServerBoundCert(
      "mit.com",
      CLIENT_CERT_RSA_SIGN,
      base::Time(),
      base::Time(),
      "g", "h");

  EXPECT_EQ(4, store.GetCertCount());
  ServerBoundCertStore::ServerBoundCertList certs;
  store.GetAllServerBoundCerts(&certs);
  EXPECT_EQ(4u, certs.size());
}

TEST(DefaultServerBoundCertStoreTest, TestInitializeFrom) {
  scoped_refptr<MockPersistentStore> persistent_store(new MockPersistentStore);
  DefaultServerBoundCertStore store(persistent_store.get());

  store.SetServerBoundCert(
      "preexisting.com",
      CLIENT_CERT_RSA_SIGN,
      base::Time(),
      base::Time(),
      "a", "b");
  store.SetServerBoundCert(
      "both.com",
      CLIENT_CERT_ECDSA_SIGN,
      base::Time(),
      base::Time(),
      "c", "d");
  EXPECT_EQ(2, store.GetCertCount());

  ServerBoundCertStore::ServerBoundCertList source_certs;
  source_certs.push_back(ServerBoundCertStore::ServerBoundCert(
      "both.com",
      CLIENT_CERT_RSA_SIGN,
      base::Time(),
      base::Time(),
      // Key differs from above to test that existing entries are overwritten.
      "e", "f"));
  source_certs.push_back(ServerBoundCertStore::ServerBoundCert(
      "copied.com",
      CLIENT_CERT_RSA_SIGN,
      base::Time(),
      base::Time(),
      "g", "h"));
  store.InitializeFrom(source_certs);
  EXPECT_EQ(3, store.GetCertCount());

  ServerBoundCertStore::ServerBoundCertList certs;
  store.GetAllServerBoundCerts(&certs);
  ASSERT_EQ(3u, certs.size());

  ServerBoundCertStore::ServerBoundCertList::iterator cert = certs.begin();
  EXPECT_EQ("both.com", cert->server_identifier());
  EXPECT_EQ("e", cert->private_key());

  ++cert;
  EXPECT_EQ("copied.com", cert->server_identifier());
  EXPECT_EQ("g", cert->private_key());

  ++cert;
  EXPECT_EQ("preexisting.com", cert->server_identifier());
  EXPECT_EQ("a", cert->private_key());
}

}  // namespace net
