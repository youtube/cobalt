// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECRYPT_CONFIG_H_
#define MEDIA_BASE_DECRYPT_CONFIG_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"

namespace media {

// Contains all information that a decryptor needs to decrypt a frame.
class MEDIA_EXPORT DecryptConfig {
 public:
  // Keys are always 128 bits.
  static const int kDecryptionKeySize = 16;

  // |key_id| is the ID that references the decryption key for this frame. |iv|
  // is the initialization vector defined by the encrypted format. Currently
  // |iv_size| must be 16 bytes as defined by WebM and ISO. |checksum| is the
  // hash value of the encrypted buffer. |checksum| is defined by the
  // encrypted format and may be NULL. |encrypted_frame_offset| is the offset
  // into the encrypted buffer that the encrypted frame starts. The class
  // will copy the data from |key_id|, |iv|, and |checksum|.
  DecryptConfig(const uint8* key_id, int key_id_size,
                const uint8* iv, int iv_size,
                const uint8* checksum, int checksum_size,
                int encrypted_frame_offset);
  ~DecryptConfig();

  const uint8* key_id() const { return key_id_.get(); }
  int key_id_size() const { return key_id_size_; }
  const uint8* iv() const { return iv_.get(); }
  int iv_size() const { return iv_size_; }
  const uint8* checksum() const { return checksum_.get(); }
  int checksum_size() const { return checksum_size_; }
  int encrypted_frame_offset() const { return encrypted_frame_offset_; }

 private:
  const scoped_array<uint8> key_id_;
  const int key_id_size_;

  // Initialization vector.
  const scoped_array<uint8> iv_;
  const int iv_size_;

  // Checksum of the data to be verified before decrypting the data. This may
  // be NULL for some formats.
  const scoped_array<uint8> checksum_;
  const int checksum_size_;

  // This is the offset in bytes to where the encrypted data starts within
  // the input buffer.
  const int encrypted_frame_offset_;

  DISALLOW_COPY_AND_ASSIGN(DecryptConfig);
};

}  // namespace media

#endif  // MEDIA_BASE_DECRYPT_CONFIG_H_
