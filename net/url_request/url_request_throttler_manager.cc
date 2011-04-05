// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_throttler_manager.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/net_util.h"

namespace net {

const unsigned int URLRequestThrottlerManager::kMaximumNumberOfEntries = 1500;
const unsigned int URLRequestThrottlerManager::kRequestsBetweenCollecting = 200;

URLRequestThrottlerManager* URLRequestThrottlerManager::GetInstance() {
  return Singleton<URLRequestThrottlerManager>::get();
}

scoped_refptr<URLRequestThrottlerEntryInterface>
    URLRequestThrottlerManager::RegisterRequestUrl(const GURL &url) {
  DCHECK(!enable_thread_checks_ || CalledOnValidThread());

  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);

  // Periodically garbage collect old entries.
  GarbageCollectEntriesIfNecessary();

  // Find the entry in the map or create it.
  scoped_refptr<URLRequestThrottlerEntry>& entry = url_entries_[url_id];
  if (entry.get() == NULL) {
    entry = new URLRequestThrottlerEntry(this);

    // We only disable back-off throttling on an entry that we have
    // just constructed.  This is to allow unit tests to explicitly override
    // the entry for localhost URLs.  Given that we do not attempt to
    // disable throttling for entries already handed out (see comment
    // in AddToOptOutList), this is not a problem.
    std::string host = url.host();
    if (opt_out_hosts_.find(host) != opt_out_hosts_.end() ||
        IsLocalhost(host)) {
      // TODO(joi): Once sliding window is separate from back-off throttling,
      // we can simply return a dummy implementation of
      // URLRequestThrottlerEntryInterface here that never blocks anything (and
      // not keep entries in url_entries_ for opted-out sites).
      entry->DisableBackoffThrottling();
    }
  }

  return entry;
}

void URLRequestThrottlerManager::AddToOptOutList(const std::string& host) {
  // There is an edge case here that we are not handling, to keep things
  // simple.  If a host starts adding the opt-out header to its responses
  // after there are already one or more entries in url_entries_ for that
  // host, the pre-existing entries may still perform back-off throttling.
  // In practice, this would almost never occur.
  opt_out_hosts_.insert(host);
}

void URLRequestThrottlerManager::OverrideEntryForTests(
    const GURL& url,
    URLRequestThrottlerEntry* entry) {
  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);

  // Periodically garbage collect old entries.
  GarbageCollectEntriesIfNecessary();

  url_entries_[url_id] = entry;
}

void URLRequestThrottlerManager::EraseEntryForTests(const GURL& url) {
  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);
  url_entries_.erase(url_id);
}

void URLRequestThrottlerManager::set_enable_thread_checks(bool enable) {
  enable_thread_checks_ = enable;
}

bool URLRequestThrottlerManager::enable_thread_checks() const {
  return enable_thread_checks_;
}

void URLRequestThrottlerManager::set_enforce_throttling(bool enforce) {
  enforce_throttling_ = enforce;
}

bool URLRequestThrottlerManager::enforce_throttling() {
  return enforce_throttling_;
}

// TODO(joi): Turn throttling on by default when appropriate.
URLRequestThrottlerManager::URLRequestThrottlerManager()
    : requests_since_last_gc_(0),
      enforce_throttling_(false),
      enable_thread_checks_(false) {
  // Construction/destruction is on main thread (because BrowserMain
  // retrieves an instance to call InitializeOptions), but is from then on
  // used on I/O thread.
  DetachFromThread();

  url_id_replacements_.ClearPassword();
  url_id_replacements_.ClearUsername();
  url_id_replacements_.ClearQuery();
  url_id_replacements_.ClearRef();
}

URLRequestThrottlerManager::~URLRequestThrottlerManager() {
  // Destruction is on main thread (AtExit), but real use is on I/O thread.
  DetachFromThread();

  // Since, for now, the manager object might conceivably go away before
  // the entries, detach the entries' back-pointer to the manager.
  //
  // TODO(joi): Revisit whether to make entries non-refcounted.
  UrlEntryMap::iterator i = url_entries_.begin();
  while (i != url_entries_.end()) {
    if (i->second != NULL) {
      i->second->DetachManager();
    }
    ++i;
  }

  // Delete all entries.
  url_entries_.clear();
}

std::string URLRequestThrottlerManager::GetIdFromUrl(const GURL& url) const {
  if (!url.is_valid())
    return url.possibly_invalid_spec();

  GURL id = url.ReplaceComponents(url_id_replacements_);
  return StringToLowerASCII(id.spec()).c_str();
}

void URLRequestThrottlerManager::GarbageCollectEntriesIfNecessary() {
  requests_since_last_gc_++;
  if (requests_since_last_gc_ < kRequestsBetweenCollecting)
    return;
  requests_since_last_gc_ = 0;

  GarbageCollectEntries();
}

void URLRequestThrottlerManager::GarbageCollectEntries() {
  UrlEntryMap::iterator i = url_entries_.begin();
  while (i != url_entries_.end()) {
    if ((i->second)->IsEntryOutdated()) {
      url_entries_.erase(i++);
    } else {
      ++i;
    }
  }

  // In case something broke we want to make sure not to grow indefinitely.
  while (url_entries_.size() > kMaximumNumberOfEntries) {
    url_entries_.erase(url_entries_.begin());
  }
}

}  // namespace net
