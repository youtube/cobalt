// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/encryptor.h"

#include <cryptohi.h>
#include <vector>

#include "base/logging.h"
#include "crypto/nss_util.h"
#include "crypto/symmetric_key.h"

namespace crypto {

Encryptor::Encryptor()
    : key_(NULL),
      mode_(CBC) {
  EnsureNSSInit();
}

Encryptor::~Encryptor() {
}

bool Encryptor::Init(SymmetricKey* key, Mode mode, const std::string& iv) {
  DCHECK(key);
  DCHECK_EQ(CBC, mode);

  key_ = key;
  mode_ = mode;

  if (iv.size() != AES_BLOCK_SIZE)
    return false;

  slot_.reset(PK11_GetBestSlot(CKM_AES_CBC_PAD, NULL));
  if (!slot_.get())
    return false;

  SECItem iv_item;
  iv_item.type = siBuffer;
  iv_item.data = reinterpret_cast<unsigned char*>(
      const_cast<char *>(iv.data()));
  iv_item.len = iv.size();

  param_.reset(PK11_ParamFromIV(CKM_AES_CBC_PAD, &iv_item));
  if (!param_.get())
    return false;

  return true;
}

bool Encryptor::Encrypt(const std::string& plaintext, std::string* ciphertext) {
  ScopedPK11Context context(PK11_CreateContextBySymKey(CKM_AES_CBC_PAD,
                                                       CKA_ENCRYPT,
                                                       key_->key(),
                                                       param_.get()));
  if (!context.get())
    return false;

  size_t ciphertext_len = plaintext.size() + AES_BLOCK_SIZE;
  std::vector<unsigned char> buffer(ciphertext_len);

  int op_len;
  SECStatus rv = PK11_CipherOp(context.get(),
                               &buffer[0],
                               &op_len,
                               ciphertext_len,
                               reinterpret_cast<unsigned char*>(
                                   const_cast<char*>(plaintext.data())),
                               plaintext.size());
  if (SECSuccess != rv)
    return false;

  unsigned int digest_len;
  rv = PK11_DigestFinal(context.get(),
                        &buffer[op_len],
                        &digest_len,
                        ciphertext_len - op_len);
  if (SECSuccess != rv)
    return false;

  ciphertext->assign(reinterpret_cast<char *>(&buffer[0]),
                     op_len + digest_len);
  return true;
}

bool Encryptor::Decrypt(const std::string& ciphertext, std::string* plaintext) {
  if (ciphertext.empty())
    return false;

  ScopedPK11Context context(PK11_CreateContextBySymKey(CKM_AES_CBC_PAD,
                                                       CKA_DECRYPT,
                                                       key_->key(),
                                                       param_.get()));
  if (!context.get())
    return false;

  size_t plaintext_len = ciphertext.size();
  std::vector<unsigned char> buffer(plaintext_len);

  int op_len;
  SECStatus rv = PK11_CipherOp(context.get(),
                               &buffer[0],
                               &op_len,
                               plaintext_len,
                               reinterpret_cast<unsigned char*>(
                                   const_cast<char*>(ciphertext.data())),
                               ciphertext.size());
  if (SECSuccess != rv)
    return false;

  unsigned int digest_len;
  rv = PK11_DigestFinal(context.get(),
                        &buffer[op_len],
                        &digest_len,
                        plaintext_len - op_len);
  if (SECSuccess != rv)
    return false;

  plaintext->assign(reinterpret_cast<char *>(&buffer[0]),
                    op_len + digest_len);
  return true;
}

}  // namespace crypto
