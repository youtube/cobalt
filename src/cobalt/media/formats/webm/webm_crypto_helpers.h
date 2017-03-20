// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FORMATS_WEBM_WEBM_CRYPTO_HELPERS_H_
#define COBALT_MEDIA_FORMATS_WEBM_WEBM_CRYPTO_HELPERS_H_

#include <stdint.h>

#include "base/memory/scoped_ptr.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/media_export.h"

namespace media {

// Fills an initialized DecryptConfig, which can be sent to the Decryptor if
// the stream has potentially encrypted frames. Also sets |data_offset| which
// indicates where the encrypted data starts. Leaving the IV empty will tell
// the decryptor that the frame is unencrypted. Returns true if |data| is valid,
// false otherwise, in which case |decrypt_config| and |data_offset| will not be
// changed. Current encrypted WebM request for comments specification is here
// http://wiki.webmproject.org/encryption/webm-encryption-rfc
bool MEDIA_EXPORT WebMCreateDecryptConfig(
    const uint8_t* data, int data_size, const uint8_t* key_id, int key_id_size,
    scoped_ptr<DecryptConfig>* decrypt_config, int* data_offset);

}  // namespace media

#endif  // COBALT_MEDIA_FORMATS_WEBM_WEBM_CRYPTO_HELPERS_H_
