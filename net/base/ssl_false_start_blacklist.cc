// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_false_start_blacklist.h"

namespace net {

// static
bool SSLFalseStartBlacklist::IsMember(const std::string& host) {
  const std::string last_two_components(LastTwoComponents(host));
  if (last_two_components.empty())
    return false;

  const size_t bucket = Hash(last_two_components) & (kBuckets - 1);
  for (size_t i = kHashTable[bucket]; i < kHashTable[bucket + 1]; ) {
    const size_t blacklist_entry_len = static_cast<uint8>(kHashData[i]);
    if (host.length() >= blacklist_entry_len &&
        !host.compare(host.length() - blacklist_entry_len, blacklist_entry_len,
                      &kHashData[i + 1], blacklist_entry_len) &&
        (host.length() == blacklist_entry_len ||
         host[host.length() - blacklist_entry_len - 1] == '.')) {
      return true;
    }
    i += blacklist_entry_len + 1;
  }

  return false;
}

}  // namespace net
