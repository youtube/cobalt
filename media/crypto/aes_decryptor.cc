// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/crypto/aes_decryptor.h"

#include <string>

#include "base/logging.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "media/base/buffers.h"
#include "media/base/data_buffer.h"
#include "media/base/decrypt_config.h"

namespace media {

static const char* kInitialCounter = "0000000000000000";

// Decrypts |input| using |raw_key|, which is the binary data for the decryption
// key.
// Return a scoped_refptr to a Buffer object with the decrypted data on success.
// Return a scoped_refptr to NULL if the data could not be decrypted.
// TODO(xhwang): Both the input and output are copied! Any performance concern?
static scoped_refptr<Buffer> DecryptData(const Buffer& input,
                                         const uint8* raw_key,
                                         int raw_key_size) {
  CHECK(raw_key && raw_key_size > 0);
  CHECK(input.GetDataSize());

  scoped_ptr<crypto::SymmetricKey> key(crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES,
      std::string(reinterpret_cast<const char*>(raw_key), raw_key_size)));
  if (!key.get()) {
    DVLOG(1) << "Could not import key.";
    return NULL;
  }

  // Initialize encryption data.
  // The IV must be exactly as long as the cipher block size.
  crypto::Encryptor encryptor;
  if (!encryptor.Init(key.get(), crypto::Encryptor::CBC, kInitialCounter)) {
    DVLOG(1) << "Could not initialize encryptor.";
    return NULL;
  }

  std::string decrypt_text;
  std::string encrypted_text(reinterpret_cast<const char*>(input.GetData()),
                             input.GetDataSize());
  if (!encryptor.Decrypt(encrypted_text, &decrypt_text)) {
    DVLOG(1) << "Could not decrypt data.";
    return NULL;
  }

  return DataBuffer::CopyFrom(
      reinterpret_cast<const uint8*>(decrypt_text.data()),
      decrypt_text.size());
}

AesDecryptor::AesDecryptor() {}

scoped_refptr<Buffer> AesDecryptor::Decrypt(
    const scoped_refptr<Buffer>& encrypted) {
  CHECK(encrypted->GetDecryptConfig());

  // For now, the key is the key ID.
  const uint8* key = encrypted->GetDecryptConfig()->key_id();
  int key_size = encrypted->GetDecryptConfig()->key_id_size();
  scoped_refptr<Buffer> decrypted = DecryptData(*encrypted, key, key_size);
  if (decrypted) {
    decrypted->SetTimestamp(encrypted->GetTimestamp());
    decrypted->SetDuration(encrypted->GetDuration());
  }

  return decrypted;
}

}  // namespace media
