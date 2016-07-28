// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines an in-memory private key store, primarily used for testing.

#include <openssl/evp.h>

#include "net/base/openssl_private_key_store.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "net/base/x509_certificate.h"

namespace net {

namespace {

class OpenSSLMemoryKeyStore : public OpenSSLPrivateKeyStore {
 public:
  OpenSSLMemoryKeyStore() {}

  static OpenSSLMemoryKeyStore* GetInstance() {
    return Singleton<OpenSSLMemoryKeyStore>::get();
  }

  virtual ~OpenSSLMemoryKeyStore() {
    base::AutoLock lock(lock_);
    for (std::vector<EVP_PKEY*>::iterator it = keys_.begin();
         it != keys_.end(); ++it) {
      EVP_PKEY_free(*it);
    }
  }

  virtual bool StorePrivateKey(const GURL& url, EVP_PKEY* pkey) {
    CRYPTO_add(&pkey->references, 1, CRYPTO_LOCK_EVP_PKEY);
    base::AutoLock lock(lock_);
    keys_.push_back(pkey);
    return true;
  }

  virtual EVP_PKEY* FetchPrivateKey(EVP_PKEY* pkey) {
    base::AutoLock lock(lock_);
    for (std::vector<EVP_PKEY*>::iterator it = keys_.begin();
         it != keys_.end(); ++it) {
      if (EVP_PKEY_cmp(*it, pkey) == 1)
        return *it;
    }
    return NULL;
  }

 private:
  std::vector<EVP_PKEY*> keys_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(OpenSSLMemoryKeyStore);
};

}  // namespace

// static
OpenSSLPrivateKeyStore* OpenSSLPrivateKeyStore::GetInstance() {
  return OpenSSLMemoryKeyStore::GetInstance();
}

} // namespace net

