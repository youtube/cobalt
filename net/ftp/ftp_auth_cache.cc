// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_auth_cache.h"

#include "base/logging.h"
#include "googleurl/src/gurl.h"

namespace net {

// static
const size_t FtpAuthCache::kMaxEntries = 10;

AuthData* FtpAuthCache::Lookup(const GURL& origin) {
  Entry* entry = LookupEntry(origin);
  return (entry ? entry->auth_data : NULL);
}

void FtpAuthCache::Add(const GURL& origin, AuthData* auth_data) {
  DCHECK(origin.SchemeIs("ftp"));
  DCHECK_EQ(origin.GetOrigin(), origin);

  Entry* entry = LookupEntry(origin);
  if (entry) {
    entry->auth_data = auth_data;
  } else {
    entries_.push_front(Entry(origin, auth_data));

    // Prevent unbound memory growth of the cache.
    if (entries_.size() > kMaxEntries)
      entries_.pop_back();
  }
}

void FtpAuthCache::Remove(const GURL& origin) {
  for (EntryList::iterator it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->origin == origin) {
      entries_.erase(it);
      DCHECK(!LookupEntry(origin));
      return;
    }
  }
}

FtpAuthCache::Entry* FtpAuthCache::LookupEntry(const GURL& origin) {
  for (EntryList::iterator it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->origin == origin)
      return &(*it);
  }
  return NULL;
}

}  // namespace net
