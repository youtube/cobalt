// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_cache.h"

#include "base/logging.h"
#include "net/base/net_errors.h"

namespace net {

//-----------------------------------------------------------------------------

HostCache::Entry::Entry(int error,
                        const AddressList& addrlist,
                        base::TimeTicks expiration)
    : error(error), addrlist(addrlist), expiration(expiration) {
}

HostCache::Entry::~Entry() {
}

//-----------------------------------------------------------------------------

HostCache::HostCache(size_t max_entries,
                     base::TimeDelta success_entry_ttl,
                     base::TimeDelta failure_entry_ttl)
    : max_entries_(max_entries),
      success_entry_ttl_(success_entry_ttl),
      failure_entry_ttl_(failure_entry_ttl) {
}

HostCache::~HostCache() {
}

const HostCache::Entry* HostCache::Lookup(const Key& key,
                                          base::TimeTicks now) const {
  DCHECK(CalledOnValidThread());
  if (caching_is_disabled())
    return NULL;

  EntryMap::const_iterator it = entries_.find(key);
  if (it == entries_.end())
    return NULL;  // Not found.

  Entry* entry = it->second.get();
  if (CanUseEntry(entry, now))
    return entry;

  return NULL;
}

HostCache::Entry* HostCache::Set(const Key& key,
                                 int error,
                                 const AddressList& addrlist,
                                 base::TimeTicks now) {
  DCHECK(CalledOnValidThread());
  if (caching_is_disabled())
    return NULL;

  base::TimeTicks expiration = now +
      (error == OK ? success_entry_ttl_ : failure_entry_ttl_);

  scoped_refptr<Entry>& entry = entries_[key];
  if (!entry) {
    // Entry didn't exist, creating one now.
    Entry* ptr = new Entry(error, addrlist, expiration);
    entry = ptr;

    // Compact the cache if we grew it beyond limit -- exclude |entry| from
    // being pruned though!
    if (entries_.size() > max_entries_)
      Compact(now, ptr);
    return ptr;
  } else {
    // Update an existing cache entry.
    entry->error = error;
    entry->addrlist = addrlist;
    entry->expiration = expiration;
    return entry.get();
  }
}

void HostCache::clear() {
  DCHECK(CalledOnValidThread());
  entries_.clear();
}

size_t HostCache::size() const {
  DCHECK(CalledOnValidThread());
  return entries_.size();
}

size_t HostCache::max_entries() const {
  DCHECK(CalledOnValidThread());
  return max_entries_;
}

base::TimeDelta HostCache::success_entry_ttl() const {
  DCHECK(CalledOnValidThread());
  return success_entry_ttl_;
}

base::TimeDelta HostCache::failure_entry_ttl() const {
  DCHECK(CalledOnValidThread());
  return failure_entry_ttl_;
}

// Note that this map may contain expired entries.
const HostCache::EntryMap& HostCache::entries() const {
  DCHECK(CalledOnValidThread());
  return entries_;
}

// static
bool HostCache::CanUseEntry(const Entry* entry, const base::TimeTicks now) {
  return entry->expiration > now;
}

void HostCache::Compact(base::TimeTicks now, const Entry* pinned_entry) {
  // Clear out expired entries.
  for (EntryMap::iterator it = entries_.begin(); it != entries_.end(); ) {
    Entry* entry = (it->second).get();
    if (entry != pinned_entry && !CanUseEntry(entry, now)) {
      entries_.erase(it++);
    } else {
      ++it;
    }
  }

  if (entries_.size() <= max_entries_)
    return;

  // If we still have too many entries, start removing unexpired entries
  // at random.
  // TODO(eroman): this eviction policy could be better (access count FIFO
  // or whatever).
  for (EntryMap::iterator it = entries_.begin();
       it != entries_.end() && entries_.size() > max_entries_; ) {
    Entry* entry = (it->second).get();
    if (entry != pinned_entry) {
      entries_.erase(it++);
    } else {
      ++it;
    }
  }

  if (entries_.size() > max_entries_)
    DLOG(WARNING) << "Still above max entries limit";
}

}  // namespace net
