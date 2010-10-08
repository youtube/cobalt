// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/keygen_handler.h"

#if defined(USE_OPENSSL)

namespace net {

std::string KeygenHandler::GenKeyAndSignChallenge() {
  // TODO(bulach): implement me.
  return "";
}

}  // namespace net

#endif  // USE_OPENSSL
