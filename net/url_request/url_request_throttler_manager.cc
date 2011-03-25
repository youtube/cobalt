// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_throttler_manager.h"

#include "base/logging.h"
#include "base/string_util.h"

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
  if (entry.get() == NULL)
    entry = new URLRequestThrottlerEntry();

  return entry;
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
    // TODO(joi): We know, as per crbug.com/71721, that values here
    // can sometimes be NULL because of a memory overwrite coming
    // from somewhere else.  Minidumps of the crash are at this point
    // not giving us any new information so adding the [i->second ==
    // NULL] check lessens the impact on our users (it seems to
    // generally avoid the crash).
    if (i->second == NULL || (i->second)->IsEntryOutdated()) {
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
