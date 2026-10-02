// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_CRYPTO_HELPERS_H_
#define MEDIA_FORMATS_WEBM_WEBM_CRYPTO_HELPERS_H_

#include <stdint.h>

#include <memory>

#include "build/build_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_export.h"

namespace media {

// Fills |decrypt_config|, which can be sent to the Decryptor if the stream
// has potentially encrypted frames. Also sets |data_offset| which indicates
// where the encrypted data starts. If the frame is unencrypted
// |*decrypt_config| will be null. Returns true if |data| is valid, false
// otherwise, in which case |decrypt_config| and |data_offset| will not be
// changed. Current encrypted WebM request for comments specification is here
// http://wiki.webmproject.org/encryption/webm-encryption-rfc
bool MEDIA_EXPORT
WebMCreateDecryptConfig(const uint8_t* data,
                        int data_size,
                        const uint8_t* key_id,
                        int key_id_size,
                        std::unique_ptr<DecryptConfig>* decrypt_config,
                        size_t* data_offset);

#if BUILDFLAG(USE_STARBOARD_MEDIA)
// The most bytes a WebM encryption header can occupy: a signal byte, an 8 byte
// IV, a partition count byte, and up to 255 four byte partition offsets.
inline constexpr int kWebMMaxEncryptionHeaderSize = 1 + 8 + 1 + 255 * 4;

// Same as above, where |data| holds only the first |data_size| bytes of a frame
// that is |frame_size| bytes long. |data_size| must cover the whole encryption
// header, so kWebMMaxEncryptionHeaderSize bytes are always enough.
//
// This lets a caller whose frame is not contiguous inspect just the header,
// which is why |frame_size| has to be passed separately: the size of the last
// subsample partition is derived from it.
bool MEDIA_EXPORT
WebMCreateDecryptConfig(const uint8_t* data,
                        int data_size,
                        int frame_size,
                        const uint8_t* key_id,
                        int key_id_size,
                        std::unique_ptr<DecryptConfig>* decrypt_config,
                        size_t* data_offset);
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_WEBM_CRYPTO_HELPERS_H_
