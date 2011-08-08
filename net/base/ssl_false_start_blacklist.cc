// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_false_start_blacklist.h"

namespace net {

// static
bool SSLFalseStartBlacklist::IsMember(const char* host) {
  const char* last_two_labels = LastTwoLabels(host);
  if (!last_two_labels)
    return false;
  const unsigned bucket = Hash(last_two_labels) & (kBuckets - 1);
  const uint32 start = kHashTable[bucket];
  const uint32 end = kHashTable[bucket + 1];
  const size_t len = strlen(host);

  for (size_t i = start; i < end;) {
    const size_t blacklist_entry_len = static_cast<uint8>(kHashData[i]);
    if (len >= blacklist_entry_len &&
        memcmp(&host[len - blacklist_entry_len], &kHashData[i + 1],
               blacklist_entry_len) == 0 &&
        (len == blacklist_entry_len ||
         host[len - blacklist_entry_len - 1] == '.')) {
      return true;
    }
    i += blacklist_entry_len + 1;
  }

  return false;
}

}  // namespace net
