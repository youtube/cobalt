// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/keygen_handler.h"

#include "net/third_party/mozilla_security_manager/nsKeygenHandler.h"

// PSM = Mozilla's Personal Security Manager.
namespace psm = mozilla_security_manager;

namespace net {

std::string KeygenHandler::GenKeyAndSignChallenge() {
  return psm::GenKeyAndSignChallenge(key_size_in_bits_, challenge_, url_,
                                     stores_key_);
}

}  // namespace net
