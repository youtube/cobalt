// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/player/crypto/proxy_decryptor.h"

// WebMediaPlayer.h needs uint8_t defined
// See: https://bugs.webkit.org/show_bug.cgi?id=92031
#include <stdint.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#if defined(_DEBUG)
#include "base/string_number_conversions.h"
#endif
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor_client.h"
#include "media/base/shell_buffer_factory.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/crypto/shell_decryptor_factory.h"
#include "media/player/crypto/key_systems.h"

namespace media {

ProxyDecryptor::ProxyDecryptor(DecryptorClient* decryptor_client)
    : client_(decryptor_client), destroy_soon_(false) {}

ProxyDecryptor::~ProxyDecryptor() {}

// TODO(xhwang): Support multiple decryptor notification request (e.g. from
// video and audio decoders). The current implementation is okay for the current
// media pipeline since we initialize audio and video decoders in sequence.
// But ProxyDecryptor should not depend on media pipeline's implementation
// detail.
void ProxyDecryptor::SetDecryptorReadyCB(
    const DecryptorReadyCB& decryptor_ready_cb) {
  base::AutoLock auto_lock(lock_);

  if (destroy_soon_) {
    decryptor_ready_cb.Run(NULL);
    return;
  }

  // Cancels the previous decryptor request.
  if (decryptor_ready_cb.is_null()) {
    if (!decryptor_ready_cb_.is_null())
      base::ResetAndReturn(&decryptor_ready_cb_).Run(NULL);
    return;
  }

  // Normal decryptor request.
  DCHECK(decryptor_ready_cb_.is_null());
  if (decryptor_) {
    decryptor_ready_cb.Run(decryptor_.get());
    return;
  }
  decryptor_ready_cb_ = decryptor_ready_cb;
}

void ProxyDecryptor::DestroySoon() {
  base::AutoLock auto_lock(lock_);

  destroy_soon_ = true;
  if (!decryptor_ready_cb_.is_null()) {
    base::ResetAndReturn(&decryptor_ready_cb_).Run(NULL);
  }
}

bool ProxyDecryptor::GenerateKeyRequest(const std::string& key_system,
                                        const std::string& type,
                                        const uint8* init_data,
                                        int init_data_length) {
  // We do not support run-time switching of decryptors. GenerateKeyRequest()
  // only creates a new decryptor when |decryptor_| is not initialized.
  DVLOG(1) << "GenerateKeyRequest: key_system = " << key_system;

  base::AutoLock auto_lock(lock_);

  if (!decryptor_) {
    decryptor_ = CreateDecryptor(key_system);
    if (!decryptor_) {
      client_->KeyError(key_system, "", Decryptor::kUnknownError, 0);
      return false;
    }
  }

  if (!decryptor_->GenerateKeyRequest(key_system, type, init_data,
                                      init_data_length)) {
    decryptor_.reset();
    return false;
  }

  if (!decryptor_ready_cb_.is_null())
    base::ResetAndReturn(&decryptor_ready_cb_).Run(decryptor_.get());

  return true;
}

void ProxyDecryptor::AddKey(const std::string& key_system,
                            const uint8* key,
                            int key_length,
                            const uint8* init_data,
                            int init_data_length,
                            const std::string& session_id) {
  DVLOG(1) << "AddKey()";
#if defined(_DEBUG)
  std::string hex = base::HexEncode(key, key_length);
  DLOG(INFO) << "DRM Key Response: " << hex;
#endif

  // WebMediaPlayerImpl ensures GenerateKeyRequest() has been called.
  decryptor_->AddKey(key_system, key, key_length, init_data, init_data_length,
                     session_id);
}

void ProxyDecryptor::CancelKeyRequest(const std::string& key_system,
                                      const std::string& session_id) {
  DVLOG(1) << "CancelKeyRequest()";

  // WebMediaPlayerImpl ensures GenerateKeyRequest() has been called.
  decryptor_->CancelKeyRequest(key_system, session_id);
}

void ProxyDecryptor::RegisterKeyAddedCB(StreamType stream_type,
                                        const KeyAddedCB& key_added_cb) {
  NOTREACHED() << "KeyAddedCB should not be registered with ProxyDecryptor.";
}

void ProxyDecryptor::Decrypt(StreamType stream_type,
                             const scoped_refptr<DecoderBuffer>& encrypted,
                             const DecryptCB& decrypt_cb) {
  NOTREACHED() << "ProxyDecryptor does not support decryption";
}

void ProxyDecryptor::CancelDecrypt(StreamType stream_type) {
  base::AutoLock auto_lock(lock_);

  if (decryptor_)
    decryptor_->CancelDecrypt(stream_type);
}

void ProxyDecryptor::InitializeAudioDecoder(
    scoped_ptr<AudioDecoderConfig> config,
    const DecoderInitCB& init_cb) {
  NOTREACHED() << "ProxyDecryptor does not support audio decoding";
}

void ProxyDecryptor::InitializeVideoDecoder(
    scoped_ptr<VideoDecoderConfig> config,
    const DecoderInitCB& init_cb) {
  NOTREACHED() << "ProxyDecryptor does not support video decoding";
}

void ProxyDecryptor::DecryptAndDecodeAudio(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const AudioDecodeCB& audio_decode_cb) {
  NOTREACHED() << "ProxyDecryptor does not support audio decoding";
}

void ProxyDecryptor::DecryptAndDecodeVideo(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const VideoDecodeCB& video_decode_cb) {
  NOTREACHED() << "ProxyDecryptor does not support video decoding";
}

void ProxyDecryptor::ResetDecoder(StreamType stream_type) {
  NOTREACHED() << "ProxyDecryptor does not support audio/video decoding";
}

void ProxyDecryptor::DeinitializeDecoder(StreamType stream_type) {
  NOTREACHED() << "ProxyDecryptor does not support audio/video decoding";
}

scoped_ptr<Decryptor> ProxyDecryptor::CreateDecryptor(
    const std::string& key_system) {
  DCHECK(client_);

  // lb_shell doesn't support ppapi or AesDecryptor, so we have our own
  // decryptor factory to handle cdm support.
  return scoped_ptr<Decryptor>(
      ShellDecryptorFactory::Create(key_system, client_));
}

}  // namespace media
