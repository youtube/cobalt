// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_THROTTLER_MANAGER_H_
#define NET_URL_REQUEST_URL_REQUEST_THROTTLER_MANAGER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/synchronization/lock.h"  // ThreadCheckerForRelease
#include "base/threading/platform_thread.h"  // ThreadCheckerForRelease
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_throttler_entry.h"

namespace net {

// TODO(joi): Delete this temporary copy of base::ThreadChecker (needed to
// enable it in release builds) and go back to simply inheriting from
// NonThreadSafe once crbug.com/71721 has been tracked down.
class ThreadCheckerForRelease {
 public:
  ThreadCheckerForRelease();
  ~ThreadCheckerForRelease();

  bool CalledOnValidThread() const;

  // Changes the thread that is checked for in CalledOnValidThread.  This may
  // be useful when an object may be created on one thread and then used
  // exclusively on another thread.
  void DetachFromThread();

 private:
  void EnsureThreadIdAssigned() const;

  mutable base::Lock lock_;
  // This is mutable so that CalledOnValidThread can set it.
  // It's guarded by |lock_|.
  mutable base::PlatformThreadId valid_thread_id_;
};

// Class that registers URL request throttler entries for URLs being accessed
// in order to supervise traffic. URL requests for HTTP contents should
// register their URLs in this manager on each request.
//
// URLRequestThrottlerManager maintains a map of URL IDs to URL request
// throttler entries. It creates URL request throttler entries when new URLs
// are registered, and does garbage collection from time to time in order to
// clean out outdated entries. URL ID consists of lowercased scheme, host, port
// and path. All URLs converted to the same ID will share the same entry.
//
// NOTE: All usage of the singleton object of this class should be on the same
// thread.
//
// TODO(joi): Switch back to NonThreadSafe (and remove checks in release builds)
// once crbug.com/71721 has been tracked down.
class URLRequestThrottlerManager {
 public:
  static URLRequestThrottlerManager* GetInstance();

  // Must be called for every request, returns the URL request throttler entry
  // associated with the URL. The caller must inform this entry of some events.
  // Please refer to url_request_throttler_entry_interface.h for further
  // informations.
  scoped_refptr<URLRequestThrottlerEntryInterface> RegisterRequestUrl(
      const GURL& url);

  // Registers a new entry in this service and overrides the existing entry (if
  // any) for the URL. The service will hold a reference to the entry.
  // It is only used by unit tests.
  void OverrideEntryForTests(const GURL& url, URLRequestThrottlerEntry* entry);

  // Explicitly erases an entry.
  // This is useful to remove those entries which have got infinite lifetime and
  // thus won't be garbage collected.
  // It is only used by unit tests.
  void EraseEntryForTests(const GURL& url);

  void InitializeOptions(bool enforce_throttling);

  bool enforce_throttling() const { return enforce_throttling_; }

 protected:
  URLRequestThrottlerManager();
  ~URLRequestThrottlerManager();

  // Method that allows us to transform a URL into an ID that can be used in our
  // map. Resulting IDs will be lowercase and consist of the scheme, host, port
  // and path (without query string, fragment, etc.).
  // If the URL is invalid, the invalid spec will be returned, without any
  // transformation.
  std::string GetIdFromUrl(const GURL& url) const;

  // Method that ensures the map gets cleaned from time to time. The period at
  // which garbage collecting happens is adjustable with the
  // kRequestBetweenCollecting constant.
  void GarbageCollectEntriesIfNecessary();

  // Method that does the actual work of garbage collecting.
  void GarbageCollectEntries();

  // Used by tests.
  int GetNumberOfEntriesForTests() const { return url_entries_.size(); }

 private:
  friend struct DefaultSingletonTraits<URLRequestThrottlerManager>;

  // From each URL we generate an ID composed of the scheme, host, port and path
  // that allows us to uniquely map an entry to it.
  typedef std::map<std::string, scoped_refptr<URLRequestThrottlerEntry> >
      UrlEntryMap;

  // Maximum number of entries that we are willing to collect in our map.
  static const unsigned int kMaximumNumberOfEntries;
  // Number of requests that will be made between garbage collection.
  static const unsigned int kRequestsBetweenCollecting;

  // Constructor copies the string "MAGICZZ\0" into this buffer; using it
  // to try to detect memory overwrites affecting url_entries_ in the wild.
  // TODO(joi): Remove once crbug.com/71721 is figured out.
  char magic_buffer_1_[8];

  // Map that contains a list of URL ID and their matching
  // URLRequestThrottlerEntry.
  UrlEntryMap url_entries_;

  // Constructor copies the string "GOOGYZZ\0" into this buffer; using it
  // to try to detect memory overwrites affecting url_entries_ in the wild.
  // TODO(joi): Remove once crbug.com/71721 is figured out.
  char magic_buffer_2_[8];

  // This keeps track of how many requests have been made. Used with
  // GarbageCollectEntries.
  unsigned int requests_since_last_gc_;

  // Valid after construction.
  GURL::Replacements url_id_replacements_;

  // Whether we would like to reject outgoing HTTP requests during the back-off
  // period.
  bool enforce_throttling_;

  // Certain tests do not obey the net component's threading policy, so we
  // keep track of whether we're being used by tests, and turn off certain
  // checks.
  //
  // TODO(joi): See if we can fix the offending unit tests and remove this
  // workaround.
  bool being_tested_;

  ThreadCheckerForRelease thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestThrottlerManager);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_THROTTLER_MANAGER_H_
