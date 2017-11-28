// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CRYPTO_AES_DECRYPTOR_H_
#define MEDIA_CRYPTO_AES_DECRYPTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/synchronization/lock.h"
#include "media/base/decryptor.h"
#include "media/base/media_export.h"

namespace crypto {
class SymmetricKey;
}

namespace media {

class DecryptorClient;

// Decrypts an AES encrypted buffer into an unencrypted buffer. The AES
// encryption must be CTR with a key size of 128bits.
class MEDIA_EXPORT AesDecryptor : public Decryptor {
 public:
  // The AesDecryptor does not take ownership of the |client|. The |client|
  // must be valid throughout the lifetime of the AesDecryptor.
  explicit AesDecryptor(DecryptorClient* client);
  virtual ~AesDecryptor();

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
  // TODO(fgalligan): Remove this and change KeyMap to use crypto::SymmetricKey
  // as there are no decryptors that are performing an integrity check.
  // Helper class that manages the decryption key.
  class DecryptionKey {
   public:
    explicit DecryptionKey(const std::string& secret);
    ~DecryptionKey();

    // Creates the encryption key.
    bool Init();

    crypto::SymmetricKey* decryption_key() { return decryption_key_.get(); }

   private:
    // The base secret that is used to create the decryption key.
    const std::string secret_;

    // The key used to decrypt the data.
    scoped_ptr<crypto::SymmetricKey> decryption_key_;

    DISALLOW_COPY_AND_ASSIGN(DecryptionKey);
  };

  // Sets |key| for |key_id|. The AesDecryptor takes the ownership of the |key|.
  void SetKey(const std::string& key_id, scoped_ptr<DecryptionKey> key);

  // Gets a DecryptionKey associated with |key_id|. The AesDecryptor still owns
  // the key. Returns NULL if no key is associated with |key_id|.
  DecryptionKey* GetKey(const std::string& key_id) const;

  // KeyMap owns the DecryptionKey* and must delete them when they are
  // not needed any more.
  typedef base::hash_map<std::string, DecryptionKey*> KeyMap;

  // Since only Decrypt() is called off the renderer thread, we only need to
  // protect |key_map_|, the only member variable that is shared between
  // Decrypt() and other methods.
  KeyMap key_map_;  // Protected by the |key_map_lock_|.
  mutable base::Lock key_map_lock_;  // Protects the |key_map_|.

  // Make session ID unique per renderer by making it static.
  // TODO(xhwang): Make session ID more strictly defined if needed:
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=16739#c0
  static uint32 next_session_id_;

  DecryptorClient* const client_;

  KeyAddedCB audio_key_added_cb_;
  KeyAddedCB video_key_added_cb_;

  DISALLOW_COPY_AND_ASSIGN(AesDecryptor);
};

}  // namespace media

#endif  // MEDIA_CRYPTO_AES_DECRYPTOR_H_
