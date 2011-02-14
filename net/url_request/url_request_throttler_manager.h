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
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_throttler_entry.h"

namespace net {

// Class that registers URL request throttler entries for URLs being accessed in
// order to supervise traffic. URL requests for HTTP contents should register
// their URLs in this manager on each request.
// URLRequestThrottlerManager maintains a map of URL IDs to URL request
// throttler entries. It creates URL request throttler entries when new URLs are
// registered, and does garbage collection from time to time in order to clean
// out outdated entries. URL ID consists of lowercased scheme, host, port and
// path. All URLs converted to the same ID will share the same entry.
//
// NOTE: All usage of the singleton object of this class should be on the same
// thread.
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

  // Map that contains a list of URL ID and their matching
  // URLRequestThrottlerEntry.
  UrlEntryMap url_entries_;

  // This keeps track of how many requests have been made. Used with
  // GarbageCollectEntries.
  unsigned int requests_since_last_gc_;

  mutable scoped_ptr<GURL::Replacements> url_id_replacements_;

  // Whether we would like to reject outgoing HTTP requests during the back-off
  // period.
  bool enforce_throttling_;

  // Whether to record threads that have accessed the URLRequestThrottlerManager
  // singleton object.
  // TODO(yzshen): It is used for diagnostic purpose and should be removed once
  // we figure out crbug.com/71721
  bool record_access_log_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestThrottlerManager);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_THROTTLER_MANAGER_H_
