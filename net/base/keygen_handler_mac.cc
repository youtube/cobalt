// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/keygen_handler.h"

#include "base/logging.h"

namespace net {

KeygenHandler::KeygenHandler(int key_size_index,
                             const std::string& challenge)
    : key_size_index_(key_size_index),
      challenge_(challenge) {
  NOTIMPLEMENTED();
}

std::string KeygenHandler::GenKeyAndSignChallenge() {
  NOTIMPLEMENTED();
  return std::string();
}

}  // namespace net
