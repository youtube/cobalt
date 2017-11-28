// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PLAYER_CRYPTO_PROXY_DECRYPTOR_H_
#define MEDIA_PLAYER_CRYPTO_PROXY_DECRYPTOR_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "media/base/decryptor.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

class DecryptorClient;

// A decryptor proxy that creates a real decryptor object on demand and
// forwards decryptor calls to it.
// TODO(xhwang): Currently we don't support run-time switching among decryptor
// objects. Fix this when needed.
class ProxyDecryptor : public Decryptor {
 public:
  ProxyDecryptor(DecryptorClient* decryptor_client);
  virtual ~ProxyDecryptor();

  // Requests the ProxyDecryptor to notify the decryptor when it's ready through
  // the |decryptor_ready_cb| provided.
  // If |decryptor_ready_cb| is null, the existing callback will be fired with
  // NULL immediately and reset.
  void SetDecryptorReadyCB(const DecryptorReadyCB& decryptor_ready_cb);
  // Notify that the decryptor is going to be destroyed soon.  So any pending or
  // future DecryptorReadyCB will be called with NULL and the pipeline can be
  // torn down.
  void DestroySoon();

  // Decryptor implementation.
  virtual bool GenerateKeyRequest(const std::string& key_system,
                                  const std::string& type,
                                  const uint8* init_data,
                                  int init_data_length) override;
  virtual void AddKey(const std::string& key_system,
                      const uint8* key,
                      int key_length,
                      const uint8* init_data,
                      int init_data_length,
                      const std::string& session_id) override;
  virtual void CancelKeyRequest(const std::string& key_system,
                                const std::string& session_id) override;
  virtual void RegisterKeyAddedCB(StreamType stream_type,
                                  const KeyAddedCB& key_added_cb) override;
  virtual void Decrypt(StreamType stream_type,
                       const scoped_refptr<DecoderBuffer>& encrypted,
                       const DecryptCB& decrypt_cb) override;
  virtual void CancelDecrypt(StreamType stream_type) override;
  virtual void InitializeAudioDecoder(scoped_ptr<AudioDecoderConfig> config,
                                      const DecoderInitCB& init_cb) override;
  virtual void InitializeVideoDecoder(scoped_ptr<VideoDecoderConfig> config,
                                      const DecoderInitCB& init_cb) override;
  virtual void DecryptAndDecodeAudio(
      const scoped_refptr<DecoderBuffer>& encrypted,
      const AudioDecodeCB& audio_decode_cb) override;
  virtual void DecryptAndDecodeVideo(
      const scoped_refptr<DecoderBuffer>& encrypted,
      const VideoDecodeCB& video_decode_cb) override;
  virtual void ResetDecoder(StreamType stream_type) override;
  virtual void DeinitializeDecoder(StreamType stream_type) override;

 private:
  // Helper functions to create decryptors to handle the given |key_system|.
  scoped_ptr<Decryptor> CreateDecryptor(const std::string& key_system);

  // DecryptorClient through which key events are fired.
  DecryptorClient* client_;

  // Protects the |decryptor_|. Note that |decryptor_| itself should be thread
  // safe as per the Decryptor interface.
  base::Lock lock_;

  bool destroy_soon_;
  DecryptorReadyCB decryptor_ready_cb_;

  // The real decryptor that does decryption for the ProxyDecryptor.
  // This pointer is protected by the |lock_|.
  scoped_ptr<Decryptor> decryptor_;

  DISALLOW_COPY_AND_ASSIGN(ProxyDecryptor);
};

}  // namespace media

#endif  // MEDIA_PLAYER_CRYPTO_PROXY_DECRYPTOR_H_
