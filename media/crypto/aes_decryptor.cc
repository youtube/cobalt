// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/crypto/aes_decryptor.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/crypto/decryptor_client.h"

namespace media {

// TODO(xhwang): Get real IV from frames.
static const char kInitialCounter[] = "0000000000000000";

uint32 AesDecryptor::next_session_id_ = 1;

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

AesDecryptor::AesDecryptor(DecryptorClient* client)
    : client_(client) {
}

AesDecryptor::~AesDecryptor() {
  STLDeleteValues(&key_map_);
}

void AesDecryptor::GenerateKeyRequest(const std::string& key_system,
                                      const uint8* init_data,
                                      int init_data_length) {
  std::string session_id_string(base::UintToString(next_session_id_++));

  // For now, just fire the event with the |init_data| as the request.
  int message_length = init_data_length;
  scoped_array<uint8> message(new uint8[message_length]);
  memcpy(message.get(), init_data, message_length);

  client_->KeyMessage(key_system, session_id_string,
                      message.Pass(), message_length, "");
}

void AesDecryptor::AddKey(const std::string& key_system,
                          const uint8* key,
                          int key_length,
                          const uint8* init_data,
                          int init_data_length,
                          const std::string& session_id) {
  CHECK(key);
  CHECK_GT(key_length, 0);

  // TODO(xhwang): Add |session_id| check after we figure out how:
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=16550

  const int kSupportedKeyLength = 16;  // 128-bit key.
  if (key_length != kSupportedKeyLength) {
    DVLOG(1) << "Invalid key length: " << key_length;
    client_->KeyError(key_system, session_id, kUnknownError, 0);
    return;
  }

  // TODO(xhwang): Fix the decryptor to accept no |init_data|. See
  // http://crbug.com/123265. Until then, ensure a non-empty value is passed.
  static const uint8 kDummyInitData[1] = { 0 };
  if (!init_data) {
    init_data = kDummyInitData;
    init_data_length = arraysize(kDummyInitData);
  }

  // TODO(xhwang): For now, use |init_data| for key ID. Make this more spec
  // compliant later (http://crbug.com/123262, http://crbug.com/123265).
  std::string key_id_string(reinterpret_cast<const char*>(init_data),
                            init_data_length);
  std::string key_string(reinterpret_cast<const char*>(key) , key_length);
  crypto::SymmetricKey* symmetric_key = crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES, key_string);
  if (!symmetric_key) {
    DVLOG(1) << "Could not import key.";
    client_->KeyError(key_system, session_id, kUnknownError, 0);
    return;
  }

  {
    base::AutoLock auto_lock(key_map_lock_);
    KeyMap::iterator found = key_map_.find(key_id_string);
    if (found != key_map_.end()) {
      delete found->second;
      key_map_.erase(found);
    }
    key_map_[key_id_string] = symmetric_key;
  }

  client_->KeyAdded(key_system, session_id);
}

void AesDecryptor::CancelKeyRequest(const std::string& key_system,
                                    const std::string& session_id) {
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
    base::AutoLock auto_lock(key_map_lock_);
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
