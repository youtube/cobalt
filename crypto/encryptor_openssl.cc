// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/encryptor.h"

#include <openssl/aes.h>
#include <openssl/evp.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "crypto/openssl_util.h"
#include "crypto/symmetric_key.h"

namespace crypto {

namespace {

const EVP_CIPHER* GetCipherForKey(SymmetricKey* key) {
  switch (key->key().length()) {
    case 16: return EVP_aes_128_cbc();
    case 24: return EVP_aes_192_cbc();
    case 32: return EVP_aes_256_cbc();
    default: return NULL;
  }
}

// On destruction this class will cleanup the ctx, and also clear the OpenSSL
// ERR stack as a convenience.
class ScopedCipherCTX {
 public:
  explicit ScopedCipherCTX() {
    EVP_CIPHER_CTX_init(&ctx_);
  }
  ~ScopedCipherCTX() {
    EVP_CIPHER_CTX_cleanup(&ctx_);
    ClearOpenSSLERRStack(FROM_HERE);
  }
  EVP_CIPHER_CTX* get() { return &ctx_; }

 private:
  EVP_CIPHER_CTX ctx_;
};

}  // namespace

Encryptor::Encryptor()
    : key_(NULL),
      mode_(CBC) {
}

Encryptor::~Encryptor() {
}

bool Encryptor::Init(SymmetricKey* key, Mode mode, const std::string& iv) {
  DCHECK(key);
  DCHECK_EQ(CBC, mode);

  EnsureOpenSSLInit();
  if (iv.size() != AES_BLOCK_SIZE)
    return false;

  if (GetCipherForKey(key) == NULL)
    return false;

  key_ = key;
  mode_ = mode;
  iv_ = iv;
  return true;
}

bool Encryptor::Encrypt(const std::string& plaintext, std::string* ciphertext) {
  return Crypt(true, plaintext, ciphertext);
}

bool Encryptor::Decrypt(const std::string& ciphertext, std::string* plaintext) {
  return Crypt(false, ciphertext, plaintext);
}

bool Encryptor::Crypt(bool do_encrypt,
                      const std::string& input,
                      std::string* output) {
  DCHECK(key_);  // Must call Init() before En/De-crypt.
  // Work on the result in a local variable, and then only transfer it to
  // |output| on success to ensure no partial data is returned.
  std::string result;
  output->swap(result);

  const EVP_CIPHER* cipher = GetCipherForKey(key_);
  DCHECK(cipher);  // Already handled in Init();

  const std::string& key = key_->key();
  DCHECK_EQ(EVP_CIPHER_iv_length(cipher), static_cast<int>(iv_.length()));
  DCHECK_EQ(EVP_CIPHER_key_length(cipher), static_cast<int>(key.length()));

  ScopedCipherCTX ctx;
  if (!EVP_CipherInit_ex(ctx.get(), cipher, NULL,
                         reinterpret_cast<const uint8*>(key.data()),
                         reinterpret_cast<const uint8*>(iv_.data()),
                         do_encrypt))
    return false;

  // When encrypting, add another block size of space to allow for any padding.
  const size_t output_size = input.size() + (do_encrypt ? iv_.size() : 0);
  uint8* out_ptr = reinterpret_cast<uint8*>(WriteInto(&result,
                                                      output_size + 1));
  int out_len;
  if (!EVP_CipherUpdate(ctx.get(), out_ptr, &out_len,
                        reinterpret_cast<const uint8*>(input.data()),
                        input.length()))
    return false;

  // Write out the final block plus padding (if any) to the end of the data
  // just written.
  int tail_len;
  if (!EVP_CipherFinal_ex(ctx.get(), out_ptr + out_len, &tail_len))
    return false;

  out_len += tail_len;
  DCHECK_LE(out_len, static_cast<int>(output_size));
  result.resize(out_len);

  output->swap(result);
  return true;
}

}  // namespace crypto
