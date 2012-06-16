// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CRYPTO_AES_DECRYPTOR_H_
#define MEDIA_CRYPTO_AES_DECRYPTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/base/media_export.h"

namespace crypto {
class SymmetricKey;
}

namespace media {

class DecoderBuffer;
class DecryptorClient;

// Decrypts AES encrypted buffer into unencrypted buffer.
// All public methods other than Decrypt() will be called on the renderer
// thread. Therefore, these calls should be fast and nonblocking, with key
// events fired asynchronously. Decrypt() will be called on the (video/audio)
// decoder thread synchronously.
class MEDIA_EXPORT AesDecryptor {
 public:
  enum KeyError {
    kUnknownError = 1,
    kClientError,
    kServiceError,
    kOutputError,
    kHardwareChangeError,
    kDomainError
  };

  // The AesDecryptor does not take ownership of the |client|. The |client|
  // must be valid throughout the lifetime of the AesDecryptor.
  explicit AesDecryptor(DecryptorClient* client);
  ~AesDecryptor();

  // Generates a key request. The result of this call will be reported via the
  // client's KeyMessage() or KeyError() methods.
  void GenerateKeyRequest(const std::string& key_system,
                          const uint8* init_data,
                          int init_data_length);

  // Adds a |key| to the key system. The key is not limited to a decryption key.
  // It can be any data that the key system accepts, such as a license.
  // If multiple calls of this function set different keys for the same
  // |key_id|, the older key will be replaced by the newer key.
  // The result of this call will be reported via the client's KeyAdded(),
  // KeyMessage() or KeyError() methods.
  void AddKey(const std::string& key_system,
              const uint8* key,
              int key_length,
              const uint8* init_data,
              int init_data_length,
              const std::string& session_id);

  // Cancels the key request specified by |session_id|.
  void CancelKeyRequest(const std::string& key_system,
                        const std::string& session_id);

  // Decrypts the |input| buffer, which should not be NULL.
  // Returns a DecoderBuffer with the decrypted data if decryption succeeded.
  // Returns NULL if decryption failed.
  scoped_refptr<DecoderBuffer> Decrypt(
      const scoped_refptr<DecoderBuffer>& input);

 private:
  // KeyMap owns the crypto::SymmetricKey* and must delete them when they are
  // not needed any more.
  typedef base::hash_map<std::string, crypto::SymmetricKey*> KeyMap;

  // Since only Decrypt() is called off the renderer thread, we only need to
  // protect |key_map_|, the only member variable that is shared between
  // Decrypt() and other methods.
  KeyMap key_map_;  // Protected by the |key_map_lock_|.
  base::Lock key_map_lock_;  // Protects the |key_map_|.

  DecryptorClient* client_;

  // Make session ID unique per renderer by making it static.
  // TODO(xhwang): Make session ID more strictly defined if needed:
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=16739#c0
  static uint32 next_session_id_;

  DISALLOW_COPY_AND_ASSIGN(AesDecryptor);
};

}  // namespace media

#endif  // MEDIA_CRYPTO_AES_DECRYPTOR_H_
