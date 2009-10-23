// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_KEYGEN_HANDLER_H_
#define NET_BASE_KEYGEN_HANDLER_H_

#include <string>

namespace net {

// This class handles keypair generation for generating client
// certificates via the Netscape <keygen> tag.

class KeygenHandler {
 public:
  KeygenHandler(int key_size_index, const std::string& challenge);
  std::string GenKeyAndSignChallenge();

 private:
  int key_size_index_;
  std::string challenge_;
};

}  // namespace net

#endif  // NET_BASE_KEYGEN_HANDLER_H_
