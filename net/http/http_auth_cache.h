// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_CACHE_H_
#define NET_HTTP_HTTP_AUTH_CACHE_H_

#include <list>
#include <string>

#include "base/ref_counted.h"
#include "googleurl/src/gurl.h"
// This is needed for the FRIEND_TEST() macro.
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace net {

// HttpAuthCache stores HTTP authentication identities and challenge info.
// For each (origin, realm, scheme) triple the cache stores a
// HttpAuthCache::Entry, which holds:
//   - the origin server {protocol scheme, host, port}
//   - the last identity used (username/password)
//   - the last auth handler used (contains realm and authentication scheme)
//   - the list of paths which used this realm
// Entries can be looked up by either (origin, realm, scheme) or (origin, path).
class HttpAuthCache {
 public:
  class Entry;

  // Find the realm entry on server |origin| for realm |realm| and
  // scheme |scheme|.
  //   |origin| - the {scheme, host, port} of the server.
  //   |realm|  - case sensitive realm string.
  //   |scheme| - case sensitive authentication scheme, should be lower-case.
  //   returns  - the matched entry or NULL.
  Entry* Lookup(const GURL& origin, const std::string& realm,
                const std::string& scheme);

  // Find the entry on server |origin| whose protection space includes
  // |path|. This uses the assumption in RFC 2617 section 2 that deeper
  // paths lie in the same protection space.
  //   |origin| - the {scheme, host, port} of the server.
  //   |path|   - absolute path of the resource, or empty string in case of
  //              proxy auth (which does not use the concept of paths).
  //   returns  - the matched entry or NULL.
  Entry* LookupByPath(const GURL& origin, const std::string& path);

  // Add an entry on server |origin| for realm |handler->realm()| and
  // scheme |handler->scheme()|.  If an entry for this (realm,scheme)
  // already exists, update it rather than replace it -- this  preserves the
  // paths list.
  //   |origin|   - the {scheme, host, port} of the server.
  //   |realm|    - the auth realm for the challenge.
  //   |scheme|   - the authentication scheme for the challenge.
  //   |username| - login information for the realm.
  //   |password| - login information for the realm.
  //   |path|     - absolute path for a resource contained in the protection
  //                space; this will be added to the list of known paths.
  //   returns    - the entry that was just added/updated.
  Entry* Add(const GURL& origin,
             const std::string& realm,
             const std::string& scheme,
             const std::string& auth_challenge,
             const std::wstring& username,
             const std::wstring& password,
             const std::string& path);

  // Remove entry on server |origin| for realm |realm| and scheme |scheme|
  // if one exists AND if the cached identity matches (|username|, |password|).
  //   |origin|   - the {scheme, host, port} of the server.
  //   |realm|    - case sensitive realm string.
  //   |scheme|   - authentication scheme
  //   |username| - condition to match.
  //   |password| - condition to match.
  //   returns    - true if an entry was removed.
  bool Remove(const GURL& origin,
              const std::string& realm,
              const std::string& scheme,
              const std::wstring& username,
              const std::wstring& password);

  // Prevent unbounded memory growth. These are safeguards for abuse; it is
  // not expected that the limits will be reached in ordinary usage.
  // This also defines the worst-case lookup times (which grow linearly
  // with number of elements in the cache).
  enum { kMaxNumPathsPerRealmEntry = 10 };
  enum { kMaxNumRealmEntries = 10 };

 private:
  typedef std::list<Entry> EntryList;
  EntryList entries_;
};

// An authentication realm entry.
class HttpAuthCache::Entry {
 public:
  const GURL& origin() const {
    return origin_;
  }

  // The case-sensitive realm string of the challenge.
  const std::string realm() const {
    return realm_;
  }

  // The authentication scheme string of the challenge
  const std::string scheme() const {
    return scheme_;
  }

  // The authentication challenge.
  const std::string auth_challenge() const {
    return auth_challenge_;
  }

  // The login username.
  const std::wstring username() const {
    return username_;
  }

  // The login password.
  const std::wstring password() const {
    return password_;
  }

  int IncrementNonceCount() {
    return ++nonce_count_;
  }

 private:
  friend class HttpAuthCache;
  FRIEND_TEST(HttpAuthCacheTest, AddPath);
  FRIEND_TEST(HttpAuthCacheTest, AddToExistingEntry);

  Entry() {}

  // Adds a path defining the realm's protection space. If the path is
  // already contained in the protection space, is a no-op.
  void AddPath(const std::string& path);

  // Returns true if |dir| is contained within the realm's protection space.
  bool HasEnclosingPath(const std::string& dir);

  // |origin_| contains the {scheme, host, port} of the server.
  GURL origin_;
  std::string realm_;
  std::string scheme_;

  // Identity.
  std::string auth_challenge_;
  std::wstring username_;
  std::wstring password_;

  int nonce_count_;

  // List of paths that define the realm's protection space.
  typedef std::list<std::string> PathList;
  PathList paths_;
};

} // namespace net

#endif  // NET_HTTP_HTTP_AUTH_CACHE_H_
