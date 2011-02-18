// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_throttler_manager.h"

#include <list>

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
  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);

  // Periodically garbage collect old entries.
  GarbageCollectEntriesIfNecessary();

  // Find the entry in the map or create it.
  UrlEntryMap::iterator i = url_entries_.find(url_id);
  if (i == url_entries_.end()) {
    scoped_refptr<URLRequestThrottlerEntry> entry(
        new URLRequestThrottlerEntry());
    // Explicitly check whether the new operation is successful or not, in order
    // to track down crbug.com/71721
    CHECK(entry.get());

    url_entries_.insert(std::make_pair(url_id, entry));
    return entry;
  } else {
    CHECK(i->second.get());
    return i->second;
  }
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
}

URLRequestThrottlerManager::URLRequestThrottlerManager()
    : requests_since_last_gc_(0),
      enforce_throttling_(true) {
}

URLRequestThrottlerManager::~URLRequestThrottlerManager() {
  // Delete all entries.
  url_entries_.clear();
}

std::string URLRequestThrottlerManager::GetIdFromUrl(const GURL& url) const {
  if (!url.is_valid())
    return url.possibly_invalid_spec();

  if (url_id_replacements_ == NULL) {
    url_id_replacements_.reset(new GURL::Replacements());

    url_id_replacements_->ClearPassword();
    url_id_replacements_->ClearUsername();
    url_id_replacements_->ClearQuery();
    url_id_replacements_->ClearRef();
  }

  GURL id = url.ReplaceComponents(*url_id_replacements_);
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
  // The more efficient way to remove outdated entries is iterating over all the
  // elements in the map, and removing those outdated ones during the process.
  // However, one hypothesis about the cause of crbug.com/71721 is that some
  // kind of iterator error happens when we change the map during iteration. As
  // a result, we write the code this way in order to track down the bug.
  std::list<std::string> outdated_keys;
  for (UrlEntryMap::iterator entry_iter = url_entries_.begin();
       entry_iter != url_entries_.end(); ++entry_iter) {
    std::string key = entry_iter->first;
    CHECK(entry_iter->second.get());

    if ((entry_iter->second)->IsEntryOutdated())
      outdated_keys.push_back(key);
  }
  for (std::list<std::string>::iterator key_iter = outdated_keys.begin();
       key_iter != outdated_keys.end(); ++key_iter) {
    url_entries_.erase(*key_iter);
  }

  // In case something broke we want to make sure not to grow indefinitely.
  while (url_entries_.size() > kMaximumNumberOfEntries) {
    url_entries_.erase(url_entries_.begin());
  }
}

}  // namespace net
