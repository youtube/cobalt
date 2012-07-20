// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/openssl_private_key_store.h"

#include <openssl/evp.h>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "crypto/openssl_util.h"
#include "net/android/network_library.h"

namespace net {

namespace {

class OpenSSLKeyStoreAndroid : public OpenSSLPrivateKeyStore {
 public:
  ~OpenSSLKeyStoreAndroid() {}

  // TODO(joth): Use the |url| to help identify this key to the user.
  // Currently Android has no UI to list these stored private keys (and no
  // API to associate a name with them), so this is a non-issue.
  virtual bool StorePrivateKey(const GURL& url, EVP_PKEY* pkey) {
    uint8* public_key = NULL;
    int public_len = i2d_PublicKey(pkey, &public_key);
    uint8* private_key = NULL;
    int private_len = i2d_PrivateKey(pkey, &private_key);

    bool ret = false;
    if (public_len && private_len) {
      ret = net::android::StoreKeyPair(public_key, public_len, private_key,
                                       private_len);
    }
    LOG_IF(ERROR, !ret) << "StorePrivateKey failed. pub len = " << public_len
                        << " priv len = " << private_len;
    OPENSSL_free(public_key);
    OPENSSL_free(private_key);
    return ret;
  }

  virtual EVP_PKEY* FetchPrivateKey(EVP_PKEY* pkey) {
    // TODO(joth): Implement when client authentication is required.
    NOTIMPLEMENTED();
    return NULL;
  }

  static OpenSSLKeyStoreAndroid* GetInstance() {
    return Singleton<OpenSSLKeyStoreAndroid>::get();
  }

 private:
  OpenSSLKeyStoreAndroid() {}
  friend struct DefaultSingletonTraits<OpenSSLKeyStoreAndroid>;

  DISALLOW_COPY_AND_ASSIGN(OpenSSLKeyStoreAndroid);
};

}  // namespace

OpenSSLPrivateKeyStore* OpenSSLPrivateKeyStore::GetInstance() {
  return OpenSSLKeyStoreAndroid::GetInstance();
}

}  // namespace net
