// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Definitions that allow for DRM support, common between Player and Decoder
// interfaces.

#include "media/crypto/starboard_decryptor.h"

#include "base/logging.h"
#include "media/base/decrypt_config.h"

namespace media {

namespace {

std::string ToString(const void* buffer, int size) {
  const char* char_buffer = reinterpret_cast<const char*>(buffer);
  return std::string(char_buffer, char_buffer + size);
}

}  // namespace

StarboardDecryptor::StarboardDecryptor(const char* key_system,
                                       DecryptorClient* decryptor_client)
    : client_(decryptor_client) {
  drm_system_ =
      SbDrmCreateSystem(key_system, this, KeyRequestFunc, KeyAddedFunc);
}

StarboardDecryptor::~StarboardDecryptor() {
  SbDrmDestroySystem(drm_system_);
}

bool StarboardDecryptor::GenerateKeyRequest(const std::string& key_system,
                                            const std::string& type,
                                            const uint8_t* init_data,
                                            int init_data_length) {
  if (drm_system_ == kSbDrmSystemInvalid) {
    return false;
  }
  if (key_system_.empty()) {
    key_system_ = key_system;
  }
  DCHECK_EQ(key_system_, key_system);
  SbDrmGenerateSessionUpdateRequest(drm_system_,
#if SB_API_VERSION >= 4
                                    0,  // No need to distinguish between
                                        // callbacks in EME 0.1b implementation.
#endif                                  // SB_API_VERSION >= 4
                                    type.c_str(), init_data, init_data_length);
  return true;
}

void StarboardDecryptor::AddKey(const std::string& key_system,
                                const uint8_t* key,
                                int key_length,
                                const uint8_t* init_data,
                                int init_data_length,
                                const std::string& session_id) {
  SbDrmUpdateSession(drm_system_, key, key_length, session_id.c_str(),
                     session_id.size());
}

void StarboardDecryptor::CancelKeyRequest(const std::string& key_system,
                                          const std::string& session_id) {
  SbDrmCloseSession(drm_system_, session_id.c_str(), session_id.size());
}

void StarboardDecryptor::RegisterKeyAddedCB(StreamType stream_type,
                                            const KeyAddedCB& key_added_cb) {
  NOTREACHED();
}

void StarboardDecryptor::Decrypt(StreamType stream_type,
                                 const scoped_refptr<DecoderBuffer>& encrypted,
                                 const DecryptCB& decrypt_cb) {
  decrypt_cb.Run(kSuccess, encrypted);
}

void StarboardDecryptor::CancelDecrypt(StreamType stream_type) {
  // no-op
}

void StarboardDecryptor::InitializeAudioDecoder(
    scoped_ptr<AudioDecoderConfig> config,
    const DecoderInitCB& init_cb) {
  // StarboardDecryptor does not support audio decoding.
  // This puts us in "decrypt only" mode.
  // Decoding is handled by the system default decoders.
  init_cb.Run(false);
}

void StarboardDecryptor::InitializeVideoDecoder(
    scoped_ptr<VideoDecoderConfig> config,
    const DecoderInitCB& init_cb) {
  // StarboardDecryptor does not support video decoding.
  // This puts us in "decrypt only" mode.
  // Decoding is handled by the system default decoders.
  init_cb.Run(false);
}

void StarboardDecryptor::DecryptAndDecodeAudio(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const AudioDecodeCB& audio_decode_cb) {
  NOTREACHED();
}

void StarboardDecryptor::DecryptAndDecodeVideo(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const VideoDecodeCB& video_decode_cb) {
  NOTREACHED();
}

void StarboardDecryptor::ResetDecoder(StreamType stream_type) {
  NOTREACHED();
}

void StarboardDecryptor::DeinitializeDecoder(StreamType stream_type) {
  NOTREACHED();
}

void StarboardDecryptor::OnKeyRequest(const void* session_id,
                                      int session_id_size,
                                      const void* content,
                                      int content_size,
                                      const char* url) {
  if (session_id) {
    client_->KeyMessage(key_system_, ToString(session_id, session_id_size),
                        ToString(content, content_size), url);
  } else {
    client_->KeyError(key_system_, "", Decryptor::kUnknownError, 0);
  }
}

void StarboardDecryptor::OnKeyAdded(const void* session_id,
                                    int session_id_length,
                                    bool succeeded) {
  std::string session(
      reinterpret_cast<const char*>(session_id),
      reinterpret_cast<const char*>(session_id) + session_id_length);

  if (succeeded) {
    client_->KeyAdded(key_system_, session);
  } else {
    client_->KeyError(key_system_, session, Decryptor::kUnknownError, 0);
  }
}

// static
void StarboardDecryptor::KeyRequestFunc(SbDrmSystem drm_system,
                                        void* context,
#if SB_API_VERSION >= 4
                                        int /*ticket*/,
#endif  // SB_API_VERSION >= 4
                                        const void* session_id,
                                        int session_id_size,
                                        const void* content,
                                        int content_size,
                                        const char* url) {
  DCHECK(context);
  StarboardDecryptor* decryptor =
      reinterpret_cast<StarboardDecryptor*>(context);
  DCHECK_EQ(drm_system, decryptor->drm_system_);

  decryptor->OnKeyRequest(session_id, session_id_size, content, content_size,
                          url);
}

// static
void StarboardDecryptor::KeyAddedFunc(SbDrmSystem drm_system,
                                      void* context,
                                      const void* session_id,
                                      int session_id_length,
                                      bool succeeded) {
  DCHECK(context);
  StarboardDecryptor* decryptor =
      reinterpret_cast<StarboardDecryptor*>(context);
  DCHECK_EQ(drm_system, decryptor->drm_system_);
  decryptor->OnKeyAdded(session_id, session_id_length, succeeded);
}

}  // namespace media
