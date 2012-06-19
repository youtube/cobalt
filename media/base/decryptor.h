// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECRYPTOR_H_
#define MEDIA_BASE_DECRYPTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"

namespace media {

class DecoderBuffer;

// Performs key operations and decrypts encrypted buffer.
// All public methods other than Decrypt() will be called on the renderer
// thread. Therefore, these calls should be fast and nonblocking, with key
// events fired asynchronously. Decrypt() will be called on the (video/audio)
// decoder thread synchronously.
class MEDIA_EXPORT Decryptor {
 public:
  enum KeyError {
    kUnknownError = 1,
    kClientError,
    kServiceError,
    kOutputError,
    kHardwareChangeError,
    kDomainError
  };

  Decryptor() {}
  virtual ~Decryptor() {}

  // Generates a key request for the |key_system| with |init_data| provided.
  virtual void GenerateKeyRequest(const std::string& key_system,
                                  const uint8* init_data,
                                  int init_data_length) = 0;

  // Adds a |key| to the |key_system|. The |key| is not limited to a decryption
  // key. It can be any data that the key system accepts, such as a license.
  // If multiple calls of this function set different keys for the same
  // key ID, the older key will be replaced by the newer key.
  virtual void AddKey(const std::string& key_system,
                      const uint8* key,
                      int key_length,
                      const uint8* init_data,
                      int init_data_length,
                      const std::string& session_id) = 0;

  // Cancels the key request specified by |session_id|.
  virtual void CancelKeyRequest(const std::string& key_system,
                                const std::string& session_id) = 0;

  // Decrypts the |input| buffer, which should not be NULL.
  // Returns a DecoderBuffer with the decrypted data if decryption succeeded.
  // Returns NULL if decryption failed.
  virtual scoped_refptr<DecoderBuffer> Decrypt(
      const scoped_refptr<DecoderBuffer>& input) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Decryptor);
};

}  // namespace media

#endif  // MEDIA_BASE_DECRYPTOR_H_
