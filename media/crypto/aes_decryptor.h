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

// Decrypts AES encrypted buffer into unencrypted buffer.
class MEDIA_EXPORT AesDecryptor {
 public:
  AesDecryptor();
  ~AesDecryptor();

  // Add a |key_id| and |key| pair to the key system. The key is not limited to
  // a decryption key. It can be any data that the key system accepts, such as
  // a license. If multiple calls of this function set different keys for the
  // same |key_id|, the older key will be replaced by the newer key.
  void AddKey(const uint8* key_id, int key_id_size,
              const uint8* key, int key_size);

  // Decrypt |input| buffer. The |input| should not be NULL.
  // Return a DecoderBuffer with the decrypted data if decryption succeeded.
  // Return NULL if decryption failed.
  scoped_refptr<DecoderBuffer> Decrypt(
      const scoped_refptr<DecoderBuffer>& input);

 private:
  // KeyMap owns the crypto::SymmetricKey* and must delete them when they are
  // not needed any more.
  typedef base::hash_map<std::string, crypto::SymmetricKey*> KeyMap;
  KeyMap key_map_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(AesDecryptor);
};

}  // namespace media

#endif  // MEDIA_CRYPTO_AES_DECRYPTOR_H_
