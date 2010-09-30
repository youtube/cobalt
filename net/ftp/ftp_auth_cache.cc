// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_auth_cache.h"

#include "base/logging.h"
#include "googleurl/src/gurl.h"

namespace net {

// static
const size_t FtpAuthCache::kMaxEntries = 10;

FtpAuthCache::Entry::Entry(const GURL& origin,
                           const string16& username,
                           const string16& password)
    : origin(origin),
      username(username),
      password(password) {
}

FtpAuthCache::Entry::~Entry() {}

FtpAuthCache::FtpAuthCache() {}

FtpAuthCache::~FtpAuthCache() {}

FtpAuthCache::Entry* FtpAuthCache::Lookup(const GURL& origin) {
  for (EntryList::iterator it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->origin == origin)
      return &(*it);
  }
  return NULL;
}

void FtpAuthCache::Add(const GURL& origin, const string16& username,
                       const string16& password) {
  DCHECK(origin.SchemeIs("ftp"));
  DCHECK_EQ(origin.GetOrigin(), origin);

  Entry* entry = Lookup(origin);
  if (entry) {
    entry->username = username;
    entry->password = password;
  } else {
    entries_.push_front(Entry(origin, username, password));

    // Prevent unbound memory growth of the cache.
    if (entries_.size() > kMaxEntries)
      entries_.pop_back();
  }
}

void FtpAuthCache::Remove(const GURL& origin, const string16& username,
                          const string16& password) {
  for (EntryList::iterator it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->origin == origin && it->username == username &&
        it->password == password) {
      entries_.erase(it);
      DCHECK(!Lookup(origin));
      return;
    }
  }
}

}  // namespace net
