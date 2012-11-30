// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/crypto/aes_decryptor.h"

#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/decryptor_client.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"

namespace media {

uint32 AesDecryptor::next_session_id_ = 1;

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
                                      const std::string& type,
                                      const uint8* init_data,
                                      int init_data_length) {
  std::string session_id_string(base::UintToString(next_session_id_++));

  if (!init_data || !init_data_length) {
    DVLOG(1) << "init_data required to generate a key request.";
    return false;
  }

  // For now, the AesDecryptor does not care about |key_system| and |type|;
  // just fire the event with the |init_data| as the request.
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

  if (!audio_key_added_cb_.is_null())
    audio_key_added_cb_.Run();

  if (!video_key_added_cb_.is_null())
    video_key_added_cb_.Run();

  client_->KeyAdded(key_system, session_id);
}

void AesDecryptor::CancelKeyRequest(const std::string& key_system,
                                    const std::string& session_id) {
}

void AesDecryptor::RegisterKeyAddedCB(StreamType stream_type,
                                      const KeyAddedCB& key_added_cb) {
  switch (stream_type) {
    case kAudio:
      audio_key_added_cb_ = key_added_cb;
      break;
    case kVideo:
      video_key_added_cb_ = key_added_cb;
      break;
    default:
      NOTREACHED();
  }
}

void AesDecryptor::Decrypt(StreamType stream_type,
                           const scoped_refptr<DecoderBuffer>& encrypted,
                           const DecryptCB& decrypt_cb) {
  CHECK(encrypted->GetDecryptConfig());
  const std::string& key_id = encrypted->GetDecryptConfig()->key_id();

  DecryptionKey* key = GetKey(key_id);
  if (!key) {
    DVLOG(1) << "Could not find a matching key for the given key ID.";
    decrypt_cb.Run(kNoKey, NULL);
    return;
  }

  scoped_refptr<DecoderBuffer> decrypted;
  // An empty iv string signals that the frame is unencrypted.
  if (encrypted->GetDecryptConfig()->iv().empty()) {
    int data_offset = encrypted->GetDecryptConfig()->data_offset();
    decrypted = DecoderBuffer::CopyFrom(encrypted->GetData() + data_offset,
                                        encrypted->GetDataSize() - data_offset);
  } else {
    crypto::SymmetricKey* decryption_key = key->decryption_key();
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

void AesDecryptor::CancelDecrypt(StreamType stream_type) {
  // Decrypt() calls the DecryptCB synchronously so there's nothing to cancel.
}

void AesDecryptor::InitializeAudioDecoder(scoped_ptr<AudioDecoderConfig> config,
                                          const DecoderInitCB& init_cb) {
  // AesDecryptor does not support audio decoding.
  init_cb.Run(false);
}

void AesDecryptor::InitializeVideoDecoder(scoped_ptr<VideoDecoderConfig> config,
                                          const DecoderInitCB& init_cb) {
  // AesDecryptor does not support video decoding.
  init_cb.Run(false);
}

void AesDecryptor::DecryptAndDecodeAudio(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const AudioDecodeCB& audio_decode_cb) {
  NOTREACHED() << "AesDecryptor does not support audio decoding";
}

void AesDecryptor::DecryptAndDecodeVideo(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const VideoDecodeCB& video_decode_cb) {
  NOTREACHED() << "AesDecryptor does not support video decoding";
}

void AesDecryptor::ResetDecoder(StreamType stream_type) {
  NOTREACHED() << "AesDecryptor does not support audio/video decoding";
}

void AesDecryptor::DeinitializeDecoder(StreamType stream_type) {
  NOTREACHED() << "AesDecryptor does not support audio/video decoding";
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
  return true;
}

}  // namespace media
