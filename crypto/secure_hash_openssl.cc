// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/secure_hash.h"

#include <openssl/ssl.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "crypto/openssl_util.h"

namespace crypto {

namespace {

class SecureHashSHA256OpenSSL : public SecureHash {
 public:
  SecureHashSHA256OpenSSL() {
    SHA256_Init(&ctx_);
  }

  virtual ~SecureHashSHA256OpenSSL() {
    OPENSSL_cleanse(&ctx_, sizeof(ctx_));
  }

  virtual void Update(const void* input, size_t len) {
    SHA256_Update(&ctx_, static_cast<const unsigned char*>(input), len);
  }

  virtual void Finish(void* output, size_t len) {
    ScopedOpenSSLSafeSizeBuffer<SHA256_DIGEST_LENGTH> result(
        static_cast<unsigned char*>(output), len);
    SHA256_Final(result.safe_buffer(), &ctx_);
  }

 private:
  SHA256_CTX ctx_;
};

}  // namespace

SecureHash* SecureHash::Create(Algorithm algorithm) {
  switch (algorithm) {
    case SHA256:
      return new SecureHashSHA256OpenSSL();
    default:
      NOTIMPLEMENTED();
      return NULL;
  }
}

}  // namespace crypto
