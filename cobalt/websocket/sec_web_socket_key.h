// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef COBALT_WEBSOCKET_SEC_WEB_SOCKET_KEY_H_
#define COBALT_WEBSOCKET_SEC_WEB_SOCKET_KEY_H_

#include <algorithm>
#include <string>

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "starboard/memory.h"

namespace cobalt {
namespace websocket {

struct SecWebSocketKey {
  enum { kKeySizeInBytes = 16 };
  typedef char SecWebSocketKeyBytes[kKeySizeInBytes];

  SecWebSocketKey() {
    memset(&key_bytes[0], 0, kKeySizeInBytes);
    base::StringPiece key_stringpiece(key_bytes, sizeof(key_bytes));
    bool success = base::Base64Encode(key_stringpiece, &key_base64_encoded);
    DCHECK(success);
  }

  explicit SecWebSocketKey(const SecWebSocketKeyBytes& key) {
    memcpy(&key_bytes[0], &key[0], sizeof(key_bytes));
    base::StringPiece key_stringpiece(key_bytes, sizeof(key_bytes));
    bool success = base::Base64Encode(key_stringpiece, &key_base64_encoded);
    DCHECK(success);
  }

  const std::string& GetKeyEncodedInBase64() const {
    DCHECK_GT(key_base64_encoded.size(), 0ull);
    return key_base64_encoded;
  }
  const SecWebSocketKeyBytes& GetRawKeyBytes() const { return key_bytes; }

 private:
  SecWebSocketKeyBytes key_bytes;
  std::string key_base64_encoded;
};

}  // namespace websocket
}  // namespace cobalt

#endif  // COBALT_WEBSOCKET_SEC_WEB_SOCKET_KEY_H_
