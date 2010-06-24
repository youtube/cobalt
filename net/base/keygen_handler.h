// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_KEYGEN_HANDLER_H_
#define NET_BASE_KEYGEN_HANDLER_H_

#include <string>

namespace net {

// This class handles keypair generation for generating client
// certificates via the <keygen> tag.
// <http://dev.w3.org/html5/spec/Overview.html#the-keygen-element>
// <https://developer.mozilla.org/En/HTML/HTML_Extensions/KEYGEN_Tag>

class KeygenHandler {
 public:
  // Creates a handler that will generate a key with the given key size
  // and incorporate the |challenge| into the Netscape SPKAC structure.
  inline KeygenHandler(int key_size_in_bits, const std::string& challenge);

  // Actually generates the key-pair and the cert request (SPKAC), and returns
  // a base64-encoded string suitable for use as the form value of <keygen>.
  std::string GenKeyAndSignChallenge();

  // Exposed only for unit tests.
  void set_stores_key(bool store) { stores_key_ = store;}

 private:
  int key_size_in_bits_;  // key size in bits (usually 2048)
  std::string challenge_;  // challenge string sent by server
  bool stores_key_;  // should the generated key-pair be stored persistently?
};

KeygenHandler::KeygenHandler(int key_size_in_bits,
                             const std::string& challenge)
    : key_size_in_bits_(key_size_in_bits),
      challenge_(challenge),
      stores_key_(true) {
}

}  // namespace net

#endif  // NET_BASE_KEYGEN_HANDLER_H_
