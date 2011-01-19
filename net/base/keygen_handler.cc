// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/keygen_handler.h"

#if defined(USE_NSS)
#include "base/crypto/crypto_module_blocking_password_delegate.h"
#endif

namespace net {

// The constructor and destructor must be defined in a .cc file so that
// CryptoModuleBlockingPasswordDelegate can be forward-declared on platforms
// which use NSS.

KeygenHandler::KeygenHandler(int key_size_in_bits,
                             const std::string& challenge,
                             const GURL& url)
    : key_size_in_bits_(key_size_in_bits),
      challenge_(challenge),
      url_(url),
      stores_key_(true) {
}

KeygenHandler::~KeygenHandler() {
}

}  // namespace net
