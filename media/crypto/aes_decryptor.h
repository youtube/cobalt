// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CRYPTO_AES_DECRYPTOR_H_
#define MEDIA_CRYPTO_AES_DECRYPTOR_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"

namespace media {

class Buffer;

// Decrypts AES encrypted buffer into unencrypted buffer.
class MEDIA_EXPORT AesDecryptor {
 public:
  AesDecryptor();

  // Decrypt |input| buffer. The |input| should not be NULL.
  // Return a Buffer that contains decrypted data if decryption succeeded.
  // Return NULL if decryption failed.
  scoped_refptr<Buffer> Decrypt(const scoped_refptr<Buffer>& input);

 private:
  DISALLOW_COPY_AND_ASSIGN(AesDecryptor);
};

}  // namespace media

#endif  // MEDIA_CRYPTO_AES_DECRYPTOR_H_
