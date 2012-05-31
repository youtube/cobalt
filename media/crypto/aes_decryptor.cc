// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/crypto/aes_decryptor.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_piece.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"

namespace media {

// TODO(xhwang): Get real IV from frames.
static const char kInitialCounter[] = "0000000000000000";

// Decrypt |input| using |key|.
// Return a DecoderBuffer with the decrypted data if decryption succeeded.
// Return NULL if decryption failed.
static scoped_refptr<DecoderBuffer> DecryptData(const DecoderBuffer& input,
                                                crypto::SymmetricKey* key) {
  CHECK(input.GetDataSize());
  CHECK(key);

  // Initialize encryption data.
  // The IV must be exactly as long as the cipher block size.
  crypto::Encryptor encryptor;
  if (!encryptor.Init(key, crypto::Encryptor::CBC, kInitialCounter)) {
    DVLOG(1) << "Could not initialize encryptor.";
    return NULL;
  }

  std::string decrypted_text;
  base::StringPiece encrypted_text(
      reinterpret_cast<const char*>(input.GetData()),
      input.GetDataSize());
  if (!encryptor.Decrypt(encrypted_text, &decrypted_text)) {
    DVLOG(1) << "Could not decrypt data.";
    return NULL;
  }

  // TODO(xhwang): Find a way to avoid this data copy.
  return DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8*>(decrypted_text.data()),
      decrypted_text.size());
}

AesDecryptor::AesDecryptor() {}

AesDecryptor::~AesDecryptor() {
  STLDeleteValues(&key_map_);
}

void AesDecryptor::AddKey(const uint8* key_id, int key_id_size,
                          const uint8* key, int key_size) {
  CHECK(key_id && key);
  CHECK_GT(key_id_size, 0);
  CHECK_GT(key_size, 0);

  std::string key_id_string(reinterpret_cast<const char*>(key_id), key_id_size);
  std::string key_string(reinterpret_cast<const char*>(key) , key_size);

  crypto::SymmetricKey* symmetric_key = crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES, key_string);
  if (!symmetric_key) {
    DVLOG(1) << "Could not import key.";
    return;
  }

  base::AutoLock auto_lock(lock_);
  KeyMap::iterator found = key_map_.find(key_id_string);
  if (found != key_map_.end()) {
    delete found->second;
    key_map_.erase(found);
  }
  key_map_[key_id_string] = symmetric_key;
}

scoped_refptr<DecoderBuffer> AesDecryptor::Decrypt(
    const scoped_refptr<DecoderBuffer>& encrypted) {
  CHECK(encrypted->GetDecryptConfig());
  const uint8* key_id = encrypted->GetDecryptConfig()->key_id();
  const int key_id_size = encrypted->GetDecryptConfig()->key_id_size();

  // TODO(xhwang): Avoid always constructing a string with StringPiece?
  std::string key_id_string(reinterpret_cast<const char*>(key_id), key_id_size);

  crypto::SymmetricKey* key = NULL;
  {
    base::AutoLock auto_lock(lock_);
    KeyMap::const_iterator found = key_map_.find(key_id_string);
    if (found == key_map_.end()) {
      DVLOG(1) << "Could not find a matching key for given key ID.";
      return NULL;
    }
    key = found->second;
  }

  scoped_refptr<DecoderBuffer> decrypted = DecryptData(*encrypted, key);

  if (decrypted) {
    decrypted->SetTimestamp(encrypted->GetTimestamp());
    decrypted->SetDuration(encrypted->GetDuration());
  }

  return decrypted;
}

}  // namespace media
