// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_ENCRYPTION_SCHEME_H_
#define COBALT_MEDIA_BASE_ENCRYPTION_SCHEME_H_

#include "cobalt/media/base/encryption_pattern.h"
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

  // The default constructor makes an instance that indicates no encryption.
  EncryptionScheme();

  // This constructor allows specification of the cipher mode and the pattern.
  EncryptionScheme(CipherMode mode, const EncryptionPattern& pattern);
  ~EncryptionScheme();

  bool Matches(const EncryptionScheme& other) const;

  bool is_encrypted() const { return mode_ != CIPHER_MODE_UNENCRYPTED; }
  CipherMode mode() const { return mode_; }
  const EncryptionPattern& pattern() const { return pattern_; }

 private:
  CipherMode mode_;
  EncryptionPattern pattern_;

  // Allow copy and assignment.
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_ENCRYPTION_SCHEME_H_
