// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_cache.h"

#include "base/logging.h"
#include "net/base/net_errors.h"

namespace net {

//-----------------------------------------------------------------------------

HostCache::Entry::Entry(int error, const AddressList& addrlist)
    : error(error),
      addrlist(addrlist) {
}

HostCache::Entry::~Entry() {
}

//-----------------------------------------------------------------------------

HostCache::HostCache(size_t max_entries)
    : entries_(max_entries) {
}

HostCache::~HostCache() {
}

const HostCache::Entry* HostCache::Lookup(const Key& key,
                                          base::TimeTicks now) {
  DCHECK(CalledOnValidThread());
  if (caching_is_disabled())
    return NULL;

  return entries_.Get(key, now);
}

void HostCache::Set(const Key& key,
                    int error,
                    const AddressList& addrlist,
                    base::TimeTicks now,
                    base::TimeDelta ttl) {
  DCHECK(CalledOnValidThread());
  if (caching_is_disabled())
    return;

  entries_.Put(key, Entry(error, addrlist), now, now + ttl);
}

void HostCache::clear() {
  DCHECK(CalledOnValidThread());
  entries_.Clear();
}

size_t HostCache::size() const {
  DCHECK(CalledOnValidThread());
  return entries_.size();
}

size_t HostCache::max_entries() const {
  DCHECK(CalledOnValidThread());
  return entries_.max_entries();
}

// Note that this map may contain expired entries.
const HostCache::EntryMap& HostCache::entries() const {
  DCHECK(CalledOnValidThread());
  return entries_;
}

// static
HostCache* HostCache::CreateDefaultCache() {
#if defined(OS_CHROMEOS)
  // Increase HostCache size for the duration of the async DNS field trial.
  // http://crbug.com/143454
  // TODO(szym): Determine the best size. http://crbug.com/114277
  static const size_t kMaxHostCacheEntries = 1000;
#else
  static const size_t kMaxHostCacheEntries = 100;
#endif
  return new HostCache(kMaxHostCacheEntries);
}

}  // namespace net
