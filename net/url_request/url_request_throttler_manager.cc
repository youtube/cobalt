// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_throttler_manager.h"

#include <list>

#include "base/logging.h"
#include "base/string_util.h"

namespace {

// TODO(joi): Remove after crbug.com/71721 is fixed.
struct IteratorHistory {
  // Copy of 'this' pointer at time of access; this helps both because
  // the this pointer is often obfuscated (at least for this particular
  // stack trace) in fully optimized builds, and possibly to detect
  // changes in the this pointer during iteration over the map (e.g.
  // from another thread overwriting memory).
  net::URLRequestThrottlerManager* self;

  // Copy of URL key.
  char url[256];

  // Not a refptr, we don't want to change behavior by keeping it alive.
  net::URLRequestThrottlerEntryInterface* entry;

  // Set to true if the entry gets erased. Helpful to verify that entries
  // with 0 refcount (since we don't take a refcount above) have been
  // erased from the map.
  bool was_erased;
};

}  // namespace

namespace net {

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

  // TODO(joi): Remove after crbug.com/71721 is fixed.
  base::strlcpy(magic_buffer_1_, "MAGICZZ", arraysize(magic_buffer_1_));
  base::strlcpy(magic_buffer_2_, "GOOGYZZ", arraysize(magic_buffer_2_));
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
  // TODO(joi): Remove "GOOGY/MONSTA" stuff once crbug.com/71721 is done
  return StringPrintf("GOOGY%sMONSTA", StringToLowerASCII(id.spec()).c_str());
}

void URLRequestThrottlerManager::GarbageCollectEntriesIfNecessary() {
  requests_since_last_gc_++;
  if (requests_since_last_gc_ < kRequestsBetweenCollecting)
    return;
  requests_since_last_gc_ = 0;

  GarbageCollectEntries();
}

void URLRequestThrottlerManager::GarbageCollectEntries() {
  // TODO(joi): Remove these crash report aids once crbug.com/71721
  // is figured out.
  IteratorHistory history[32] = { { 0 } };
  size_t history_ix = 0;
  history[history_ix++].self = this;

  int nulls_found = 0;
  UrlEntryMap::iterator i = url_entries_.begin();
  while (i != url_entries_.end()) {
    if (i->second == NULL) {
      ++nulls_found;
    }

    // Keep a log of the first 31 items accessed after the first
    // NULL encountered (hypothesis is there are multiple NULLs,
    // and we may learn more about pattern of memory overwrite).
    // We also log when we access the first entry, to get an original
    // value for our this pointer.
    if (nulls_found > 0 && history_ix < arraysize(history)) {
      history[history_ix].self = this;
      base::strlcpy(history[history_ix].url, i->first.c_str(),
                    arraysize(history[history_ix].url));
      history[history_ix].entry = i->second.get();
      history[history_ix].was_erased = false;
      ++history_ix;
    }

    // TODO(joi): Remove first i->second check when crbug.com/71721 is fixed.
    if (i->second == NULL || (i->second)->IsEntryOutdated()) {
      url_entries_.erase(i++);

      if (nulls_found > 0 && (history_ix - 1) < arraysize(history)) {
        history[history_ix - 1].was_erased = true;
      }
    } else {
      ++i;
    }
  }

  // TODO(joi): Make this a CHECK again after M11 branch point.
  DCHECK(nulls_found == 0);

  // In case something broke we want to make sure not to grow indefinitely.
  while (url_entries_.size() > kMaximumNumberOfEntries) {
    url_entries_.erase(url_entries_.begin());
  }
}

}  // namespace net
