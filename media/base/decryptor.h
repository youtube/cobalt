// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECRYPTOR_H_
#define MEDIA_BASE_DECRYPTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"

namespace media {

class DecoderBuffer;

// Performs key operations and decrypts encrypted buffer.
// All public methods other than Decrypt() will be called on the renderer
// thread. Therefore, these calls should be fast and nonblocking, with key
// events fired asynchronously. Decrypt() will be called on the (video/audio)
// decoder thread.
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

  enum Status {
    kSuccess,  // Decryption successfully completed. Decrypted buffer ready.
    kNoKey,  // No key is available to decrypt.
    kError  // Key is available but an error occurred during decryption.
  };

  Decryptor() {}
  virtual ~Decryptor() {}

  // Generates a key request for the |key_system| with |init_data| provided.
  // Returns true if generating key request succeeded, false otherwise.
  // Note: AddKey() and CancelKeyRequest() should only be called after
  // GenerateKeyRequest() returns true.
  virtual bool GenerateKeyRequest(const std::string& key_system,
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

  // Decrypts the |encrypted| buffer. The decrypt status and decrypted buffer
  // are returned via the provided callback |decrypt_cb|. The |encrypted| buffer
  // must not be NULL.
  //
  // Note that the callback maybe called synchronously or asynchronously.
  //
  // If the returned status is kSuccess, the |encrypted| buffer is successfully
  // decrypted and the decrypted buffer is ready to be read.
  // If the returned status is kNoKey, no decryption key is available to decrypt
  // |encrypted| buffer. In this case the decrypted buffer must be NULL.
  // If the returned status is kError, unexpected error has occurred. In this
  // case the decrypted buffer must be NULL.
  typedef base::Callback<void(Status,
                              const scoped_refptr<DecoderBuffer>&)> DecryptCB;
  virtual void Decrypt(const scoped_refptr<DecoderBuffer>& encrypted,
                       const DecryptCB& decrypt_cb) = 0;

  // Stops the decryptor and fires any pending DecryptCB immediately with kError
  // and NULL buffer.
  virtual void Stop() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Decryptor);
};

}  // namespace media

#endif  // MEDIA_BASE_DECRYPTOR_H_
