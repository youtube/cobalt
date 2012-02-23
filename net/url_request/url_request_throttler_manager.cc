// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_throttler_manager.h"

#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "net/base/net_log.h"
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

  if (registered_from_thread_ == base::kInvalidThreadId) {
    // We can't currently do this in the constructor as it is run on the
    // UI thread and notifications go to the thread from which they are
    // registered.
    // TODO(joi): Clean this up once this is no longer a Singleton.
    NetworkChangeNotifier::AddIPAddressObserver(this);
    NetworkChangeNotifier::AddOnlineStateObserver(this);
    registered_from_thread_ = base::PlatformThread::CurrentId();
  }

  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);

  // Periodically garbage collect old entries.
  GarbageCollectEntriesIfNecessary();

  // Find the entry in the map or create a new NULL entry.
  scoped_refptr<URLRequestThrottlerEntry>& entry = url_entries_[url_id];

  // If the entry exists but could be garbage collected at this point, we
  // start with a fresh entry so that we possibly back off a bit less
  // aggressively (i.e. this resets the error count when the entry's URL
  // hasn't been requested in long enough).
  if (entry.get() && entry->IsEntryOutdated()) {
    entry = NULL;
  }

  // Create the entry if needed.
  if (entry.get() == NULL) {
    entry = new URLRequestThrottlerEntry(this, url_id);

    // We only disable back-off throttling on an entry that we have
    // just constructed.  This is to allow unit tests to explicitly override
    // the entry for localhost URLs.  Given that we do not attempt to
    // disable throttling for entries already handed out (see comment
    // in AddToOptOutList), this is not a problem.
    std::string host = url.host();
    if (opt_out_hosts_.find(host) != opt_out_hosts_.end() ||
        IsLocalhost(host)) {
      if (!logged_for_localhost_disabled_ && IsLocalhost(host)) {
        logged_for_localhost_disabled_ = true;
        net_log_->AddEvent(
            NetLog::TYPE_THROTTLING_DISABLED_FOR_HOST,
            make_scoped_refptr(new NetLogStringParameter("host", host)));
      }

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
  if (opt_out_hosts_.find(host) == opt_out_hosts_.end()) {
    UMA_HISTOGRAM_COUNTS("Throttling.SiteOptedOut", 1);

    net_log_->EndEvent(
        NetLog::TYPE_THROTTLING_DISABLED_FOR_HOST,
        make_scoped_refptr(new NetLogStringParameter("host", host)));
    opt_out_hosts_.insert(host);
  }
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

void URLRequestThrottlerManager::set_net_log(NetLog* net_log) {
  DCHECK(net_log);
  NetLog::Source source(NetLog::SOURCE_EXPONENTIAL_BACKOFF_THROTTLING,
                        net_log->NextID());
  net_log_.reset(new BoundNetLog(source, net_log));
}

NetLog* URLRequestThrottlerManager::net_log() const {
  return net_log_->net_log();
}

void URLRequestThrottlerManager::OnIPAddressChanged() {
  OnNetworkChange();
}

void URLRequestThrottlerManager::OnOnlineStateChanged(bool online) {
  OnNetworkChange();
}

// TODO(joi): Turn throttling on by default when appropriate.
URLRequestThrottlerManager::URLRequestThrottlerManager()
    : requests_since_last_gc_(0),
      enforce_throttling_(false),
      enable_thread_checks_(false),
      logged_for_localhost_disabled_(false),
      registered_from_thread_(base::kInvalidThreadId) {
  // Construction/destruction is on main thread (because BrowserMain
  // retrieves an instance to call InitializeOptions), but is from then on
  // used on I/O thread.
  DetachFromThread();

  url_id_replacements_.ClearPassword();
  url_id_replacements_.ClearUsername();
  url_id_replacements_.ClearQuery();
  url_id_replacements_.ClearRef();

  // Make sure there is always a net_log_ instance, even if it logs to
  // nowhere.
  net_log_.reset(new BoundNetLog());
}

URLRequestThrottlerManager::~URLRequestThrottlerManager() {
  // Destruction is on main thread (AtExit), but real use is on I/O thread.
  // The AtExit manager does not run until the I/O thread has finished
  // processing.
  DetachFromThread();

  // We must currently skip this in the production case, where the destructor
  // does not run on the thread we registered from.
  // TODO(joi): Fix once we are no longer a Singleton.
  if (base::PlatformThread::CurrentId() == registered_from_thread_) {
    NetworkChangeNotifier::RemoveIPAddressObserver(this);
    NetworkChangeNotifier::RemoveOnlineStateObserver(this);
  }

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

void URLRequestThrottlerManager::OnNetworkChange() {
  // Remove all entries.  Any entries that in-flight requests have a reference
  // to will live until those requests end, and these entries may be
  // inconsistent with new entries for the same URLs, but since what we
  // want is a clean slate for the new connection state, this is OK.
  url_entries_.clear();
  requests_since_last_gc_ = 0;
}

}  // namespace net
