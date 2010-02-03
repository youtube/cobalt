// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_STATIC_COOKIE_POLICY_H_
#define NET_BASE_STATIC_COOKIE_POLICY_H_

#include "base/basictypes.h"
#include "net/base/cookie_policy.h"

class GURL;

namespace net {

// The StaticCookiePolicy class implements a static cookie policy that supports
// three modes: allow all, deny all, or block third-party cookies.
//
// NOTE: This CookiePolicy implementation always completes synchronously.  The
// callback parameter will be ignored if specified.  Just pass NULL.
//
class StaticCookiePolicy : public CookiePolicy {
 public:
  enum Type {
    ALLOW_ALL_COOKIES = 0,      // Do not perform any cookie blocking.
    BLOCK_THIRD_PARTY_COOKIES,  // Prevent third-party cookies from being set.
    BLOCK_ALL_COOKIES           // Disable cookies.
  };

  StaticCookiePolicy()
      : type_(StaticCookiePolicy::ALLOW_ALL_COOKIES) {
  }

  explicit StaticCookiePolicy(Type type)
      : type_(type) {
  }

  // Sets the current policy to enforce. This should be called when the user's
  // preferences change.
  void set_type(Type type) { type_ = type; }
  Type type() const { return type_; }

  // CookiePolicy methods:

  // Consults the user's third-party cookie blocking preferences to determine
  // whether the URL's cookies can be read.
  virtual int CanGetCookies(const GURL& url,
                            const GURL& first_party_for_cookies,
                            CompletionCallback* callback);

  // Consults the user's third-party cookie blocking preferences to determine
  // whether the URL's cookies can be set.
  virtual int CanSetCookie(const GURL& url,
                           const GURL& first_party_for_cookies,
                           const std::string& cookie_line,
                           CompletionCallback* callback);

 private:
  Type type_;

  DISALLOW_COPY_AND_ASSIGN(StaticCookiePolicy);
};

}  // namespace net

#endif  // NET_BASE_STATIC_COOKIE_POLICY_H_
