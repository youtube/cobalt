// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/openssl_private_key_store.h"

#include <openssl/evp.h>
#include <openssl/x509.h>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "crypto/openssl_util.h"
#include "net/android/network_library.h"

namespace net {

namespace {

class OpenSSLKeyStoreAndroid : public OpenSSLPrivateKeyStore {
 public:
  ~OpenSSLKeyStoreAndroid() {}

  virtual bool StorePrivateKey(const GURL& url, EVP_PKEY* pkey) {
    // Always clear openssl errors on exit.
    crypto::OpenSSLErrStackTracer err_trace(FROM_HERE);

    // Important: Do not use i2d_PublicKey() here, which returns data in
    // PKCS#1 format, use i2d_PUBKEY() which returns it as DER-encoded
    // SubjectPublicKeyInfo (X.509), as expected by the platform.
    unsigned char* public_key = NULL;
    int public_len = i2d_PUBKEY(pkey, &public_key);

    // Important: Do not use i2d_PrivateKey() here, it returns data
    // in a format that is incompatible with what the platform expects.
    unsigned char* private_key = NULL;
    int private_len = 0;
    crypto::ScopedOpenSSL<PKCS8_PRIV_KEY_INFO,
        PKCS8_PRIV_KEY_INFO_free> pkcs8(EVP_PKEY2PKCS8(pkey));
    if (pkcs8.get() != NULL) {
      private_len = i2d_PKCS8_PRIV_KEY_INFO(pkcs8.get(), &private_key);
    }
    bool ret = false;
    if (public_len > 0 && private_len > 0) {
      ret = net::android::StoreKeyPair(
            static_cast<const uint8*>(public_key), public_len,
            static_cast<const uint8*>(private_key), private_len);
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
    // Leak the OpenSSL key store as it is used from a non-joinable worker
    // thread that may still be running at shutdown.
    return Singleton<
        OpenSSLKeyStoreAndroid,
        OpenSSLKeyStoreAndroidLeakyTraits>::get();
  }

 private:
  friend struct DefaultSingletonTraits<OpenSSLKeyStoreAndroid>;
  typedef LeakySingletonTraits<OpenSSLKeyStoreAndroid>
      OpenSSLKeyStoreAndroidLeakyTraits;

  OpenSSLKeyStoreAndroid() {}

  DISALLOW_COPY_AND_ASSIGN(OpenSSLKeyStoreAndroid);
};

}  // namespace

OpenSSLPrivateKeyStore* OpenSSLPrivateKeyStore::GetInstance() {
  return OpenSSLKeyStoreAndroid::GetInstance();
}

}  // namespace net
