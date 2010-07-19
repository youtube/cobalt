// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_URL_SECURITY_MANAGER_H_
#define NET_HTTP_URL_SECURITY_MANAGER_H_

#include "base/scoped_ptr.h"
#include "base/basictypes.h"

class GURL;

namespace net {

class HttpAuthFilter;

// The URL security manager controls the policies (allow, deny, prompt user)
// regarding URL actions (e.g., sending the default credentials to a server).
class URLSecurityManager {
 public:
  URLSecurityManager() {}
  virtual ~URLSecurityManager() {}

  // Creates a platform-dependent instance of URLSecurityManager.
  // The URLSecurityManager takes ownership of the HttpAuthFilter.
  static URLSecurityManager* Create(HttpAuthFilter* whitelist);

  // Returns true if we can send the default credentials to the server at
  // |auth_origin| for HTTP NTLM or Negotiate authentication.
  virtual bool CanUseDefaultCredentials(const GURL& auth_origin) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(URLSecurityManager);
};

class URLSecurityManagerWhitelist : public URLSecurityManager {
 public:
  // The URLSecurityManagerWhitelist takes ownership of the HttpAuthFilter.
  explicit URLSecurityManagerWhitelist(HttpAuthFilter* whitelist);

  // URLSecurityManager methods.
  virtual bool CanUseDefaultCredentials(const GURL& auth_origin);

 private:
  scoped_ptr<HttpAuthFilter> whitelist_;

  DISALLOW_COPY_AND_ASSIGN(URLSecurityManagerWhitelist);
};

#if defined(UNIT_TEST)
// An URLSecurityManager which always allows default credentials.
class URLSecurityManagerAllow : public URLSecurityManager {
 public:
  URLSecurityManagerAllow() {}
  virtual ~URLSecurityManagerAllow() {}

  virtual bool CanUseDefaultCredentials(const GURL& auth_origin) {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(URLSecurityManagerAllow);
};
#endif  // defined(UNIT_TEST)

}  // namespace net

#endif  // NET_HTTP_URL_SECURITY_MANAGER_H_
