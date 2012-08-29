// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/crypto/aes_decryptor.h"

#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "crypto/encryptor.h"
#include "crypto/hmac.h"
#include "crypto/symmetric_key.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/decryptor_client.h"

namespace media {

// The size is from the WebM encrypted specification. Current encrypted WebM
// request for comments specification is here
// http://wiki.webmproject.org/encryption/webm-encryption-rfc
static const int kWebmSha1DigestSize = 20;
static const char kWebmHmacSeed[] = "hmac-key";
static const char kWebmEncryptionSeed[] = "encryption-key";

uint32 AesDecryptor::next_session_id_ = 1;

// Derives a key using SHA1 HMAC. |secret| is the base secret to derive
// the key from. |seed| is the known message to the HMAC algorithm. |key_size|
// is how many bytes are returned in the key. Returns a string containing the
// key on success. Returns an empty string on failure.
static std::string DeriveKey(const base::StringPiece& secret,
                             const base::StringPiece& seed,
                             int key_size) {
  CHECK(!secret.empty());
  CHECK(!seed.empty());
  CHECK_GT(key_size, 0);

  crypto::HMAC hmac(crypto::HMAC::SHA1);
  if (!hmac.Init(secret)) {
    DVLOG(1) << "Could not initialize HMAC with secret data.";
    return std::string();
  }

  scoped_array<uint8> calculated_hmac(new uint8[hmac.DigestLength()]);
  if (!hmac.Sign(seed, calculated_hmac.get(), hmac.DigestLength())) {
    DVLOG(1) << "Could not calculate HMAC.";
    return std::string();
  }

  return std::string(reinterpret_cast<const char*>(calculated_hmac.get()),
                     key_size);
}

// Checks data in |input| matches the HMAC in |input|. The check is using the
// SHA1 algorithm. |hmac_key| is the key of the HMAC algorithm. Returns true if
// the integrity check passes.
static bool CheckData(const DecoderBuffer& input,
                      const base::StringPiece& hmac_key) {
  CHECK(input.GetDataSize());
  CHECK(input.GetDecryptConfig());
  CHECK_GT(input.GetDecryptConfig()->checksum().size(), 0u);
  CHECK(!hmac_key.empty());

  crypto::HMAC hmac(crypto::HMAC::SHA1);
  if (!hmac.Init(hmac_key))
    return false;

  // The component that initializes |input.GetDecryptConfig()| is responsible
  // for checking that |input.GetDecryptConfig()->checksum_size()| matches
  // what is defined by the format.

  // Here, check that checksum size is not greater than the hash
  // algorithm's digest length.
  DCHECK_LE(input.GetDecryptConfig()->checksum().size(),
            hmac.DigestLength());

  base::StringPiece data_to_check(
      reinterpret_cast<const char*>(input.GetData()), input.GetDataSize());

  return hmac.VerifyTruncated(data_to_check,
                              input.GetDecryptConfig()->checksum());
}

enum ClearBytesBufferSel {
  kSrcContainsClearBytes,
  kDstContainsClearBytes
};

static void CopySubsamples(const std::vector<SubsampleEntry>& subsamples,
                           const ClearBytesBufferSel sel,
                           const uint8* src,
                           uint8* dst) {
  for (size_t i = 0; i < subsamples.size(); i++) {
    const SubsampleEntry& subsample = subsamples[i];
    if (sel == kSrcContainsClearBytes) {
      src += subsample.clear_bytes;
    } else {
      dst += subsample.clear_bytes;
    }
    memcpy(dst, src, subsample.cypher_bytes);
    src += subsample.cypher_bytes;
    dst += subsample.cypher_bytes;
  }
}

// Decrypts |input| using |key|.  Returns a DecoderBuffer with the decrypted
// data if decryption succeeded or NULL if decryption failed.
static scoped_refptr<DecoderBuffer> DecryptData(const DecoderBuffer& input,
                                                crypto::SymmetricKey* key) {
  CHECK(input.GetDataSize());
  CHECK(input.GetDecryptConfig());
  CHECK(key);

  crypto::Encryptor encryptor;
  if (!encryptor.Init(key, crypto::Encryptor::CTR, "")) {
    DVLOG(1) << "Could not initialize decryptor.";
    return NULL;
  }

  DCHECK_EQ(input.GetDecryptConfig()->iv().size(),
            static_cast<size_t>(DecryptConfig::kDecryptionKeySize));
  if (!encryptor.SetCounter(input.GetDecryptConfig()->iv())) {
    DVLOG(1) << "Could not set counter block.";
    return NULL;
  }

  const int data_offset = input.GetDecryptConfig()->data_offset();
  const char* sample =
      reinterpret_cast<const char*>(input.GetData() + data_offset);
  int sample_size = input.GetDataSize() - data_offset;

  if (input.GetDecryptConfig()->subsamples().empty()) {
    std::string decrypted_text;
    base::StringPiece encrypted_text(sample, sample_size);
    if (!encryptor.Decrypt(encrypted_text, &decrypted_text)) {
      DVLOG(1) << "Could not decrypt data.";
      return NULL;
    }

    // TODO(xhwang): Find a way to avoid this data copy.
    return DecoderBuffer::CopyFrom(
        reinterpret_cast<const uint8*>(decrypted_text.data()),
        decrypted_text.size());
  }

  const std::vector<SubsampleEntry>& subsamples =
      input.GetDecryptConfig()->subsamples();

  int total_clear_size = 0;
  int total_encrypted_size = 0;
  for (size_t i = 0; i < subsamples.size(); i++) {
    total_clear_size += subsamples[i].clear_bytes;
    total_encrypted_size += subsamples[i].cypher_bytes;
  }
  if (total_clear_size + total_encrypted_size != sample_size) {
    DVLOG(1) << "Subsample sizes do not equal input size";
    return NULL;
  }

  // The encrypted portions of all subsamples must form a contiguous block,
  // such that an encrypted subsample that ends away from a block boundary is
  // immediately followed by the start of the next encrypted subsample. We
  // copy all encrypted subsamples to a contiguous buffer, decrypt them, then
  // copy the decrypted bytes over the encrypted bytes in the output.
  // TODO(strobe): attempt to reduce number of memory copies
  scoped_array<uint8> encrypted_bytes(new uint8[total_encrypted_size]);
  CopySubsamples(subsamples, kSrcContainsClearBytes,
                 reinterpret_cast<const uint8*>(sample), encrypted_bytes.get());

  base::StringPiece encrypted_text(
      reinterpret_cast<const char*>(encrypted_bytes.get()),
      total_encrypted_size);
  std::string decrypted_text;
  if (!encryptor.Decrypt(encrypted_text, &decrypted_text)) {
    DVLOG(1) << "Could not decrypt data.";
    return NULL;
  }

  scoped_refptr<DecoderBuffer> output = DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8*>(sample), sample_size);
  CopySubsamples(subsamples, kDstContainsClearBytes,
                 reinterpret_cast<const uint8*>(decrypted_text.data()),
                 output->GetWritableData());
  return output;
}

AesDecryptor::AesDecryptor(DecryptorClient* client)
    : client_(client) {
}

AesDecryptor::~AesDecryptor() {
  STLDeleteValues(&key_map_);
}

bool AesDecryptor::GenerateKeyRequest(const std::string& key_system,
                                      const uint8* init_data,
                                      int init_data_length) {
  std::string session_id_string(base::UintToString(next_session_id_++));

  // For now, just fire the event with the |init_data| as the request.
  int message_length = init_data_length;
  scoped_array<uint8> message(new uint8[message_length]);
  memcpy(message.get(), init_data, message_length);

  client_->KeyMessage(key_system, session_id_string,
                      message.Pass(), message_length, "");
  return true;
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
  if (key_length != DecryptConfig::kDecryptionKeySize) {
    DVLOG(1) << "Invalid key length: " << key_length;
    client_->KeyError(key_system, session_id, Decryptor::kUnknownError, 0);
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
  scoped_ptr<DecryptionKey> decryption_key(new DecryptionKey(key_string));
  if (!decryption_key.get()) {
    DVLOG(1) << "Could not create key.";
    client_->KeyError(key_system, session_id, Decryptor::kUnknownError, 0);
    return;
  }

  if (!decryption_key->Init()) {
    DVLOG(1) << "Could not initialize decryption key.";
    client_->KeyError(key_system, session_id, Decryptor::kUnknownError, 0);
    return;
  }

  SetKey(key_id_string, decryption_key.Pass());
  client_->KeyAdded(key_system, session_id);
}

void AesDecryptor::CancelKeyRequest(const std::string& key_system,
                                    const std::string& session_id) {
}

void AesDecryptor::Decrypt(const scoped_refptr<DecoderBuffer>& encrypted,
                           const DecryptCB& decrypt_cb) {
  CHECK(encrypted->GetDecryptConfig());
  const std::string& key_id = encrypted->GetDecryptConfig()->key_id();

  DecryptionKey* key = GetKey(key_id);
  if (!key) {
    DVLOG(1) << "Could not find a matching key for the given key ID.";
    decrypt_cb.Run(kNoKey, NULL);
    return;
  }

  int checksum_size = encrypted->GetDecryptConfig()->checksum().size();
  // According to the WebM encrypted specification, it is an open question
  // what should happen when a frame fails the integrity check.
  // http://wiki.webmproject.org/encryption/webm-encryption-rfc
  if (checksum_size > 0 &&
      !key->hmac_key().empty() &&
      !CheckData(*encrypted, key->hmac_key())) {
    DVLOG(1) << "Integrity check failed.";
    decrypt_cb.Run(kError, NULL);
    return;
  }

  scoped_refptr<DecoderBuffer> decrypted;
  // An empty iv string signals that the frame is unencrypted.
  if (encrypted->GetDecryptConfig()->iv().empty()) {
    int data_offset = encrypted->GetDecryptConfig()->data_offset();
    decrypted = DecoderBuffer::CopyFrom(encrypted->GetData() + data_offset,
                                        encrypted->GetDataSize() - data_offset);
  } else {
    // TODO(strobe): Currently, presence of checksum is used to indicate the use
    // of normal or WebM decryption keys. Consider a more explicit signaling
    // mechanism and the removal of the webm_decryption_key member.
    crypto::SymmetricKey* decryption_key = (checksum_size > 0) ?
        key->webm_decryption_key() : key->decryption_key();
    decrypted = DecryptData(*encrypted, decryption_key);
    if (!decrypted) {
      DVLOG(1) << "Decryption failed.";
      decrypt_cb.Run(kError, NULL);
      return;
    }
  }

  decrypted->SetTimestamp(encrypted->GetTimestamp());
  decrypted->SetDuration(encrypted->GetDuration());
  decrypt_cb.Run(kSuccess, decrypted);
}

void AesDecryptor::Stop() {
}

void AesDecryptor::SetKey(const std::string& key_id,
                          scoped_ptr<DecryptionKey> decryption_key) {
  base::AutoLock auto_lock(key_map_lock_);
  KeyMap::iterator found = key_map_.find(key_id);
  if (found != key_map_.end()) {
    delete found->second;
    key_map_.erase(found);
  }
  key_map_[key_id] = decryption_key.release();
}

AesDecryptor::DecryptionKey* AesDecryptor::GetKey(
    const std::string& key_id) const {
  base::AutoLock auto_lock(key_map_lock_);
  KeyMap::const_iterator found = key_map_.find(key_id);
  if (found == key_map_.end())
    return NULL;

  return found->second;
}

AesDecryptor::DecryptionKey::DecryptionKey(const std::string& secret)
    : secret_(secret) {
}

AesDecryptor::DecryptionKey::~DecryptionKey() {}

bool AesDecryptor::DecryptionKey::Init() {
  CHECK(!secret_.empty());
  decryption_key_.reset(crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES, secret_));
  if (!decryption_key_.get())
    return false;

  std::string raw_key = DeriveKey(secret_,
                                  kWebmEncryptionSeed,
                                  secret_.length());
  if (raw_key.empty())
    return false;

  webm_decryption_key_.reset(crypto::SymmetricKey::Import(
      crypto::SymmetricKey::AES, raw_key));
  if (!webm_decryption_key_.get())
    return false;

  hmac_key_ = DeriveKey(secret_, kWebmHmacSeed, kWebmSha1DigestSize);
  if (hmac_key_.empty())
    return false;

  return true;
}

}  // namespace media
