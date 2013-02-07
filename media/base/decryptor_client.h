// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECRYPTOR_CLIENT_H_
#define MEDIA_BASE_DECRYPTOR_CLIENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "media/base/decryptor.h"

namespace media {

// Interface used by a decryptor to fire key events.
// See: http://dvcs.w3.org/hg/html-media/raw-file/tip/encrypted-media/encrypted-media.html#event-summary
class DecryptorClient {
 public:
  // Signals that a key has been added.
  virtual void KeyAdded(const std::string& key_system,
                        const std::string& session_id) = 0;

  // Signals that a key error occurred. The |system_code| is key
  // system-dependent. For clear key system, the |system_code| is always zero.
  virtual void KeyError(const std::string& key_system,
                        const std::string& session_id,
                        Decryptor::KeyError error_code,
                        int system_code) = 0;

  // Signals that a key message has been generated.
  virtual void KeyMessage(const std::string& key_system,
                          const std::string& session_id,
                          const std::string& message,
                          const std::string& default_url) = 0;

  // Signals that a key is needed for decryption. |key_system| and |session_id|
  // can be empty if the key system has not been selected.
  // TODO(xhwang): Figure out if "type" is optional for NeedKey fired from the
  // decoder.
  virtual void NeedKey(const std::string& key_system,
                       const std::string& session_id,
                       const std::string& type,
                       scoped_array<uint8> init_data,
                       int init_data_length) = 0;

 protected:
  virtual ~DecryptorClient() {}
};

}  // namespace media

#endif  // MEDIA_BASE_DECRYPTOR_CLIENT_H_
