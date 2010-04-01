// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_KEYGEN_HANDLER_H_
#define NET_BASE_KEYGEN_HANDLER_H_

#include <map>
#include <string>

#include "base/lock.h"
#include "base/singleton.h"

namespace net {

// This class handles keypair generation for generating client
// certificates via the <keygen> tag.
// <http://dev.w3.org/html5/spec/Overview.html#the-keygen-element>
// <https://developer.mozilla.org/En/HTML/HTML_Extensions/KEYGEN_Tag>

class KeygenHandler {
 public:
  // This class stores the relative location for a given private key. It does
  // not store the private key, or a handle to the private key, on the basis
  // that the key may be located on a smart card or device which may not be
  // present at the time of retrieval.
  class KeyLocation {
   public:
#if defined(OS_WIN)
    std::wstring container_name;
    std::wstring provider_name;
#elif defined(OS_MACOSX)
    std::string keychain_path;
#elif defined(USE_NSS)
    std::string slot_name;
#endif

    // Only used by unit tests.
    bool Equals(const KeyLocation& location) const;
  };

  // This class stores information about the keys the KeygenHandler has
  // generated, so that the private keys can be properly associated with any
  // certificates that might be sent to the client based on those keys.
  // TODO(wtc): consider adding a Remove() method.
  class Cache {
   public:
    static Cache* GetInstance();
    void Insert(const std::string& public_key_info,
                const KeyLocation& location);

    // True if the |public_key_info| was located and the location stored into
    // |*location|.
    bool Find(const std::string& public_key_info, KeyLocation* location);

   private:
    typedef std::map<std::string, KeyLocation> KeyLocationMap;

    // Obtain an instance of the KeyCache by using GetInstance().
    Cache() {}
    friend struct DefaultSingletonTraits<Cache>;

    Lock lock_;

    // The key cache. You must obtain |lock_| before using |cache_|.
    KeyLocationMap cache_;

    DISALLOW_COPY_AND_ASSIGN(Cache);
  };

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
