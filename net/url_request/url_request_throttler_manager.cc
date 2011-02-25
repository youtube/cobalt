// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_throttler_manager.h"

#include <list>

#include "base/logging.h"
#include "base/string_util.h"

namespace net {

ThreadCheckerForRelease::ThreadCheckerForRelease()
    : valid_thread_id_(base::kInvalidThreadId) {
  EnsureThreadIdAssigned();
}

ThreadCheckerForRelease::~ThreadCheckerForRelease() {}

bool ThreadCheckerForRelease::CalledOnValidThread() const {
  EnsureThreadIdAssigned();
  base::AutoLock auto_lock(lock_);
  return valid_thread_id_ == base::PlatformThread::CurrentId();
}

void ThreadCheckerForRelease::DetachFromThread() {
  base::AutoLock auto_lock(lock_);
  valid_thread_id_ = base::kInvalidThreadId;
}

void ThreadCheckerForRelease::EnsureThreadIdAssigned() const {
  base::AutoLock auto_lock(lock_);
  if (valid_thread_id_ != base::kInvalidThreadId)
    return;
  valid_thread_id_ = base::PlatformThread::CurrentId();
}

const unsigned int URLRequestThrottlerManager::kMaximumNumberOfEntries = 1500;
const unsigned int URLRequestThrottlerManager::kRequestsBetweenCollecting = 200;

URLRequestThrottlerManager* URLRequestThrottlerManager::GetInstance() {
  return Singleton<URLRequestThrottlerManager>::get();
}

scoped_refptr<URLRequestThrottlerEntryInterface>
    URLRequestThrottlerManager::RegisterRequestUrl(const GURL &url) {
  CHECK(being_tested_ || thread_checker_.CalledOnValidThread());

  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);

  // Periodically garbage collect old entries.
  GarbageCollectEntriesIfNecessary();

  // Find the entry in the map or create it.
  scoped_refptr<URLRequestThrottlerEntry>& entry = url_entries_[url_id];
  if (entry.get() == NULL)
    entry = new URLRequestThrottlerEntry();

  // TODO(joi): Demote CHECKs in this file to DCHECKs (or remove them) once
  // we fully understand crbug.com/71721
  CHECK(entry.get());
  return entry;
}

void URLRequestThrottlerManager::OverrideEntryForTests(
    const GURL& url,
    URLRequestThrottlerEntry* entry) {
  if (entry == NULL)
    return;

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

void URLRequestThrottlerManager::InitializeOptions(bool enforce_throttling) {
  enforce_throttling_ = enforce_throttling;
  being_tested_ = false;
}

URLRequestThrottlerManager::URLRequestThrottlerManager()
    : requests_since_last_gc_(0),
      enforce_throttling_(true),
      being_tested_(true) {
  // Construction/destruction is on main thread (because BrowserMain
  // retrieves an instance to call InitializeOptions), but is from then on
  // used on I/O thread.
  thread_checker_.DetachFromThread();

  url_id_replacements_.ClearPassword();
  url_id_replacements_.ClearUsername();
  url_id_replacements_.ClearQuery();
  url_id_replacements_.ClearRef();
}

URLRequestThrottlerManager::~URLRequestThrottlerManager() {
  // Destruction is on main thread (AtExit), but real use is on I/O thread.
  thread_checker_.DetachFromThread();

  // Delete all entries.
  url_entries_.clear();
}

std::string URLRequestThrottlerManager::GetIdFromUrl(const GURL& url) const {
  if (!url.is_valid())
    return url.possibly_invalid_spec();

  GURL id = url.ReplaceComponents(url_id_replacements_);
  return StringToLowerASCII(id.spec());
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
    CHECK(i->second.get());
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
