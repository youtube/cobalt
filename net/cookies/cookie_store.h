// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by number 42.

#ifndef NET_COOKIES_COOKIE_STORE_H_
#define NET_COOKIES_COOKIE_STORE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "net/cookies/cookie_options.h"
#include "net/base/net_export.h"

class GURL;

namespace net {

class CookieMonster;

// An interface for storing and retrieving cookies. Implementations need to
// be thread safe as its methods can be accessed from IO as well as UI threads.
class NET_EXPORT CookieStore : public base::RefCountedThreadSafe<CookieStore> {
 public:
  // This struct contains additional consumer-specific information that might
  // be stored with cookies; currently just MAC information, see:
  // http://tools.ietf.org/html/draft-ietf-oauth-v2-http-mac
  struct NET_EXPORT CookieInfo {
    CookieInfo();
    ~CookieInfo();

    // The name of the cookie.
    std::string name;
    // TODO(abarth): Add value if any clients need it.

    // The time at which the cookie was created.
    base::Time creation_date;

    // The value of the MAC-Key and MAC-Algorithm attributes, if present.
    std::string mac_key;
    std::string mac_algorithm;
  };

  // Callback definitions.
  typedef base::Callback <void(
      const std::string& cookie_line,
      const std::vector<CookieInfo>& cookie_infos)> GetCookieInfoCallback;
  typedef base::Callback<void(const std::string& cookie)>
      GetCookiesCallback;
  typedef base::Callback<void(bool success)> SetCookiesCallback;
  typedef base::Callback<void(int num_deleted)> DeleteCallback;


  // Sets a single cookie.  Expects a cookie line, like "a=1; domain=b.com".
  //
  // Fails either if the cookie is invalid or if this is a non-HTTPONLY cookie
  // and it would overwrite an existing HTTPONLY cookie.
  // Returns true if the cookie is successfully set.
  virtual void SetCookieWithOptionsAsync(
      const GURL& url,
      const std::string& cookie_line,
      const CookieOptions& options,
      const SetCookiesCallback& callback) = 0;

  // TODO(???): what if the total size of all the cookies >4k, can we have a
  // header that big or do we need multiple Cookie: headers?
  // Note: Some sites, such as Facebook, occasionally use Cookie headers >4k.
  //
  // Simple interface, gets a cookie string "a=b; c=d" for the given URL.
  // Use options to access httponly cookies.
  virtual void GetCookiesWithOptionsAsync(
      const GURL& url, const CookieOptions& options,
      const GetCookiesCallback& callback) = 0;

  // This function is similar to GetCookiesWithOptions same functionality as
  // GetCookiesWithOptions except that it additionally provides detailed
  // information about the cookie contained in the cookie line.  See |struct
  // CookieInfo| above for details.
  virtual void GetCookiesWithInfoAsync(
      const GURL& url,
      const CookieOptions& options,
      const GetCookieInfoCallback& callback) = 0;

  // Deletes the passed in cookie for the specified URL.
  virtual void DeleteCookieAsync(const GURL& url,
                                 const std::string& cookie_name,
                                 const base::Closure& callback) = 0;

  // Deletes all of the cookies that have a creation_date greater than or equal
  // to |delete_begin| and less than |delete_end|
  // Returns the number of cookies that have been deleted.
  virtual void DeleteAllCreatedBetweenAsync(const base::Time& delete_begin,
                                            const base::Time& delete_end,
                                            const DeleteCallback& callback) = 0;

  virtual void DeleteSessionCookiesAsync(const DeleteCallback&) = 0;

  // Returns the underlying CookieMonster.
  virtual CookieMonster* GetCookieMonster() = 0;

 protected:
  friend class base::RefCountedThreadSafe<CookieStore>;
  CookieStore();
  virtual ~CookieStore();
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_STORE_H_
