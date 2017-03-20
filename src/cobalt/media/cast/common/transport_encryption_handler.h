// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_CAST_COMMON_TRANSPORT_ENCRYPTION_HANDLER_H_
#define COBALT_MEDIA_CAST_COMMON_TRANSPORT_ENCRYPTION_HANDLER_H_

// Helper class to handle encryption for the Cast Transport library.

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/threading/non_thread_safe.h"
#include "media/cast/common/frame_id.h"

namespace crypto {
class Encryptor;
class SymmetricKey;
}

namespace media {
namespace cast {

class TransportEncryptionHandler : public base::NonThreadSafe {
 public:
  TransportEncryptionHandler();
  ~TransportEncryptionHandler();

  bool Initialize(const std::string& aes_key, const std::string& aes_iv_mask);

  bool Encrypt(FrameId frame_id, const base::StringPiece& data,
               std::string* encrypted_data);

  bool Decrypt(FrameId frame_id, const base::StringPiece& ciphertext,
               std::string* plaintext);

  bool is_activated() const { return is_activated_; }

 private:
  std::unique_ptr<crypto::SymmetricKey> key_;
  std::unique_ptr<crypto::Encryptor> encryptor_;
  std::string iv_mask_;
  bool is_activated_;

  DISALLOW_COPY_AND_ASSIGN(TransportEncryptionHandler);
};

}  // namespace cast
}  // namespace media

#endif  // COBALT_MEDIA_CAST_COMMON_TRANSPORT_ENCRYPTION_HANDLER_H_
