// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_ENCRYPTION_SCHEME_H_
#define COBALT_MEDIA_BASE_ENCRYPTION_SCHEME_H_

#include "cobalt/media/base/media_export.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

// Specification of whether and how the stream is encrypted (in whole or part).
class MEDIA_EXPORT EncryptionScheme {
 public:
  // Algorithm and mode used for encryption. CIPHER_MODE_UNENCRYPTED indicates
  // no encryption.
  enum CipherMode {
    CIPHER_MODE_UNENCRYPTED,
    CIPHER_MODE_AES_CTR,
    CIPHER_MODE_AES_CBC,
    CIPHER_MODE_MAX = CIPHER_MODE_AES_CBC
  };

  // CENC 3rd Edition adds pattern encryption, through two new protection
  // schemes: 'cens' (with AES-CTR) and 'cbcs' (with AES-CBC).
  // The pattern applies independently to each 'encrypted' part of the frame (as
  // defined by the relevant subsample entries), and reduces further the
  // actual encryption applied through a repeating pattern of (encrypt:skip)
  // 16 byte blocks. For example, in a (1:9) pattern, the first block is
  // encrypted, and the next nine are skipped. This pattern is applied
  // repeatedly until the end of the last 16-byte block in the subsample.
  // Any remaining bytes are left clear.
  // If either of encrypt_blocks or skip_blocks is 0, pattern encryption is
  // disabled.
  class MEDIA_EXPORT Pattern {
   public:
    Pattern();
    Pattern(uint32_t encrypt_blocks, uint32_t skip_blocks);
    ~Pattern();

    bool Matches(const Pattern& other) const;

    uint32_t encrypt_blocks() const { return encrypt_blocks_; }
    uint32_t skip_blocks() const { return skip_blocks_; }

    bool IsInEffect() const;

   private:
    uint32_t encrypt_blocks_;
    uint32_t skip_blocks_;

    // Allow copy and assignment.
  };

  // The default constructor makes an instance that indicates no encryption.
  EncryptionScheme();

  // This constructor allows specification of the cipher mode and the pattern.
  EncryptionScheme(CipherMode mode, const Pattern& pattern);
  ~EncryptionScheme();

  bool Matches(const EncryptionScheme& other) const;

  bool is_encrypted() const { return mode_ != CIPHER_MODE_UNENCRYPTED; }
  CipherMode mode() const { return mode_; }
  const Pattern& pattern() const { return pattern_; }

 private:
  CipherMode mode_;
  Pattern pattern_;

  // Allow copy and assignment.
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_ENCRYPTION_SCHEME_H_
