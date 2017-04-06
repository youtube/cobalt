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

#ifndef MEDIA_CRYPTO_STARBOARD_DECRYPTOR_H_
#define MEDIA_CRYPTO_STARBOARD_DECRYPTOR_H_

#include "build/build_config.h"

#if !defined(OS_STARBOARD)
#error "This class only works in Starboard!"
#endif  // !defined(OS_STARBOARD)

#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/decryptor.h"
#include "media/base/decryptor_client.h"
#include "starboard/drm.h"

namespace media {

// This class implements the Decryptor interface based on Starboard DRM
// functions provided in drm.h and media.h.
class StarboardDecryptor : public Decryptor {
 public:
  StarboardDecryptor(const char* key_system, DecryptorClient* decryptor_client);
  ~StarboardDecryptor() OVERRIDE;

  SbDrmSystem drm_system() { return drm_system_; }

  // Decryptor implementation.
  bool GenerateKeyRequest(const std::string& key_system,
                          const std::string& type,
                          const uint8_t* init_data,
                          int init_data_length) OVERRIDE;
  void AddKey(const std::string& key_system,
              const uint8_t* key,
              int key_length,
              const uint8_t* init_data,
              int init_data_length,
              const std::string& session_id) OVERRIDE;
  void CancelKeyRequest(const std::string& key_system,
                        const std::string& session_id) OVERRIDE;
  void RegisterKeyAddedCB(StreamType stream_type,
                          const KeyAddedCB& key_added_cb) OVERRIDE;
  void Decrypt(StreamType stream_type,
               const scoped_refptr<DecoderBuffer>& encrypted,
               const DecryptCB& decrypt_cb) OVERRIDE;
  void CancelDecrypt(StreamType stream_type) OVERRIDE;
  void InitializeAudioDecoder(scoped_ptr<AudioDecoderConfig> config,
                              const DecoderInitCB& init_cb) OVERRIDE;
  void InitializeVideoDecoder(scoped_ptr<VideoDecoderConfig> config,
                              const DecoderInitCB& init_cb) OVERRIDE;
  void DecryptAndDecodeAudio(const scoped_refptr<DecoderBuffer>& encrypted,
                             const AudioDecodeCB& audio_decode_cb) OVERRIDE;
  void DecryptAndDecodeVideo(const scoped_refptr<DecoderBuffer>& encrypted,
                             const VideoDecodeCB& video_decode_cb) OVERRIDE;
  void ResetDecoder(StreamType stream_type) OVERRIDE;
  void DeinitializeDecoder(StreamType stream_type) OVERRIDE;

 private:
  void OnKeyRequest(const void* session_id,
                    int session_id_size,
                    const void* content,
                    int content_size,
                    const char* url);
  void OnKeyAdded(const void* session_id,
                  int session_id_length,
                  bool succeeded);

  static void KeyRequestFunc(SbDrmSystem drm_system,
                             void* context,
#if SB_API_VERSION >= SB_DRM_SESSION_UPDATE_REQUEST_TICKET_VERSION
                             int ticket,
#endif  // SB_API_VERSION >= SB_DRM_SESSION_UPDATE_REQUEST_TICKET_VERSION
                             const void* session_id,
                             int session_id_size,
                             const void* content,
                             int content_size,
                             const char* url);
  static void KeyAddedFunc(SbDrmSystem drm_system,
                           void* context,
                           const void* session_id,
                           int session_id_length,
                           bool succeeded);

  // DecryptorClient through which key events are fired.
  // Calls to the client are both cheap and thread-safe.
  DecryptorClient* client_;
  SbDrmSystem drm_system_;
  std::string key_system_;

  std::set<std::string> keys_;

  DISALLOW_COPY_AND_ASSIGN(StarboardDecryptor);
};

}  // namespace media

#endif  // MEDIA_CRYPTO_STARBOARD_DECRYPTOR_H_
